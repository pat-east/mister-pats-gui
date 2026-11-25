#include "bridge/settings/AppInterfaceSettings.h"

#include <algorithm>
#include <cstring>
#include <strings.h>

#include "UiSounds.h"

bool AppInterfaceSettings::available() const { return true; }
void AppInterfaceSettings::update(float) {}   // nothing in this category is polled: values are read from Preferences when asked

// Show Games tab: Preferences::setShowGamesTab (App::toggleGamesTab). The new shell reads gamesTabVisible() every frame and
// leaves a vanished tab (App's selectTab(Home) when the current tab goes away).
bool AppInterfaceSettings::showGamesTab() const { return env_.context.preferences.showGamesTab(); }
void AppInterfaceSettings::setShowGamesTab(bool on) { env_.context.preferences.setShowGamesTab(on); }
bool AppInterfaceSettings::showArcadeTab() const { return env_.context.preferences.showArcadeTab(); }
void AppInterfaceSettings::setShowArcadeTab(bool on) { env_.context.preferences.setShowArcadeTab(on); }

bool AppInterfaceSettings::gamesTabVisible() const { return env_.context.preferences.showGamesTab(); }
// App::arcadeTabShown(): the preference AND an Arcade system in the library.
bool AppInterfaceSettings::arcadeTabVisible() const {
    return env_.context.preferences.showArcadeTab() && env_.context.library.arcadeSystem() != nullptr;
}

bool AppInterfaceSettings::showArcadeInConsoles() const { return env_.context.preferences.showArcadeInConsoles(); }
void AppInterfaceSettings::setShowArcadeInConsoles(bool on) { env_.context.preferences.setShowArcadeInConsoles(on); }

std::string AppInterfaceSettings::startTab() const { return env_.context.preferences.startTab(); }
void AppInterfaceSettings::setStartTab(const std::string &tab) { env_.context.preferences.setStartTab(tab); }

int AppInterfaceSettings::tabDisplay() const {
    switch (env_.context.preferences.tabDisplay()) {
        case TabDisplay::Icons: return 1;
        case TabDisplay::Both: return 2;
        case TabDisplay::Names: break;
    }
    return 0;
}
void AppInterfaceSettings::setTabDisplay(int display) {
    env_.context.preferences.setTabDisplay(display == 1 ? TabDisplay::Icons : display == 2 ? TabDisplay::Both : TabDisplay::Names);
}

// Color theme and Dark mode (SettingsScreen.cpp 405-419): Preferences, the Classic Theme, then Classic's redraw hook.
int AppInterfaceSettings::colorTheme() const { return env_.context.preferences.colorTheme(); }
void AppInterfaceSettings::setColorTheme(int index) {
    Context &context = env_.context;
    context.preferences.setColorTheme(index);   // ignores an index out of range
    context.theme.setColorTheme(context.preferences.colorTheme());
    if (context.onThemeChanged) context.onThemeChanged();
}
bool AppInterfaceSettings::darkMode() const { return env_.context.preferences.darkMode(); }
void AppInterfaceSettings::setDarkMode(bool on) {
    Context &context = env_.context;
    context.preferences.setDarkMode(on);
    context.theme.setDarkMode(on);
    if (context.onThemeChanged) context.onThemeChanged();
}

bool AppInterfaceSettings::showBoxArt() const { return env_.context.preferences.showBoxArt(); }
void AppInterfaceSettings::setShowBoxArt(bool on) { env_.context.preferences.setShowBoxArt(on); }
bool AppInterfaceSettings::loadSmallBoxartsInGridView() const { return env_.context.preferences.loadSmallBoxartsInGridView(); }
void AppInterfaceSettings::setLoadSmallBoxartsInGridView(bool on) { env_.context.preferences.setLoadSmallBoxartsInGridView(on); }
bool AppInterfaceSettings::startSound() const { return env_.context.preferences.startSoundEnabled(); }
void AppInterfaceSettings::setStartSound(bool on) { env_.context.preferences.setStartSoundEnabled(on); }
bool AppInterfaceSettings::menuSounds() const { return env_.context.preferences.menuSoundsEnabled(); }
void AppInterfaceSettings::setMenuSounds(bool on) { env_.context.preferences.setMenuSoundsEnabled(on); }

// Game view: Preferences::setDefaultView only (it validates grid / small / list / individual). Classic's cycleDefaultView also
// resets its own live screens; the new tabs read the preference when they are shown (owner decision: Classic's screens pick
// it up at their next start).
std::string AppInterfaceSettings::gameView() const { return env_.context.preferences.defaultView(); }
void AppInterfaceSettings::setGameView(const std::string &view) { env_.context.preferences.setDefaultView(view); }

// UiSounds, gated as at Classic's call sites (App.cpp 166, 679, 955, 1431).
void AppInterfaceSettings::playSound(patsgui::UiSound sound) {
    const Preferences &prefs = env_.context.preferences;
    switch (sound) {
        case patsgui::UiSound::Start: if (prefs.startSoundEnabled()) UiSounds::play(); break;
        case patsgui::UiSound::TabChange: if (prefs.menuSoundsEnabled()) UiSounds::playTabChange(); break;
        case patsgui::UiSound::Navigation: if (prefs.menuSoundsEnabled()) UiSounds::playNavigation(); break;
        case patsgui::UiSound::GameLaunch: if (prefs.menuSoundsEnabled()) UiSounds::playGameLaunch(); break;
    }
}

// SystemVisibilityScreen::refresh(): every system with games, sorted case-insensitively by name.
std::vector<patsgui::InterfaceService::SystemRow> AppInterfaceSettings::systemsWithGames() const {
    Context &context = env_.context;
    std::vector<SystemRow> out;
    for (const GameSystem &system : context.library.systems())
        if (context.library.hasGames(system)) out.push_back({system.name, context.hiddenSystems.contains(system.name)});
    std::sort(out.begin(), out.end(), [](const SystemRow &a, const SystemRow &b) { return strcasecmp(a.name.c_str(), b.name.c_str()) < 0; });
    return out;
}

// HiddenSystems::toggle (saves hidden-systems.txt at once); a row that already has the state is left alone.
void AppInterfaceSettings::setSystemHidden(const std::string &name, bool hidden) {
    HiddenSystems &hiddenSystems = env_.context.hiddenSystems;
    if (hiddenSystems.contains(name) != hidden) hiddenSystems.toggle(name);
}
