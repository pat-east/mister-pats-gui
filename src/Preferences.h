#pragma once

#include <string>
#include <vector>

#include "Paths.h"

enum class TabDisplay { Names, Icons, Both };

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

    bool showArcadeInConsoles() const { return showArcadeInConsoles_; }
    void setShowArcadeInConsoles(bool show);
    bool organizeGameVersions() const { return organizeGameVersions_; }
    void setOrganizeGameVersions(bool enabled);

    TabDisplay tabDisplay() const { return tabDisplay_; }
    void setTabDisplay(TabDisplay display);

    bool showBoxArt() const { return showBoxArt_; }
    void setShowBoxArt(bool show);

    bool loadSmallBoxartsInGridView() const { return loadSmallBoxartsInGridView_; }
    void setLoadSmallBoxartsInGridView(bool enabled);

    bool startSoundEnabled() const { return startSoundEnabled_; }
    void setStartSoundEnabled(bool enabled);

    bool menuSoundsEnabled() const { return menuSoundsEnabled_; }
    void setMenuSoundsEnabled(bool enabled);

    bool useSpaceGrotesk() const { return useSpaceGrotesk_; }
    void setUseSpaceGrotesk(bool use);

    int fontSizeScale() const { return fontSizeScale_; }
    void setFontSizeScale(int scale);

    bool highContrast() const { return highContrast_; }
    void setHighContrast(bool enabled);

    // A shared game view (grid/list/small) or "individual" for per-tab views.
    const std::string &defaultView() const { return defaultView_; }
    void setDefaultView(const std::string &view);
    const std::string &homeView() const { return homeView_; }
    const std::string &favoritesView() const { return favoritesView_; }
    const std::string &consolesView() const { return consolesView_; }
    const std::string &gamesView() const { return gamesView_; }
    void setHomeView(const std::string &view);
    void setFavoritesView(const std::string &view);
    void setConsolesView(const std::string &view);
    void setGamesView(const std::string &view);

    const std::string &faceButtonStyle() const { return faceButtonStyle_; }
    void setFaceButtonStyle(const std::string &style);

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
    bool showArcadeInConsoles_ = false;
    bool organizeGameVersions_ = true;
    TabDisplay tabDisplay_ = TabDisplay::Names;
    bool showBoxArt_ = true;
    bool loadSmallBoxartsInGridView_ = false;
    bool startSoundEnabled_ = true;
    bool menuSoundsEnabled_ = true;
    bool useSpaceGrotesk_ = true;
    int fontSizeScale_ = 100;
    bool highContrast_ = false;
    std::string defaultView_ = "grid";
    std::string homeView_ = "grid";
    std::string favoritesView_ = "grid";
    std::string consolesView_ = "grid";
    std::string gamesView_ = "grid";
    std::string faceButtonStyle_ = "xbox";
    bool checkForUpdates_ = false;
    bool use24HourClock_ = true;
    std::string timeZone_; // empty follows MiSTer's system time zone
    std::string file_ = kDefaultFile;
};
