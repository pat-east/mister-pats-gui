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

    // One of GamesScreen's --view names ("grid", "list", "large", "small", "compact") — kept
    // as a plain string here rather than the GameView enum so this class does not need to
    // know about GamesScreen at all, consistent with everything else in it.
    const std::string &defaultView() const { return defaultView_; }
    void setDefaultView(const std::string &view);

    // Off by default — the only network access in this project that is not something the
    // user just asked for in the moment (the box art scraper is a button; this would run on
    // its own), so it opts in rather than out. See UpdateCheck.
    bool checkForUpdates() const { return checkForUpdates_; }
    void setCheckForUpdates(bool check);

    static constexpr const char *kDefaultFile = MISTER_PAT_ROOT "/preferences.txt";

private:
    bool save() const;

    bool showGamesTab_ = true;
    std::string defaultView_ = "grid";
    bool checkForUpdates_ = false;
    std::string file_ = kDefaultFile;
};
