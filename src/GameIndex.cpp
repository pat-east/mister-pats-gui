#include "GameIndex.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>

namespace {

bool hasPrefixAndSuffix(const std::string &name, const char *prefix, const char *suffix) {
    const size_t p = std::strlen(prefix);
    const size_t s = std::strlen(suffix);
    return name.size() > p + s && name.compare(0, p, prefix) == 0 &&
           name.compare(name.size() - s, s, suffix) == 0;
}

} // namespace

bool GameIndex::loadFile(const std::string &file, const std::string &root) {
    std::ifstream in(file);
    if (!in) return false;

    size_t added = 0;
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

        bySystem_[system].push_back(root + "/" + relative);
        ++added;
    }

    if (added) {
        total_ += added;
        if (!source_.empty()) source_ += ", ";
        source_ += file;
    }

    return added > 0;
}

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

    for (const std::string &file : files) loadFile(file, kUsbRoot);

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
