#pragma once

// What the Interface category needs from the host (Classic: Preferences and the App callbacks behind
// SettingsScreen's Interface rows: toggleGamesTab(), toggleArcadeTab(), cycleDefaultView(), openVisibility()).
// Owned and extended by the Interface agent. Theme rows (color theme, dark mode) also change the framework
// theme of the new GUI: the host adapter applies them to its ui::Theme and requests a redraw.

#include <string>
#include <vector>

namespace patsgui {

// The four sounds of Classic's UiSounds (src/legacy/UiSounds.h). The shell and the Settings tab ask for them; the service
// plays one only when its preference allows it (Start: "Start sound"; the others: "Menu sounds"), never blocks and
// never reports a missing audio device (no aplay: silent, as Classic).
enum class UiSound { Start, TabChange, Navigation, GameLaunch };

class InterfaceService {
public:
    virtual ~InterfaceService() = default;
    virtual bool available() const = 0;
    virtual void update(float deltaSeconds) = 0;

    virtual bool showGamesTab() const = 0;
    virtual void setShowGamesTab(bool on) = 0;           // App::toggleGamesTab(): Preferences::setShowGamesTab (the shell leaves a vanished tab)
    virtual bool showArcadeTab() const = 0;
    virtual void setShowArcadeTab(bool on) = 0;          // App::toggleArcadeTab()
    // What the shell reads every frame to decide which tabs the tab row has (Classic App::visibleTabs()):
    //   Games tab:  Preferences::showGamesTab()
    //   Arcade tab: App::arcadeTabShown() = Preferences::showArcadeTab() && the library has an Arcade system
    virtual bool gamesTabVisible() const = 0;
    virtual bool arcadeTabVisible() const = 0;
    virtual bool showArcadeInConsoles() const = 0;
    virtual void setShowArcadeInConsoles(bool on) = 0;
    virtual std::string startTab() const = 0;            // "home" "favorites" "systems" "games" "arcade"
    virtual void setStartTab(const std::string &tab) = 0;
    virtual int tabDisplay() const = 0;                  // 0 Names, 1 Icons, 2 Both
    virtual void setTabDisplay(int display) = 0;
    // The index into Classic's 22 color themes, which the framework carries in the same order (src/ui-framework/kit/Palettes.cpp).
    // The set writes Preferences::setColorTheme and keeps the Classic Theme in step (Theme::setColorTheme,
    // Context::onThemeChanged); the new GUI's look follows through PatsGuiScreen::syncLook().
    virtual int colorTheme() const = 0;
    virtual void setColorTheme(int index) = 0;
    virtual bool darkMode() const = 0;
    virtual void setDarkMode(bool on) = 0;
    virtual bool showBoxArt() const = 0;
    virtual void setShowBoxArt(bool on) = 0;
    virtual bool loadSmallBoxartsInGridView() const = 0;
    virtual void setLoadSmallBoxartsInGridView(bool on) = 0;
    virtual bool startSound() const = 0;
    virtual void setStartSound(bool on) = 0;
    virtual bool menuSounds() const = 0;
    virtual void setMenuSounds(bool on) = 0;
    virtual std::string gameView() const = 0;            // "grid" "small" "list" "individual" (Preferences::defaultView)
    // Preferences::setDefaultView. Classic's App::cycleDefaultView also resets its own live screens; the new GUI's tabs
    // read the view when they are shown, and Classic's screens pick it up at their next start (owner decision).
    virtual void setGameView(const std::string &view) = 0;

    // ---- Sounds (Classic: UiSounds). Gated by the two preferences above.
    virtual void playSound(UiSound sound) = 0;

    // ---- Manage Consoles (Classic: SystemVisibilityScreen, HiddenSystems)
    struct SystemRow { std::string name; bool hidden = false; };
    virtual std::vector<SystemRow> systemsWithGames() const = 0;     // sorted, as the Classic list
    virtual void setSystemHidden(const std::string &name, bool hidden) = 0;
};

}  // namespace patsgui
