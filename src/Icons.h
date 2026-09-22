#pragma once

#include "Paths.h"

#include <set>
#include <string>

// Maps a system name to a console icon. The names in ConsoleMode's section files are
// descriptive ("NINTENDO 64", "FAMICOM DISK SYSTEM") while the icon set uses short platform
// codes ("N64", "FDS"), so an alias table bridges the two.
class Icons {
public:
    bool load(const std::string &directory = kDefaultDirectory);

    // Never empty while any icon is available: systems without a matching icon get
    // the fallback, so a row of tiles keeps one consistent shape.
    std::string pathFor(const std::string &systemName) const;

    size_t available() const { return available_.size(); }

    static constexpr const char *kDefaultDirectory = MISTER_PAT_ROOT "/icons";

    // Stand-in for systems the icon set does not cover.
    static constexpr const char *kFallbackIcon = "C64";

private:
    std::string directory_;
    std::set<std::string> available_;
};
