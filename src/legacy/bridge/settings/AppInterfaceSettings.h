#pragma once

// Host side of the Interface service: implements src/app/settings/InterfaceService.h on the Classic objects, calling what
// Classic's Interface rows call (src/legacy/SettingsScreen.cpp buildRows(), src/legacy/App.cpp toggleGamesTab()/toggleArcadeTab()/
// cycleDefaultView(), src/legacy/SystemVisibilityScreen.*, src/legacy/UiSounds.*).
// Every setter writes Preferences (which saves at once, as Classic); the theme setters also keep the Classic Theme in step.
// The new GUI's own look follows through PatsGuiScreen::syncLook() (it reads Preferences every frame).

#include "app/settings/InterfaceService.h"
#include "bridge/settings/AdapterEnv.h"

class AppInterfaceSettings : public patsgui::InterfaceService {
public:
    explicit AppInterfaceSettings(const AdapterEnv &env) : env_(env) {}

    bool available() const override;
    void update(float deltaSeconds) override;
    bool showGamesTab() const override;
    void setShowGamesTab(bool on) override;
    bool showArcadeTab() const override;
    void setShowArcadeTab(bool on) override;
    bool gamesTabVisible() const override;
    bool arcadeTabVisible() const override;
    bool showArcadeInConsoles() const override;
    void setShowArcadeInConsoles(bool on) override;
    std::string startTab() const override;
    void setStartTab(const std::string &tab) override;
    int tabDisplay() const override;
    void setTabDisplay(int display) override;
    int colorTheme() const override;
    void setColorTheme(int index) override;
    bool darkMode() const override;
    void setDarkMode(bool on) override;
    bool showBoxArt() const override;
    void setShowBoxArt(bool on) override;
    bool loadSmallBoxartsInGridView() const override;
    void setLoadSmallBoxartsInGridView(bool on) override;
    bool startSound() const override;
    void setStartSound(bool on) override;
    bool menuSounds() const override;
    void setMenuSounds(bool on) override;
    std::string gameView() const override;
    void setGameView(const std::string &view) override;
    void playSound(patsgui::UiSound sound) override;
    std::vector<SystemRow> systemsWithGames() const override;
    void setSystemHidden(const std::string &name, bool hidden) override;

private:
    AdapterEnv env_;
};
