#pragma once

#include <string>
#include <vector>

#include "Paths.h"

// A handful of on/off toggles for parts of the interface someone might not want to see.
// One flat file, one line per value that differs from its default, so a fresh install
// needs no file at all and adding a future toggle needs no migration.
class Preferences {
public:
    bool load(const std::string &file = kDefaultFile);

    bool showGamesTab() const { return showGamesTab_; }
    void setShowGamesTab(bool show);

    // The Arcade tab. On by default, but only ever shown when the library actually holds
    // Arcade games — see App — so this is the switch for someone who has them and does not
    // want the tab.
    bool showArcadeTab() const { return showArcadeTab_; }
    void setShowArcadeTab(bool show);

    bool showBoxArt() const { return showBoxArt_; }
    void setShowBoxArt(bool show);

    bool startSoundEnabled() const { return startSoundEnabled_; }
    void setStartSoundEnabled(bool enabled);

    bool menuSoundsEnabled() const { return menuSoundsEnabled_; }
    void setMenuSoundsEnabled(bool enabled);

    bool useSpaceGrotesk() const { return useSpaceGrotesk_; }
    void setUseSpaceGrotesk(bool use);

    // One of GamesScreen's --view names ("grid", "list", "small") — kept
    // as a plain string here rather than the GameView enum so this class does not need to
    // know about GamesScreen at all, consistent with everything else in it.
    const std::string &defaultView() const { return defaultView_; }
    void setDefaultView(const std::string &view);

    // Off by default — the only network access in this project that is not something the
    // user just asked for in the moment (the box art scraper is a button; this would run on
    // its own), so it opts in rather than out. See UpdateService.
    bool checkForUpdates() const { return checkForUpdates_; }
    void setCheckForUpdates(bool check);

    bool use24HourClock() const { return use24HourClock_; }
    void setUse24HourClock(bool value);

    const std::string &timeZone() const { return timeZone_; }
    bool setTimeZone(const std::string &zone);
    static std::vector<std::string> timeZoneRegions();
    static std::vector<std::string> timeZonesIn(const std::string &region);
    void applyTimeZone() const;

    static constexpr const char *kDefaultFile = MISTER_PAT_ROOT "/preferences.txt";

private:
    bool save() const;

    bool showGamesTab_ = true;
    bool showArcadeTab_ = true;
    bool showBoxArt_ = true;
    bool startSoundEnabled_ = true;
    bool menuSoundsEnabled_ = true;
    bool useSpaceGrotesk_ = true;
    std::string defaultView_ = "grid";
    bool checkForUpdates_ = false;
    bool use24HourClock_ = true;
    std::string timeZone_; // empty follows MiSTer's system time zone
    std::string file_ = kDefaultFile;
};
