#pragma once

#include <string>

#include "Paths.h"

// A handful of on/off toggles for parts of the interface someone might not want to see.
// One flat file, one line per value that differs from its default, so a fresh install
// needs no file at all and adding a future toggle needs no migration.
class Preferences {
public:
    bool load(const std::string &file = kDefaultFile);

    bool showGamesTab() const { return showGamesTab_; }
    void setShowGamesTab(bool show);

    static constexpr const char *kDefaultFile = MISTER_PAT_ROOT "/preferences.txt";

private:
    bool save() const;

    bool showGamesTab_ = true;
    std::string file_ = kDefaultFile;
};
