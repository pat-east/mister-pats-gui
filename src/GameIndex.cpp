#include "GameIndex.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>

#include "GameDatabase.h"

namespace {

bool hasPrefixAndSuffix(const std::string &name, const char *prefix, const char *suffix) {
    const size_t p = std::strlen(prefix);
    const size_t s = std::strlen(suffix);
    return name.size() > p + s && name.compare(0, p, prefix) == 0 &&
           name.compare(name.size() - s, s, suffix) == 0;
}

bool fileExists(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

struct Entry {
    std::string system;
    std::string relative;   // path relative to whichever volume this cache file describes
};

std::vector<Entry> parseFile(const std::string &file) {
    std::vector<Entry> entries;
    std::ifstream in(file);
    if (!in) return entries;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        const size_t first = line.find('\t');
        if (first == std::string::npos) continue;
        const size_t second = line.find('\t', first + 1);
        if (second == std::string::npos) continue;

        const size_t third = line.find('\t', second + 1);
        const std::string system = line.substr(0, first);
        const std::string relative =
            line.substr(second + 1, (third == std::string::npos) ? std::string::npos
                                                                 : third - second - 1);
        if (system.empty() || relative.empty()) continue;

        entries.push_back({system, relative});
    }
    return entries;
}

// ConsoleMode names each cache file after the volume's own filesystem serial or USB signature
// (`usb_60B7-BD53.txt`, `usb_sig00000000....txt`) — nothing in that name says which
// /media/usbN it belongs to, and MiSTer does not promise that numbering stays the same
// between boots anyway. So a file is matched to a mount point the same way GameDatabase
// recognises a moved root: by checking that a spread sample of the paths it records are
// actually there. Trusting the file name here is exactly the assumption that made box art
// point at the wrong drive earlier — this does not repeat it.
std::string resolveRoot(const std::vector<Entry> &entries,
                        const std::vector<std::string> &candidates) {
    if (entries.empty() || candidates.empty()) return {};

    constexpr size_t kSamples = 8;
    const size_t step = std::max<size_t>(1, entries.size() / kSamples);

    std::string best;
    size_t bestHits = 0;
    for (const std::string &candidate : candidates) {
        size_t hits = 0;
        for (size_t i = 0; i < entries.size(); i += step)
            if (fileExists(candidate + "/" + entries[i].relative)) ++hits;
        if (hits > bestHits) {
            bestHits = hits;
            best = candidate;
        }
    }
    return best;
}

} // namespace

bool GameIndex::load(const std::string &cacheDir) {
    bySystem_.clear();
    total_ = 0;
    source_.clear();

    DIR *dir = opendir(cacheDir.c_str());
    if (!dir) {
        std::printf("index: no cache directory at %s\n", cacheDir.c_str());
        return false;
    }

    std::vector<std::string> files;
    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (hasPrefixAndSuffix(name, "usb_", ".txt")) files.push_back(cacheDir + "/" + name);
    }
    closedir(dir);

    // Cache files are ConsoleMode's own record of USB volumes specifically (the "usb_" name),
    // so /media/fat is never a candidate root for one — narrowing this also keeps a game
    // that happens to share a relative path with something on the SD card from being
    // mismatched onto it.
    std::vector<std::string> candidates;
    for (std::string &point : GameDatabase::mountPoints())
        if (point != "/media/fat") candidates.push_back(std::move(point));

    for (const std::string &file : files) {
        const std::vector<Entry> entries = parseFile(file);
        if (entries.empty()) continue;

        const std::string root = resolveRoot(entries, candidates);
        if (root.empty()) {
            std::printf("index: %s matches no attached volume, skipped\n", file.c_str());
            continue;
        }

        for (const Entry &entry : entries) bySystem_[entry.system].push_back(root + "/" + entry.relative);
        total_ += entries.size();
        if (!source_.empty()) source_ += ", ";
        source_ += file;
    }

    if (available())
        std::printf("index: %zu games in %zu systems from the ConsoleMode cache\n", total_,
                    bySystem_.size());
    else
        std::printf("index: no usable cache, falling back to scanning directories\n");

    return available();
}

const std::vector<std::string> *GameIndex::pathsFor(const std::string &systemName) const {
    const auto it = bySystem_.find(systemName);
    return (it == bySystem_.end() || it->second.empty()) ? nullptr : &it->second;
}
