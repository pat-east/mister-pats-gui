#pragma once

#include <memory>
#include <vector>

#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/DetailPanel.h"
#include "ui-framework/kit/Grid.h"
#include "ui-framework/kit/Input.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/Modal.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/TextEntryDialog.h"
#include "ui-framework/kit/Toast.h"
#include "ui-framework/kit/Wizard.h"
#include "ui-framework/kit/WizardParts.h"

namespace ui {
namespace demo {

// The UI Wireframes demo (docs/UI-DEMO.md): a main-screen layout with tabs, each tab showing
// components with generic content. Nothing here is application content.
enum Tab { Overview, TextTab, Lists, Data, Grids, Dialogs, Flows, Input, Appearance, TabCount };

void registerStrings();

struct Options {
    bool forceWarning = false;   // show the nav items in their warning state
};

// What the demo reacts to; the host maps its own input onto these.
enum class Event { TabPrev, TabNext, Up, Down, Left, Right, Confirm, CycleView, Back, Delete, Start };

struct State {
    int tab = Overview;
    int cursor[TabCount] = {0, 0, 3, 1, 0, 0, 0, 0, 0};   // the cursor row per tab (where a tab has one)

    // The Appearance tab's live settings.
    int textScale = 100;
    bool pseudo = false;
    bool forceWarning = false;
    TabDisplay tabDisplay = TabDisplay::Names;
    bool dark = true;            // dark or light mode
    int colorTheme = 0;          // an index into the theme's color themes
    bool highContrast = false;
    int highlight = kDefaultHighlight;   // the highlight color of high contrast

    // The Grid views tab ("Grids"): the view mode (0 Grid, 1 Grid-Small, 2 Grid-List) and
    // the layouts, configured when drawn and read when input arrives, like the Classic GUI's views.
    int viewMode = 0;
    GridLayout grid;
    ListLayout list;
    int gridCount = 40;

    // The Grids tab's search field (PageHeader): its query, and whether the cursor is on the field.
    std::string gridSearch;
    bool searchFocused = false;

    // The Input tab: two text fields (the second masked). Editing one opens the TextEntryDialog.
    std::string field[2];
    int entryTarget = -1;        // 0 and 1: an Input field, 2: the Grids search; -1 none (no dialog)
    TextEntryDialog entry;

    // The Flows tab: the open wizard (a Medium flow or a Large flow, each with its own pages), and the crash screen preview.
    // Shared, so a copy of the state (the renders) sees the same flow.
    std::shared_ptr<Wizard> flow;
    bool crashPreview = false;

    // The Text tab's toast: A shows a confirmation, Y a one-line notice.
    Toast toast;

    // Dialogs opened from the Dialogs tab; while one is open it takes all input and the tabs are locked.
    ModalStack modals;
};

// System properties for the nav items. On a device the platform provider fills them in; the host
// uses fixed sample values.
struct SystemValues {
    std::string clock = "10:42";
    std::string date = "Tue 22 Nov";
    std::string address = "192.168.1.20";
    bool wireless = false;   // the link is Wi-Fi (the Wi-Fi word and symbol) instead of Ethernet
};

// Generated pictures, so the demo needs no files: the logo, the tab icons, cover-like sample art at the
// grid's art size (never scaled) and larger icons for icon tiles.
struct Assets {
    ImagePtr logo;
    std::vector<IconPtr> tabIcons;
    std::vector<ImagePtr> samples;        // cover art at the grid's art size
    std::vector<ImagePtr> samplesSmall;   // at the compact grid's art size
    std::vector<ImagePtr> samplesDetail;  // at the detail panel's picture size
    std::vector<IconPtr> bigIcons;
};
Assets makeAssets(const Theme &t);

// Applies the Appearance tab's settings that live outside the drawing (text size, pseudo-language).
void applyAppearance(const State &state, Theme &theme);

// Applies an event; returns true when the demo asks to be closed (Back). An open dialog takes the event first.
bool handle(State &state, Event event, const Assets &assets);

// Advances time-driven things (a Busy wizard page). Returns true when something changed on screen.
bool update(State &state, float deltaSeconds);

// Starts a flow of the Flows tab: 0 the Medium flow (text, choose, capture, busy, results), 1 the Large flow (multiple
// choice, keyboard, scrolling review, busy without cancel, warning result).
void startFlow(State &state, int which);

// Opens one of the demo's dialogs (0 information, 1 confirm, 2 error, 3 result, 4 progress, 5 choose,
// 6 viewer, 7 a confirm over an information dialog).
constexpr int kDialogCount = 8;
void openDemoDialog(State &state, int index, const Assets &assets);

// Draws the open dialogs over everything (their scrim covers the whole screen) and redraws the help bar
// above it, with the top dialog's help. Call it after drawShell and drawTab, with the same shell spec.
void drawOverlays(Painter &p, const Theme &t, State &state, const ShellSpec &spec);

// The shell for the state: tabs, nav items, help, footer. The help lists only what works now.
ShellSpec shellFor(const State &state, const Options &options, const SystemValues &values, const Assets &assets);

// Draws the active tab's content window.
void drawTab(Painter &p, const Theme &t, State &state, const Rect &content, const Assets &assets);

// A generated logo, so the splash needs no asset.
ImagePtr makeLogo(const Theme &t);

}  // namespace demo
}  // namespace ui
