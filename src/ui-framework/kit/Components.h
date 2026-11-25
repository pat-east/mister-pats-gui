#pragma once

#include <string>
#include <vector>

#include "ui-framework/kit/Primitives.h"

namespace ui {

// ---- ListItem: one list row, seven kinds (docs/components/ListItem.md)
// Entry is a content row (a title, an optional second line, a favorite mark); the other kinds are controls.
enum class ListItemKind { Nav, Value, Switch, Choice, Action, Info, Heading, Entry };
enum class Density { Roomy, Dense };

struct ListItemSpec {
    ListItemKind kind = ListItemKind::Nav;
    Text title;
    Text detail;                 // second line (Choice)
    Text value;                  // Value and Info
    Text status;                 // Choice: status word on the right
    Tone statusTone = Tone::Neutral;
    bool checked = false;        // Switch and Choice
    bool favorite = false;       // Entry: the mark on the right
    bool disabled = false;       // muted, stays focusable
};
struct ListItemState {
    bool cursor = false;         // this row has the cursor
    bool paneActive = true;      // the pane that owns the cursor has focus
};

// Roomy density (menus): frame 56, pitch 64. Dense (content lists): frame 46, pitch 52. Two-line rows:
// frame 62, pitch 68 (M0).
int listItemFrameHeight(const Theme &t, bool twoLine, Density d = Density::Roomy);
int listItemPitch(const Theme &t, bool twoLine, Density d = Density::Roomy);
void drawListItem(Painter &p, const Theme &t, const Rect &frame, const ListItemSpec &spec,
                  const ListItemState &state);

// ---- Hints: the bottom-left legend, ordered by key rank (docs/components/Hints.md)
enum class HintKey { Dpad, UpDown, LeftRight, Shoulders, Triggers, X, Y, A, AB, Start, B };
struct Hint {
    HintKey key;
    Text action;
    bool essential = false;      // kept when the line has to be shortened
    bool hold = false;           // written "Hold <key> <action>": the action needs the button held
};
// A hint from a catalog key: ui::hint(HintKey::A, "hint.play", true). `hold` writes "Hold <key> <action>".
inline Hint hint(HintKey key, const char *textKey, bool essential = false, bool hold = false) {
    Hint h;
    h.key = key;
    h.action = Text::key(textKey);
    h.essential = essential;
    h.hold = hold;
    return h;
}
// The same with an action that is not a catalog key (a page names its own words).
inline Hint hint(HintKey key, const Text &action, bool essential = false, bool hold = false) {
    Hint h;
    h.key = key;
    h.action = action;
    h.essential = essential;
    h.hold = hold;
    return h;
}
// A hold in progress: "Hold <key> <action>" with a thin bar under it that fills from 0 to 1. Draws inside
// `area` (the bottom bar's help area); returns the width used. See docs/components/HoldProgress.md.
int drawHoldProgress(Painter &p, const Theme &t, const Rect &area, const Hint &hint, float progress);
// Draws the hints inside `area`, sorted by key rank, dropping non-essential hints from the right
// until the line fits. Returns the width used.
int drawHints(Painter &p, const Theme &t, const Rect &area, std::vector<Hint> hints);

// ---- NavItem and FooterText: top-bar items and the static footer (docs/components/NavItem.md)
// One row of a nav item that lists several things, e.g. one per disk: "fat:   7% of 128GB". The three columns line
// up over all rows of the item: label left, value right (so the digits end in one line), note left.
struct NavRow {
    Text label, value, note;
    Tone tone = Tone::Neutral;   // Warning/Error colour the row; say it in the note too ("(!)"), never colour alone
};
struct NavItemSpec {
    Text primary, secondary;
    std::vector<NavRow> rows;    // below the primary line instead of `secondary`; rows that do not fit end in "..."
    IconId icon = IconId::None;
    Tone tone = Tone::Neutral;
    bool isClock = false;        // primary drawn in the Clock role
    int priority = 0;            // higher stays longer on a narrow screen
};
struct FooterTextSpec {
    Text text;
    Tone tone = Tone::Neutral;
    int priority = 0;
};

// ---- Shell: the main-screen layout (docs/components/Shell.md)
// How the tab row is drawn: names, icons (application-supplied pictures) or both. A user preference;
// when the bar is too narrow the tabs fall back to icons.
enum class TabDisplay { Names, Icons, Both };
struct TabSpec {
    Text label;
    IconPtr icon;                // optional; drawn in the tab's color
};
// A hold that is running: while active, the bottom bar shows only this (the hold hint with its bar) in
// place of the help line, so the progress is visible whatever the help contained.
struct HoldSpec {
    bool active = false;
    Hint hint;
    float progress = 0.0f;       // 0..1
};
struct ShellSpec {
    TabDisplay tabDisplay = TabDisplay::Names;
    std::vector<TabSpec> tabs;
    int activeTab = 0;
    bool tabsLocked = false;
    std::vector<NavItemSpec> left, right;
    std::vector<Hint> help;                  // always help, never anything else
    HoldSpec hold;                           // replaces the help line while a hold runs
    std::vector<FooterTextSpec> footer;      // static text only
};
// Draws only the bottom bar (the help on the left, the static footer on the right): a modal redraws it above
// its scrim so the help shown is the top modal's.
void drawShellBottomBar(Painter &p, const Theme &t, const ShellSpec &spec);
// Draws page background, top bar and bottom bar; returns the rect of the content window.
Rect drawShell(Painter &p, const Theme &t, const ShellSpec &spec);

// ---- EmptyState: why it is empty and what to do next, centered in `area` with no panel of its own.
// A page of its own (the faint panel, a tone icon, a Details line) is drawEmptyPage() in src/ui-framework/kit/EmptyPage.h; use that
// for a list, grid or table that has nothing to show.
void drawEmptyState(Painter &p, const Theme &t, const Rect &area, const std::string &title,
                    const std::string &why, const std::string &next);

// ---- CrashScreen: the failure takeover; no bars exist, so the one next step is a line inside the card
void drawCrashScreen(Painter &p, const Theme &t, const std::string &title, const std::string &what,
                     const std::string &changed, const std::string &details, const std::string &nextLine);

// ---- Splash: logo, one bar, optionally one calm status line
void drawSplash(Painter &p, const Theme &t, const Image *logo, float fraction, const std::string &status);

}  // namespace ui
