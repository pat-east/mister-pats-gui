#pragma once

// Sample Interface service: every preference in memory, a short list of systems for Manage Consoles, and a log of the
// sounds it was asked to play. `--set interface.<key>=<value>` puts it in any state (help() lists the keys).

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "app/settings/InterfaceService.h"
#include "app/settings/SampleControl.h"

namespace patsgui {

class SampleInterfaceService : public InterfaceService, public SampleControl {
public:
    bool gamesTab = true, arcadeTab = true, arcadeInConsoles = false, dark = true, boxArt = true, smallGrid = false,
         startSnd = true, menuSnd = true;
    bool hasArcade = true;            // the library has an Arcade system (Classic: Library::arcadeSystem() != nullptr)
    bool printSounds = false;         // --set interface.soundlog=1: every played sound goes to stdout
    std::string tab = "home", view = "grid";
    int display = 0, theme = 0;
    std::vector<SystemRow> systems;
    std::vector<UiSound> played;      // the sounds that were allowed by the preferences, in order

    SampleInterfaceService() { resize(12); }

    bool available() const override { return true; }
    void update(float) override {}
    bool showGamesTab() const override { return gamesTab; }
    void setShowGamesTab(bool on) override { gamesTab = on; }
    bool showArcadeTab() const override { return arcadeTab; }
    void setShowArcadeTab(bool on) override { arcadeTab = on; }
    bool gamesTabVisible() const override { return gamesTab; }
    bool arcadeTabVisible() const override { return arcadeTab && hasArcade; }
    bool showArcadeInConsoles() const override { return arcadeInConsoles; }
    void setShowArcadeInConsoles(bool on) override { arcadeInConsoles = on; }
    std::string startTab() const override { return tab; }
    void setStartTab(const std::string &t) override { tab = t; }
    int tabDisplay() const override { return display; }
    void setTabDisplay(int d) override { display = d; }
    int colorTheme() const override { return theme; }
    void setColorTheme(int i) override { theme = i; }
    bool darkMode() const override { return dark; }
    void setDarkMode(bool on) override { dark = on; }
    bool showBoxArt() const override { return boxArt; }
    void setShowBoxArt(bool on) override { boxArt = on; }
    bool loadSmallBoxartsInGridView() const override { return smallGrid; }
    void setLoadSmallBoxartsInGridView(bool on) override { smallGrid = on; }
    bool startSound() const override { return startSnd; }
    void setStartSound(bool on) override { startSnd = on; }
    bool menuSounds() const override { return menuSnd; }
    void setMenuSounds(bool on) override { menuSnd = on; }
    std::string gameView() const override { return view; }
    void setGameView(const std::string &v) override { view = v; }
    // Classic's list: every system with games, sorted case-insensitively by name.
    std::vector<SystemRow> systemsWithGames() const override {
        std::vector<SystemRow> out = systems;
        std::sort(out.begin(), out.end(), [](const SystemRow &a, const SystemRow &b) {
            return lower(a.name) < lower(b.name);
        });
        return out;
    }
    void setSystemHidden(const std::string &name, bool hidden) override {
        for (SystemRow &s : systems) if (s.name == name) s.hidden = hidden;
    }
    void playSound(UiSound sound) override {
        if (sound == UiSound::Start ? !startSnd : !menuSnd) return;   // Classic gates at the call site
        played.push_back(sound);
        if (printSounds) std::printf("sound: %s\n", soundName(sound));
    }

    static const char *soundName(UiSound s) {
        return s == UiSound::Start ? "Start" : s == UiSound::TabChange ? "TabChange" : s == UiSound::Navigation ? "Navigation" : "GameLaunch";
    }

    bool set(const std::string &key, const std::string &v) override {
        const bool on = sampleBool(v);
        if (key == "interface.systems") { resize(size_t(std::atoi(v.c_str()))); return true; }   // number of systems with games
        if (key == "interface.hidden") {                                                          // the first N of them hidden
            const size_t n = size_t(std::atoi(v.c_str()));
            for (size_t i = 0; i < systems.size(); ++i) systems[i].hidden = i < n;
            return true;
        }
        if (key == "interface.arcade") { hasArcade = on; return true; }
        if (key == "interface.gamestab") { gamesTab = on; return true; }
        if (key == "interface.arcadetab") { arcadeTab = on; return true; }
        if (key == "interface.arcadeconsoles") { arcadeInConsoles = on; return true; }
        if (key == "interface.starttab") { tab = v; return true; }
        if (key == "interface.tabdisplay") { display = v == "icons" ? 1 : v == "both" ? 2 : 0; return true; }
        if (key == "interface.theme") { theme = std::atoi(v.c_str()); return true; }
        if (key == "interface.dark") { dark = on; return true; }
        if (key == "interface.boxart") { boxArt = on; return true; }
        if (key == "interface.smallgrid") { smallGrid = on; return true; }
        if (key == "interface.startsound") { startSnd = on; return true; }
        if (key == "interface.menusounds") { menuSnd = on; return true; }
        if (key == "interface.gameview") { view = v; return true; }
        if (key == "interface.soundlog") { printSounds = on; return true; }
        return false;
    }
    std::vector<std::string> help() const override {
        return {"interface.systems=<count>  systems with games (Manage Consoles)",
                "interface.hidden=<count>  the first <count> of them hidden",
                "interface.arcade=0|1  the library has Arcade games (the Arcade tab needs them)",
                "interface.gamestab=0|1  interface.arcadetab=0|1  interface.arcadeconsoles=0|1",
                "interface.starttab=home|favorites|systems|games|arcade  interface.tabdisplay=names|icons|both",
                "interface.theme=<0..21>  interface.dark=0|1  interface.boxart=0|1  interface.smallgrid=0|1",
                "interface.startsound=0|1  interface.menusounds=0|1  interface.soundlog=0|1 (prints played sounds)",
                "interface.gameview=grid|small|list|individual"};
    }

private:
    static std::string lower(std::string s) {
        for (char &c : s) c = char(std::tolower((unsigned char)c));
        return s;
    }
    void resize(size_t n) {
        systems.resize(n);
        for (size_t i = 0; i < n; ++i)
            if (systems[i].name.empty()) {
                static const char *kNames[] = {"Atari 2600", "Game Boy", "Mega Drive", "NES", "PC Engine", "SNES", "Sega Master System",
                                               "TurboGrafx-16", "Amiga", "Commodore 64", "ZX Spectrum", "Neo Geo", "MSX", "Atari 7800"};
                const size_t k = sizeof(kNames) / sizeof(kNames[0]);
                systems[i].name = i < k ? kNames[i] : std::string("Example system ") + std::to_string(i + 1);
            }
    }
};

}  // namespace patsgui
