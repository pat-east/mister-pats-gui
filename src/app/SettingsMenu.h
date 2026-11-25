#pragma once

// The Settings menu as data: the shared row model (kinds, option lists, conditions) and the builders each
// category uses to write its own table. No drawing, no behavior. The tables live in the category files
// (src/app/settings/<Category>Settings.cpp) and mirror the Classic Settings screen (src/legacy/SettingsScreen.cpp
// buildRows()/buildUpdateRows()/settingHelp()); src/app/PARITY-settings.md lists every Classic row against them.
// Behavior is attached by the categories (src/app/settings/SettingsCategory.h).

#include <map>
#include <string>
#include <vector>

#include "ui-framework/kit/Text.h"

namespace patsgui {

enum class RowKind { Action, Switch, Value, Info, Heading };

// One option of a Value row that cycles ("Names", "Icons", "Both"). `key` is the catalog key.
struct MenuOption {
    const char *key;
    const char *english;
};
// Where a cycling row gets its options: its own list, or the framework's themes and highlight colors.
enum class OptionSource { Own, ColorThemes, HighlightColors };

// When a row is shown or enabled. Evaluated against MenuState. Developer-only things use DevBuild.
enum class Cond {
    Always,
    Never,                 // always disabled (Classic draws the row greyed out and gives it no action)
    DevBuild,              // development builds only (the Developer category)
    DiscDrive,             // an optical drive is present
    WifiAdapter,           // a Wi-Fi adapter is present
    HighContrastOn,        // the High contrast switch is on
    Pinned,                // the GUI version is pinned
    PinnedNewer,           // pinned and a newer version is installed than the selected one
    UseNewestAllowed,      // phase Idle, Current, Available, Blocked, Error or RestartRequired
    PhaseWaiting,          // WaitingForInternet or Checking
    PhaseBusy,             // Downloading, Verifying or Installing
    PhaseCancelable,       // Downloading or Verifying
    PhaseRestart,          // RestartRequired
    PhaseRestartNoPrompt,  // RestartRequired and the success prompt is not showing
    PhaseCheckable,        // none of Waiting, Busy, RestartRequired
    PhaseAvailInstall,     // Available and the update can be installed
    PhaseStatusLine,       // Blocked, Error, or Available and not installable
    PhaseBlocked,          // Blocked (lists the paths to repair)
    PhaseCurrent,          // Current
    FontAvailable,         // the Space Grotesk files are installed (the row is then a switch)
};

// A label that is not a catalog entry but text the update service supplies.
enum class Dyn { None, StatusText, RepairPaths, InstallTarget };

struct MenuRow {
    const char *id;            // stable id, also the seam key; label key "settings.<id>", help "settings.<id>.help"
    const char *label;         // English label (registered under the key); null for dynamic rows
    const char *help;          // English help text; null = none (Classic shows only the title)
    RowKind kind;
    Cond show = Cond::Always;
    Cond enabled = Cond::Always;
    Cond switchWhen = Cond::Never;         // the row is a Switch while this holds (Classic shows On/Off as a checkbox)
    Dyn dyn = Dyn::None;
    // Value
    const MenuOption *options = nullptr;   // cycling choices (in memory unless the category's current() gives the real value)
    int optionCount = 0;
    OptionSource source = OptionSource::Own;
    int initial = 0;                       // option index, or 1 for a switch that starts on
    const char *value = nullptr;           // fixed value key (status text kept by the seam), or null
    const char *disabledValue = nullptr;   // value key shown instead when the row is disabled ("No drive")
    const char *reason = nullptr;          // why the row is disabled, in a sentence (help pane: "Not available: ..."); null = none
    const char *later = nullptr;           // what Classic opens here (a submenu or flow); documentation only
};

struct MenuCategory {
    const char *id;
    const char *label;         // English
    Cond show = Cond::Always;
    std::vector<MenuRow> rows;
};

// What the menu needs to know to decide conditions and to show live values. The host fills it later
// (SettingsActions::refresh); the defaults here are sample values so the skeleton looks real.
enum class UpdatePhase { Idle, Current, Available, Blocked, Error, WaitingForInternet, Checking, Downloading,
                         Verifying, Installing, Confirming, RestartRequired };

struct MenuState {
    bool devBuild = true;
    bool discDrive = true;
    bool fontAvailable = true;             // Space Grotesk installed: the row shows On/Off, else a download status
    bool wifiAdapter = true;
    bool highContrast = false;             // kept in step with the High contrast switch
    UpdatePhase phase = UpdatePhase::Current;
    bool pinned = false;
    bool canInstall = false;
    bool restartPromptVisible = false;
    std::string running = "0.8.10", selected = "0.8.10", newest = "0.8.10", target;
    std::string statusText = "You are using the newest release";
    std::string progress = "42%";
    std::vector<std::string> repairPaths;
    // Status values the seam keeps per row id (Classic's dynamic value texts).
    std::map<std::string, std::string> status = {
        {"library.tgdb", "Not configured"},
        {"library.ss.account", "Not checked"},
        {"library.ss.user", "Not set"},
        {"library.ss.password", "Not set"},
        {"accessibility.grotesk", "Download font"},
        {"system.disccores", "Install v0.7.0"},
        {"system.icons", "All icons ready"},
        {"system.timezone", "System default"},
    };
};

// A row after conditions: the table row plus, for dynamic rows, its text.
struct VisibleRow {
    const MenuRow *row = nullptr;
    RowKind kind = RowKind::Action;   // the row's kind now (a row can switch kind with the state)
    std::string label;         // data text for dynamic rows; empty = use the catalog label
    bool enabled = true;
};

bool holds(Cond cond, const MenuState &state);
// The rows of a category table with every condition applied, headings included.
std::vector<VisibleRow> visibleRows(const MenuCategory &category, const MenuState &state);
// How many options a cycling row has (its own list, the framework's themes or highlight colors).
int optionCount(const MenuRow &row);

// Registers the strings of a table: the category label (settings.cat.<id>), every row label
// (settings.<rowId>) and help (settings.<rowId>.help). A category calls it from its strings function.
void registerTable(ui::Catalog &c, const MenuCategory &category);
// Registers the option labels of a cycling row's list (MenuOption::key -> english).
void registerOptions(ui::Catalog &c, const MenuOption *options, int count);

// ---- Builders for a category table (src/app/settings/<Category>Settings.cpp). Plain functions, no state.
#define PATSGUI_OPTIONS(name, ...) static const ::patsgui::MenuOption name[] = {__VA_ARGS__}
#define PATSGUI_COUNT(a) int(sizeof(a) / sizeof(a[0]))

namespace menu {
inline MenuRow row(const char *id, const char *label, const char *help, RowKind kind) {
    MenuRow r;
    r.id = id;
    r.label = label;
    r.help = help;
    r.kind = kind;
    return r;
}
// A row that does something when A is pressed (opens a page, starts a task). `later` is documentation only.
inline MenuRow action(const char *id, const char *label, const char *help, const char *later = nullptr) {
    MenuRow r = row(id, label, help, RowKind::Action);
    r.later = later;
    return r;
}
// An On/Off switch.
inline MenuRow toggle(const char *id, const char *label, const char *help, bool on) {
    MenuRow r = row(id, label, help, RowKind::Switch);
    r.initial = on ? 1 : 0;
    return r;
}
// A value that cycles through its options with A.
inline MenuRow cycle(const char *id, const char *label, const char *help, const MenuOption *options, int count,
                     int initial = 0) {
    MenuRow r = row(id, label, help, RowKind::Value);
    r.options = options;
    r.optionCount = count;
    r.initial = initial;
    return r;
}
// A value the category supplies as text (a status, MenuState::status[id]); A opens or runs something.
inline MenuRow status(const char *id, const char *label, const char *help, const char *later) {
    MenuRow r = row(id, label, help, RowKind::Value);
    r.later = later;
    return r;
}
inline MenuRow info(const char *id, const char *label, const char *help) { return row(id, label, help, RowKind::Info); }
inline MenuRow heading(const char *id, const char *label) { return row(id, label, nullptr, RowKind::Heading); }
inline MenuRow shown(MenuRow r, Cond show) { r.show = show; return r; }
inline MenuRow enabledWhen(MenuRow r, Cond enabled) { r.enabled = enabled; return r; }
inline MenuRow switchedBy(MenuRow r, Cond c, int initial) { r.switchWhen = c; r.initial = initial; return r; }
inline MenuRow because(MenuRow r, const char *reason) { r.reason = reason; return r; }   // the reason a disabled row gives
inline MenuRow dynamic(MenuRow r, Dyn dyn) { r.dyn = dyn; return r; }
}  // namespace menu

}  // namespace patsgui
