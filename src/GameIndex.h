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

    // The cache stores volume-relative paths, so they need a root prefixed back on.
    //
    // This assumes a single attached USB volume. Console Mode writes one cache file per
    // volume (`usb_<volume-id>.txt`) and every one of them is resolved against this same
    // root — so with two volumes attached, the one that mounts as /media/usb1 would get
    // paths pointing at /media/usb0 and its games would fail to launch. Supporting several
    // volumes means mapping each cache file to its own mount point.
    static constexpr const char *kUsbRoot = "/media/usb0";

private:
    bool loadFile(const std::string &file, const std::string &root);

    std::map<std::string, std::vector<std::string>> bySystem_;
    size_t total_ = 0;
    std::string source_;
};
