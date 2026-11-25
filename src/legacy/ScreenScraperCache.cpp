#include "ScreenScraperCache.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <vector>

#include "ScreenScraperNames.h"

namespace {

constexpr const char *kHeader = "#mister-pat screenscraper 1";

bool makeDirectories(const std::string &path) {
    if (path.empty()) return false;
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        current += path[i];
        if (path[i] == '/' || i + 1 == path.size()) {
            if (current == "/") continue;
            if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) return false;
        }
    }
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

std::string statusName(ScreenScraperResult::Status status) {
    switch (status) {
    case ScreenScraperResult::Status::Video: return "video";
    case ScreenScraperResult::Status::NoVideo: return "novideo";
    case ScreenScraperResult::Status::NoGame: return "nogame";
    }
    return "nogame";
}

bool statusFromName(const std::string &name, ScreenScraperResult::Status &status) {
    if (name == "video") status = ScreenScraperResult::Status::Video;
    else if (name == "novideo") status = ScreenScraperResult::Status::NoVideo;
    else if (name == "nogame") status = ScreenScraperResult::Status::NoGame;
    else return false;
    return true;
}

std::vector<std::string> splitTabs(const std::string &line) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) { parts.push_back(line.substr(start)); break; }
        parts.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return parts;
}

bool safeVideoType(const std::string &type) {
    return type == "video" || type == "video-normalized";
}

}  // namespace

ScreenScraperCache::ScreenScraperCache(std::string directory) : directory_(std::move(directory)) {}

std::string ScreenScraperCache::resultsPath(const std::string &systemKey) const {
    return directory_ + "/results/" + ScreenScraperNames::fileSafe(systemKey) + ".tsv";
}

ScreenScraperCache::SystemResults &ScreenScraperCache::load(const std::string &systemKey) {
    const auto found = systems_.find(systemKey);
    if (found != systems_.end()) return found->second;
    SystemResults &results = systems_[systemKey];
    std::ifstream in(resultsPath(systemKey));
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> p = splitTabs(line);
        if (p.size() < 8) continue;
        ScreenScraperResult r;
        if (!statusFromName(p[1], r.status)) continue;
        r.gameId = std::atoll(p[2].c_str());
        r.videoType = p[3];
        r.size = std::atoll(p[4].c_str());
        r.crc = std::strtoul(p[5].c_str(), nullptr, 16);
        r.fetched = std::atoll(p[6].c_str());
        r.title = p[7];
        if (r.status == ScreenScraperResult::Status::Video &&
            (r.gameId <= 0 || !safeVideoType(r.videoType) || r.size <= 0))
            continue;   // a damaged line is skipped, not trusted
        results[p[0]] = r;   // the last line for a game wins
    }
    return results;
}

bool ScreenScraperCache::find(const std::string &systemKey, const std::string &titleKey,
                              ScreenScraperResult &out) {
    std::lock_guard<std::mutex> lock(mutex_);
    SystemResults &results = load(systemKey);
    const auto it = results.find(titleKey);
    if (it == results.end()) return false;
    out = it->second;
    return true;
}

bool ScreenScraperCache::store(const std::string &systemKey, const std::string &titleKey,
                               const ScreenScraperResult &result) {
    if (titleKey.empty() || titleKey.find_first_of("\t\r\n") != std::string::npos) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    SystemResults &results = load(systemKey);
    results[titleKey] = result;

    const std::string path = resultsPath(systemKey);
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos || !makeDirectories(path.substr(0, slash))) return false;
    struct stat info {};
    const bool isNew = stat(path.c_str(), &info) != 0;
    std::FILE *file = std::fopen(path.c_str(), "a");
    if (!file) return false;
    std::string title = result.title;
    for (char &c : title) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    if (isNew) std::fprintf(file, "%s\n", kHeader);
    std::fprintf(file, "%s\t%s\t%lld\t%s\t%lld\t%lx\t%lld\t%s\n", titleKey.c_str(),
                 statusName(result.status).c_str(), result.gameId, result.videoType.c_str(),
                 result.size, result.crc, result.fetched, title.c_str());
    const bool ok = std::fflush(file) == 0;
    std::fclose(file);
    return ok;
}

size_t ScreenScraperCache::clearResults() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t forgotten = 0;
    for (const auto &entry : systems_) forgotten += entry.second.size();
    systems_.clear();
    const std::string dir = directory_ + "/results";
    if (DIR *d = opendir(dir.c_str())) {
        while (const dirent *e = readdir(d)) {
            const std::string name = e->d_name;
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tsv") == 0)
                unlink((dir + "/" + name).c_str());
        }
        closedir(d);
    }
    return forgotten;
}

std::string ScreenScraperCache::videoPath(long long gameId, const std::string &type) const {
    return directory_ + "/videos/" + std::to_string(gameId) + "-" + type + ".mp4";
}

std::string ScreenScraperCache::partialPath(long long gameId, const std::string &type) const {
    return directory_ + "/videos/" + std::to_string(gameId) + "-" + type + ".part";
}

bool ScreenScraperCache::hasVideo(long long gameId, const std::string &type) const {
    if (gameId <= 0 || !safeVideoType(type)) return false;
    struct stat info {};
    return stat(videoPath(gameId, type).c_str(), &info) == 0 && S_ISREG(info.st_mode) &&
           info.st_size > 0;
}

namespace {
struct VideoFile {
    std::string path;
    long long size;
    long long modified;
};

// The finished videos this cache wrote: "<digits>-<type>.mp4".
std::vector<VideoFile> listVideos(const std::string &directory) {
    std::vector<VideoFile> files;
    const std::string dir = directory + "/videos";
    DIR *d = opendir(dir.c_str());
    if (!d) return files;
    while (const dirent *e = readdir(d)) {
        const std::string name = e->d_name;
        if (name.size() < 6 || name.compare(name.size() - 4, 4, ".mp4") != 0) continue;
        const size_t dash = name.find('-');
        if (dash == std::string::npos || dash == 0) continue;
        bool digits = true;
        for (size_t i = 0; i < dash; ++i) if (name[i] < '0' || name[i] > '9') digits = false;
        if (!digits) continue;
        const std::string type = name.substr(dash + 1, name.size() - dash - 5);
        if (!safeVideoType(type)) continue;
        struct stat info {};
        const std::string path = dir + "/" + name;
        if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) continue;
        files.push_back({path, static_cast<long long>(info.st_size),
                         static_cast<long long>(info.st_mtime)});
    }
    closedir(d);
    return files;
}
}  // namespace

long long ScreenScraperCache::videoBytes() const {
    long long total = 0;
    for (const VideoFile &f : listVideos(directory_)) total += f.size;
    return total;
}

std::vector<ScreenScraperCache::VideoInfo> ScreenScraperCache::videos() {
    std::vector<VideoInfo> out;
    const std::vector<VideoFile> files = listVideos(directory_);
    if (files.empty()) return out;
    // Which game and system a video id belongs to: the stored results of every system.
    std::map<std::string, std::pair<std::string, std::string>> known;   // "<id>-<type>" -> title, system
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string dir = directory_ + "/results";
        std::vector<std::string> stems;
        if (DIR *d = opendir(dir.c_str())) {
            while (const dirent *e = readdir(d)) {
                const std::string name = e->d_name;
                if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tsv") == 0) stems.push_back(name.substr(0, name.size() - 4));
            }
            closedir(d);
        }
        for (const std::string &stem : stems) {
            for (const auto &entry : load(stem)) {
                const ScreenScraperResult &r = entry.second;
                if (r.status != ScreenScraperResult::Status::Video) continue;
                known[std::to_string(r.gameId) + "-" + r.videoType] = {r.title, stem};
            }
        }
    }
    for (const VideoFile &f : files) {
        const size_t slash = f.path.find_last_of('/');
        const std::string name = f.path.substr(slash + 1, f.path.size() - slash - 5);   // "<id>-<type>"
        const size_t dash = name.find('-');
        VideoInfo v;
        v.gameId = std::atoll(name.substr(0, dash).c_str());
        v.type = name.substr(dash + 1);
        v.bytes = f.size;
        v.modified = f.modified;
        const auto it = known.find(name);
        if (it != known.end()) { v.title = it->second.first; v.systemKey = it->second.second; }
        out.push_back(v);
    }
    std::sort(out.begin(), out.end(), [](const VideoInfo &a, const VideoInfo &b) {
        if (a.title != b.title) return a.title < b.title;
        return a.gameId < b.gameId;
    });
    return out;
}

bool ScreenScraperCache::deleteVideo(long long gameId, const std::string &type) {
    if (gameId <= 0 || !safeVideoType(type)) return false;
    return unlink(videoPath(gameId, type).c_str()) == 0;
}

size_t ScreenScraperCache::deleteAllVideos() {
    size_t removed = 0;
    for (const VideoFile &f : listVideos(directory_))
        if (unlink(f.path.c_str()) == 0) ++removed;
    return removed;
}

bool ScreenScraperCache::makeRoom(long long incomingBytes, long long limitBytes) const {
    if (limitBytes <= 0) return true;
    if (incomingBytes > limitBytes) return false;
    std::vector<VideoFile> files = listVideos(directory_);
    long long total = 0;
    for (const VideoFile &f : files) total += f.size;
    std::sort(files.begin(), files.end(), [](const VideoFile &a, const VideoFile &b) {
        return a.modified < b.modified;   // oldest first
    });
    for (const VideoFile &f : files) {
        if (total + incomingBytes <= limitBytes) break;
        if (unlink(f.path.c_str()) == 0) total -= f.size;
    }
    return total + incomingBytes <= limitBytes;
}

long long ScreenScraperCache::freeBytes() const {
    std::string probe = directory_;
    for (;;) {   // the directory may not exist yet; ask its closest existing parent
        struct statvfs info {};
        if (statvfs(probe.c_str(), &info) == 0)
            return static_cast<long long>(info.f_bavail) * static_cast<long long>(info.f_frsize);
        const size_t slash = probe.find_last_of('/');
        if (slash == std::string::npos || slash == 0) return -1;
        probe.resize(slash);
    }
}
