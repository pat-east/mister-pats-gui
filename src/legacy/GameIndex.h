#pragma once

#include <map>
#include <string>
#include <vector>

// Reads ConsoleMode's library cache instead of walking the filesystem.
//
// Why this exists: traversing a large library cold is what drops a marginal USB drive off the
// bus — one sequential read of a cache file on the SD card does not. ConsoleMode survives
// precisely because it works this way, and reusing its file also keeps both frontends in sync.
//
// Format, tab separated, one game per line:
//   <SYSTEM>\t<file name>\t<path relative to the volume>\t<flag>
class GameIndex {
public:
    bool load(const std::string &cacheDir = kDefaultCacheDir);

    bool available() const { return !bySystem_.empty(); }
    size_t size() const { return total_; }
    const std::string &source() const { return source_; }

    // Absolute ROM paths for a system name, empty when the index does not know it.
    const std::vector<std::string> *pathsFor(const std::string &systemName) const;

    static constexpr const char *kDefaultCacheDir = "/media/fat/ConsoleMode/caches";

private:
    std::map<std::string, std::vector<std::string>> bySystem_;
    size_t total_ = 0;
    std::string source_;
};
