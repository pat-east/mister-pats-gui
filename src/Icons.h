#pragma once

#include "Paths.h"

#include <set>
#include <string>
#include <vector>

// Maps a system name to a console icon. The names in ConsoleMode's section files are
// descriptive ("NINTENDO 64", "FAMICOM DISK SYSTEM") while the icon set uses short platform
// codes ("N64", "FDS"), so an alias table bridges the two.
class Icons {
public:
    bool load(const std::string &directory = kDefaultDirectory);

    // Never empty while any icon is available: systems without a matching icon get
    // the fallback, so a row of tiles keeps one consistent shape.
    std::string pathFor(const std::string &systemName) const;

    // Every installed icon asset, once each, in deterministic order.
    std::vector<std::string> paths() const;

    // Repository icon names still missing a local BMP. PNG-only installs are included so
    // older versions can download the faster files while continuing to work during download.
    std::vector<std::string> missingBitmaps() const;

    bool refresh() { return load(directory_.empty() ? kDefaultDirectory : directory_); }

    size_t available() const {
        size_t count = bitmaps_.size();
        for (const std::string &name : pngs_)
            if (!bitmaps_.count(name)) ++count;
        return count;
    }

    static constexpr const char *kDefaultDirectory = MISTER_PAT_ROOT "/icons";

    // Stand-in for systems the icon set does not cover.
    static constexpr const char *kFallbackIcon = "C64";

private:
    std::string directory_;
    std::set<std::string> bitmaps_;
    std::set<std::string> pngs_;
};
