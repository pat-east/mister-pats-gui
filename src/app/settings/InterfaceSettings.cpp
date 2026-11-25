#include "app/settings/InterfaceSettings.h"

#include <algorithm>

#include "app/settings/StandardPages.h"
#include "app/settings/interface/ManageConsolesPage.h"

namespace patsgui {
namespace {

using namespace menu;

PATSGUI_OPTIONS(kStartTab, {"settings.opt.home", "Home"}, {"settings.opt.favorites", "Favorites"},
     {"settings.opt.consoles", "Consoles"}, {"settings.opt.games", "Games"}, {"settings.opt.arcade", "Arcade"});
PATSGUI_OPTIONS(kTabDisplay, {"settings.opt.names", "Names"}, {"settings.opt.icons", "Icons"}, {"settings.opt.both", "Both"});
// Classic's words (displayNameForGameView, src/legacy/GamesScreen.cpp): the tabs keep their own names for the three views.
PATSGUI_OPTIONS(kGameView, {"settings.opt.view.grid", "Grid"}, {"settings.opt.view.small", "Boxart small"},
     {"settings.opt.view.list", "List"}, {"settings.opt.individual", "Individual"});

// The stored strings, in the order of the option lists (Classic's cycle code: SettingsScreen.cpp Start tab, App::cycleDefaultView).
const char *const kStartTabValues[] = {"home", "favorites", "systems", "games", "arcade"};
const char *const kGameViewValues[] = {"grid", "small", "list", "individual"};

template <size_t N>
int indexOf(const char *const (&values)[N], const std::string &value) {
    for (size_t i = 0; i < N; ++i)
        if (value == values[i]) return int(i);
    return 0;   // Classic: an unknown value counts as the first one (Start tab: index 0; Game view: its cycle restarts at grid)
}

// The Classic rows of this category, in Classic's order (src/legacy/SettingsScreen.cpp buildRows()).
const MenuCategory &table() {
    static const MenuCategory category = [] {
        MenuCategory iface{"interface", "Interface", Cond::Always, {}};
        iface.rows = {
            action("interface.consoles", "Manage Consoles",
                   "Choose which consoles appear in the Consoles and Games views.",
                   "Classic: Manage consoles screen (onManageSystems)"),
            toggle("interface.gamestab", "Show Games tab",
                   "Show or hide the tab that lists games from every system.", true),
            toggle("interface.arcadetab", "Show Arcade tab",
                   "Show or hide the tab that lists playable Arcade games.", true),
            toggle("interface.arcadeconsoles", "Show Arcade in Consoles",
                   "Add Arcade to the Consoles grid. The Arcade tab has its own list either way.", false),
            cycle("interface.starttab", "Start tab",
                  "Choose the tab the GUI opens on when it starts. A tab that is turned off or has no games falls back to Home.",
                  kStartTab, PATSGUI_COUNT(kStartTab)),
            cycle("interface.tabdisplay", "Tab display",
                  "Choose whether the top navigation shows tab names, icons, or both.", kTabDisplay, PATSGUI_COUNT(kTabDisplay)),
            [] {
                MenuRow r = cycle("interface.theme", "Color theme",
                                  "Choose the colors for the normal interface background, surfaces and highlights. High contrast keeps its separate highlight color.",
                                  nullptr, 0);
                r.source = OptionSource::ColorThemes;
                return r;
            }(),
            toggle("interface.dark", "Dark mode",
                   "Use dark backgrounds when On. Turn it Off for light backgrounds and dark text. High contrast keeps its black background.",
                   true),
            toggle("interface.boxart", "Show box art", "Show or hide cover pictures while browsing games.", true),
            toggle("interface.smallgrid", "Load small boxarts in grid-view",
                   "Use smaller cover images in Grid view. This can noticeably reduce image loading time.", false),
            toggle("interface.startsound", "Start sound", "Play a sound after the GUI finishes starting.", true),
            // "Menu sounds" are switched off for 0.9.0 (owner decision 2026-11-24: the tick came much too late); the row, the
            // navigation / tab change / game launch sounds and their code stay for a later release.
            cycle("interface.gameview", "Game view",
                  "Choose one view for every tab, or Individual to remember a separate view for Home, Favorites, Consoles and Games. Press Y while browsing to change that tab's view.",
                  kGameView, PATSGUI_COUNT(kGameView)),
        };
        return iface;
    }();
    return category;
}

void strings(ui::Catalog &c) {
    registerTable(c, table());
    c.set("settings.interface.gameview.toast", "Game view: {1}");            // App::cycleDefaultView: notify("Game view: ...")
    c.set("settings.interface.consoles.title", "Manage Consoles");           // the row's name (Classic's screen says "Systems")
    c.set("settings.interface.consoles.count", "{1} of {2} shown");
    c.set("settings.interface.consoles.empty.title", "No consoles yet");
    c.set("settings.interface.consoles.empty.why", "No games were found, so there is no console to show or hide.");
    c.set("settings.interface.consoles.empty.next", "Build the game list in Settings, then come back.");
}

}  // namespace

std::vector<VisibleRow> InterfaceSettings::rows(const MenuState &state) const { return visibleRows(table(), state); }

InterfaceService *InterfaceSettings::service() const { return attached() ? usable(env().services.interfaceService) : nullptr; }

// The real value of a row (Classic's lambdas read the live preference every frame), or kNoValue without a service.
int InterfaceSettings::current(const std::string &id) const {
    InterfaceService *s = service();
    if (!s) return kNoValue;
    if (id == "interface.gamestab") return s->showGamesTab() ? 1 : 0;
    if (id == "interface.arcadetab") return s->showArcadeTab() ? 1 : 0;
    if (id == "interface.arcadeconsoles") return s->showArcadeInConsoles() ? 1 : 0;
    if (id == "interface.starttab") return indexOf(kStartTabValues, s->startTab());
    if (id == "interface.tabdisplay") return std::max(0, std::min(s->tabDisplay(), 2));
    if (id == "interface.theme") return s->colorTheme();
    if (id == "interface.dark") return s->darkMode() ? 1 : 0;
    if (id == "interface.boxart") return s->showBoxArt() ? 1 : 0;
    if (id == "interface.smallgrid") return s->loadSmallBoxartsInGridView() ? 1 : 0;
    if (id == "interface.startsound") return s->startSound() ? 1 : 0;
    if (id == "interface.gameview") return indexOf(kGameViewValues, s->gameView());
    return kNoValue;
}

// A switch was toggled or a choice moved to option `value`: the same Preferences setters as Classic's lambdas
// (SettingsScreen.cpp 349-459). Every setter saves at once.
void InterfaceSettings::changed(const std::string &id, int value) {
    InterfaceService *s = service();
    if (!s) return;
    const bool on = value != 0;
    if (id == "interface.gamestab") s->setShowGamesTab(on);                  // App::toggleGamesTab
    else if (id == "interface.arcadetab") s->setShowArcadeTab(on);           // App::toggleArcadeTab
    else if (id == "interface.arcadeconsoles") s->setShowArcadeInConsoles(on);
    else if (id == "interface.starttab") s->setStartTab(kStartTabValues[std::max(0, std::min(value, 4))]);
    else if (id == "interface.tabdisplay") s->setTabDisplay(std::max(0, std::min(value, 2)));
    else if (id == "interface.theme") s->setColorTheme(value);
    else if (id == "interface.dark") s->setDarkMode(on);
    else if (id == "interface.boxart") s->setShowBoxArt(on);
    else if (id == "interface.smallgrid") s->setLoadSmallBoxartsInGridView(on);
    else if (id == "interface.startsound") s->setStartSound(on);
    else if (id == "interface.gameview") {                                   // App::cycleDefaultView
        const int index = std::max(0, std::min(value, 3));
        s->setGameView(kGameViewValues[index]);
        env().pages.toast(ui::Text::data(ui::format("settings.interface.gameview.toast", {ui::Catalog::get().tr(kGameView[index].key)})));
    }
}

bool InterfaceSettings::activate(const std::string &rowId, PageStack &pages) {
    if (rowId == "interface.consoles") return openPage("consoles", pages);   // App::openVisibility
    return false;
}

bool InterfaceSettings::openPage(const std::string &pageId, PageStack &pages) {
    InterfaceService *s = service();
    if (pageId == "consoles" && s) {
        pages.push(makeManageConsolesPage(*s));
        return true;
    }
    return false;
}

PATSGUI_REGISTER_SETTINGS_CATEGORY(InterfaceSettings, strings);

}  // namespace patsgui
