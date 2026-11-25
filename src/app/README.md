# src/app/: the new GUI on the UI framework

The application layer of the new GUI (shipping as 0.9.0, parallel to the Classic GUI in `src/legacy/`).
`src/ui-framework/` is the framework (generic components, theme, text catalog); `src/app/` knows the tabs, the texts and the
games library; `src/legacy/bridge/` is the host on the device and supplies the adapters. The three trees and their layering rules
are described in `src/README.md`.

```
src/ui-framework/     framework: components, Theme, Text catalog       knows nothing of src/app/ or src/legacy/
src/app/              this layer: tabs, Library interface, SampleLibrary   includes only src/ui-framework/ and src/app/
src/legacy/bridge/    host: PatsGuiScreen, AppLibraryAdapter, settings adapters   implements src/app/ interfaces on Classic code
```

Include names: `"app/Tab.h"`, `"ui-framework/kit/List.h"` (include root `src/`; the host builds here use `-I..`).

## Rules

- `src/app/` includes only `src/ui-framework/...` and `src/app/...` headers and the standard library. Never anything from `src/legacy/`.
- `src/ui-framework/` includes nothing from `src/app/` or `src/legacy/` and has no application concepts (games, systems, discs).
- Nothing blocks in `draw()` or `handle()`: no file or network access, no decoding, no scans, no sleeps.
  Ask the `Library` for data; slow things happen inside `Library::update()` in small steps.
- Pictures arrive late: `Art` is `{image, loading}`. Draw the placeholder while it is `nullptr`, ask again
  next frame. `revision()` changes when something arrived, so the host redraws.
- Every user-visible word is a `ui::Text::key("...")` registered once: words several tabs use in `src/app/Strings.cpp`, a
  tab's own words in its strings function (docs in `src/ui-framework/docs/I18N.md`). Placeholders are `{1}`, `{2}` filled with
  `ui::format()`. Library data (titles, system names) is `ui::Text::data(...)`.
- Colors, sizes and spacing come from `ui::Theme` (`t.px(...)`, `t.palette`); no literals, no light/dark
  checks (`src/ui-framework/docs/THEMING.md`).
- Controller hints use `ui::HintKey`; labels follow the user's button layout automatically.
  The layout (`ui::ButtonLayout`: Xbox, PlayStation, Nintendo) is an input of `ui::Theme`, set from `Preferences::faceButtonStyle()` by the look
  bridge (`LookSettings::buttonLayout`); `Hints` and the A glyph of action rows read it. A text that must name a button writes a placeholder
  (`{A}`, `{B}`, `{X}`, `{Y}`, `{START}`, `{SELECT}`, `{LB}`, `{RB}`, `{LT}`, `{RT}`), which labels and text blocks print in the user's layout
  (`ui::formatButtons`, `src/ui-framework/docs/components/Hints.md`); never spell a button letter in a text.

## The library interface (`src/app/Library.h`)

One pure-virtual class `patsgui::Library`, plain value types, called on the GUI thread only.

| Area | Calls |
| --- | --- |
| Frame | `update(dt)`, `revision()`, `refresh()`, `isSample()` |
| Systems | `systems()`, `setSystemHidden()`, `systemArt()` |
| Games of a system, or of all | `gameCount(scope, filter)`, `games(scope, offset, count, filter)`, `jumpLetter(scope, from, dir, filter)`, `gameArt()` |
| Favorites, recents | `favorites()`, `recents()`, `toggleFavorite()`, `removeRecent()` |
| Search | `setSearch(query, scope, owner)`, `searchState(owner)` (fills in over several frames; one per caller), `matchesSearch()` |
| One game | `detail()`, `launch()` (may answer `NeedsVersion`), `standingDown()` (a game or disc was started: the host is handing over to the core) |
| Arcade check | `arcadeScan()`, `startArcadeScan()` |
| Disc | `disc()`, `discArt()`, `discScreenshot()`, `startDisc()`, `rescanDisc()`, `reloadDiscArt()`, `cancelDiscLaunch()`, `discTabAvailable()` (the Disc tab is in the tab row) |

Notes:

- Ids are opaque. A game is `GameRef{system, id}`; do not parse either.
- Window access: ask `games(system, first, visibleCount + margin)` for what is on screen, never for the
  whole list. `gameCount()` gives the scroll range. `gameCount` of `SystemInfo` is `-1` until known.
- Scope: an EMPTY system id means all systems (every game of every system, sorted by title across systems, hidden
  systems included, like the Classic "all games" list). The first call for it builds the list (sorting tens of
  thousands of titles takes a moment on the device); later calls are cheap.
- Metadata: `GameInfo` carries `year`, `manufacturer`, `category` and the arcade check's result (`checked`,
  `works` = "might work", `coreFound`, `romFound`, `romCrcOk`), so a table row needs no `detail()` call.
  `GameDetail` adds `coreFile` and `romFile`. Only games the arcade check has seen have them (`checked`); the
  check never runs by itself (it walks the drives): the Arcade tab calls `startArcadeScan()` when the user asks and
  shows `arcadeScan()` (phase, progress, total, checked, working). Results appear as it finishes.
- `Works` filter (`All`, `Working`, `Broken`): narrows `gameCount/games/jumpLetter` by the check; an unchecked game is
  neither Working nor Broken.
- `systems()` includes hidden systems (flag set) and arcade (flag set); the tab decides what to show.
- Art sizes (`ArtSize::Small/Grid/Home/Detail`) are maximum boxes from `artBounds()`; pictures keep their
  proportions and are not enlarged. Only the Disc calls load anything for the disc, and the host fetches
  disc artwork only while a tab calls `disc()` / `discArt()` (so other tabs cost nothing).
- `launch()` on success makes the host hand the screen to the core; the tab does nothing more. On
  failure the host already shows the error dialog; `LaunchResult::error` is for the tab's own use.
- `DiscStatus::canStart()` says whether the start button is enabled; `startDisc()` runs the same re-check
  sequence as the Classic Disc screen and `launching` is true while it runs.

## Input, holds and going back

Events (`src/app/Tab.h`): `TabPrev/TabNext, Up, Down, Left, Right, Confirm, Back, X, Y, Start, XHold, BHold,
LetterPrev, LetterNext`. LetterPrev/Next are the trigger buttons (jump to a letter).

- **Holds** work like Classic: X held 2 s sends `XHold`, B held 2 s sends `BHold`. The host (`App` + `Input`) starts
  a hold only while the active tab's `favoriteKey()` / `removeKey()` is non-empty (the opaque key of the focused
  game), cancels it when the key changes, and re-checks the key before sending the event. Return an empty key
  while a dialog or the keyboard is open. A short X press is `Event::X`, a short B press `Event::Back`.
- B goes down: `Back` is sent at once, even if B is then held. A tab that returns a `removeKey()` (Recently played)
  must consume `Back` (return true) while it does, or the GUI closes before the hold completes: Home swallows
  the first Back on a recent game and the next Back (nothing in between) leaves.
- Progress is shown by the shell, not by tabs: while a hold runs the bottom bar shows "Hold X Favorite" (or the
  tab's own hold hint on that key) with a filling bar (`ui::drawHoldProgress`, docs in
  `src/ui-framework/docs/components/HoldProgress.md`). List the hold in `hints()` with `holdFavoriteHint()` ("Hold X Favorite")
  and `holdRemoveHint()` ("Hold B Remove"), or build your own `ui::Hint` with `hold = true` and the same key (the
  shell then uses your wording while it runs, e.g. to say "Remove favorite" for a game that already is one).
- Removing a favorite or a recent asks for confirmation: `ConfirmRemove` (below) opens the one question on `XHold`/`BHold`.
- The hold key has one format for every tab: `GameTab::gameKey(ref)` = `system + '\n' + id`.
- A hold is a shortcut, never the only way (src/ui-framework/docs/ACCESSIBILITY.md 2.2).
- **Going back to Systems:** when Systems opens a system (`takeOpenRequest()`), `PatsGui` remembers it
  (`State::backTo/backSystem`). An unused `Back` in the Games tab then returns to the Systems tab and calls
  `SystemsTab::focusSystem()`. Leaving the Games tab in any other way (tab buttons) forgets it and calls
  `GamesTab::openSystem(SystemId())`: an empty id means all systems. Opened directly, the Games tab shows all games.
  The same field serves the Arcade game status opened from Settings (`backTo = Tab::Settings`): closing the screen returns to Settings.
  Opened with X in the Arcade tab, closing it stays in the tab.

## The on-screen keyboard

Text is typed in `ui::TextEntryDialog`, which wraps `ui::Keyboard`, a port of Classic's `OnScreenKeyboard` (layout,
pages, navigation, Shift/Symbols/Show/Done keys, 64-character limit; see `src/ui-framework/docs/components/Keyboard.md`). A tab
fills a `ui::TextEntrySpec` (title, placeholder, value, `masked`, `live`, `maxLength`, `trim`, and for the search the
hint words `pressHint`/`doneHint`/`cancelHint`), keeps the dialog as a member while `typing_` is set, and maps its
events with the one helper `src/app/TextEntryInput.h`: `mapTextEntryEvent(Event, ui::TextEntryEvent &)`. The search tabs do not
use the dialog directly: `SearchHeader` (below) does. The mapping is the
same everywhere and is Classic's: D-pad moves, A presses, X deletes, Y inserts a space, Start is Done, B cancels;
every other event is swallowed while the dialog is open. Do not write a copy of the mapping in a tab.

## Select, Game options and the video preview

**Select** (`Event::Select`) opens the Game options of the focused game, as in the Classic GUI. Tabs with a keyboard
open ignore it; Start is its own event.

`GameOptionsView` (`src/app/GameOptionsView.h`) is the one panel for every tab: the picture (box art, or the video
preview in its place) with the preview's status line, the title, the available versions ("Selected" marks the one
Start launches; the game's own file is preselected), Start game, four screenshots and the picture viewer. It is
what Classic's panel contains (the Classic panel has no favorites, metadata or per-game settings). Start calls
`Library::launch(ref, versionId)`; a failed start shows the host's error dialog and the panel stays. A launch that
returns `NeedsVersion` is answered by opening the same panel for that game.

Integration in a tab (a handful of lines):

```cpp
GameOptionsView options_{library_};                       // member (after library_)

void onShow() override { options_.show(); }
void onHide() override { options_.hide(); }
bool update(float dt) override { return options_.update(dt) || /* the tab's own */ false; }
bool handle(Event e) override {
    if (options_.isOpen()) return options_.handle(e);     // it takes every event while open
    if (e == Event::Select && haveFocusedGame) { options_.open(focusedRef); return true; }
    /* ... */
}
std::vector<ui::Hint> hints() const override { if (options_.isOpen()) return options_.hints(); /* ... */ }
bool locked() const override { return options_.isOpen() || /* ... */; }
std::string favoriteKey() const override { if (options_.isOpen()) return {}; /* ... */ }
void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) override { /* ... */ options_.draw(p, t, p.bounds()); }
// where launch(ref) returned LaunchStatus::NeedsVersion:  options_.open(ref);
```

Home: the focused game of the active row is `focusedRef`. Favorites: the game under the cursor. Arcade: the
selected game. The shell draws the panel's hints in the bottom bar because the tab reports `locked()`; the host
calls `drawOverlays()` after `drawTab()`.

**Video preview** (`Library::focusPreview/preview/previewFrame`, wrapping Classic's `GamePreview`): the rules are
Classic's (setting on, login checked, system known to ScreenScraper, a stored "no video" answer shown at once,
countdown, lookup and download, playback, nothing blocks, every change of focus cancels it). `focusPreview(game,
slotW, slotH)` must be called **every frame** while a view shows the game; a frame without it stops the preview
(`PatsGuiScreen` does not need to drive `GamePreview::update`: `App` calls it once per frame after the screen's
update, as for the Classic screens). `slotW/slotH` are real pixels: a video larger than the slot is not played and
never scaled. `preview()` gives the status line and whether a video plays; `previewFrame()` is one `ui::Image`
the library reuses (the library bumps `revision()` per new frame, no per-frame allocation).

**Video frames repaint only the video.** A new decoded frame changes `Library::previewFrameSerial()` and does NOT
change `revision()` (status text, playing/not playing and a changed video size still do). The host then calls
`TabView::redrawPreview(p, t, damage)` on the last full frame: the tab forwards to `GameOptionsView::redrawPreview`
(modal open) or `ListDetail::redrawVideo` (list detail panel, through `PreviewSlot::redrawVideo`), which draws just the picture into the slot it
noted in the last full draw (`PreviewSlot::noteSlot(slot)` after `drawDetailPanel`/`DetailView::draw`) and returns
the damaged rectangle; `false` means "no video in the last draw, or an overlay is on top: redraw everything". A video
frame is drawn with square corners (`DetailSpec::artSquare`, `DetailSubject::pictureIsVideo`) so the partial and the
full result are identical. `app-render --partial N` checks that pixel for pixel.

`PreviewSlot` (`src/app/PreviewSlot.h`) wraps the calls for a view: `show()/hide()`, `update(gameOrEmpty)` every frame,
`setSlotSize()`, then `picture(game, size)` plus `status()`, and `noteSlot()` for the partial redraw. A tab does not
use it directly: `ListDetail` (List with details) and `GameOptionsView` own one, and size the slot from
`ui::detailPanelSlot()`, the framework's own slot rectangle (one computation).

## DetailView: one detail layout

`DetailView` (`src/app/DetailView.h`) is the single detail layout: the picture (box art or video) left with its status
line, and on the right the title, facts, an optional versions list, the Start button and a screenshot strip. It draws
in any rectangle and holds no data: the caller fills a `DetailSubject` each time it draws or handles an event
(`title`, `picture`/`pictureLoading`, `facts`, `warning`, `overview`, `versions`, `startLabel/startReady`,
`shotsPhase/shotCount/shot(i,size)`), the view keeps only the focus (Start button, versions, screenshots) and the
selected version, and `handle()` returns what a press means (`Start`, `ShowShot`, `VersionChosen`).
`GameOptionsView` is a modal frame around it (subject from `Library` for a `GameRef`; its public API is unchanged);
the Disc tab draws the same view inline (subject from `disc()/discArt()/discScreenshot()`) and keeps its own notice
pages for the states without a disc.

Screenshot rule, everywhere: only the thumbnails that exist are drawn (no empty frames for missing slots); a
thumbnail that has not arrived is a calm tile; while the lookup runs the strip says "Loading screenshots"; when it
found none, "No screenshots available". `ShotsPhase::None` hides the strip.

## Leaving a tab, and the B hint

- `TabView::onHide()` is called by the shell on the tab being left (also when the GUI closes). Stop what runs for the
  tab (the video preview, pending requests) and close its overlays. `onShow()` is its counterpart.
- The shell appends "B Back" to the help line on every tab except Home (B goes to Home; on Home B does nothing and
  the hint is not shown: the new GUI is not left with B). A tab that lists its own `ui::HintKey::B` hint (not a hold
  hint) in `hints()` replaces it, e.g. "B Back to Systems".

## The shared pieces: one thing, one component

The tabs were built one at a time and drifted apart; the consolidation (see `_vnext/vnext-0.9.0.md`) put every
repeated thing in one place. A tab supplies data and actions; it does not rebuild one of these.

| Piece | File | What it owns |
| --- | --- | --- |
| Shared words | `Strings.cpp` | The hint vocabulary ("Play", "Jump to letter", "Page", "Change view", "Edit search", "Clear search"), the view names ("Grid", "Small grid", "List with details"), the `n of total` header info, the search placeholders, the removal questions, the picture placeholders. A tab registers only words that are its own. |
| Format | `src/ui-framework/kit/Text.h` | `ui::format(key, {args})`, `ui::plural(key, n, {extras})`, `ui::position(at, total, capped)`: one placeholder syntax, `{1}`, `{2}`. No local fill helpers (`src/ui-framework/docs/I18N.md`). |
| Hints | `ui::hint(...)` in `src/ui-framework/kit/Components.h`, `Hints.h` | `hints::browseGames(Browse)` (a game has the focus: grid, small grid or list) and `hints::searchField(hasQuery, changeView)`; the keyboard, modal and Game options hints come from their components. |
| Grid page | `GridPage.h` | The tiles under a `PageHeader`, the same on every tab: tile size, caption, gaps, clip and "peek" row, focused tile drawn last, small grid with the focused title below, row-to-top after a jump, picture placeholder, the one-row strip (Home's recently played). `ViewMode` (Grid, Small, List) and the header info `gridInfo()`. |
| Search | `SearchHeader.h` | The search field, the keyboard (`ui::TextEntryDialog`), the query, and the Library search that belongs to it. |
| List with details | `ListDetail.h` | The rows, the `ui::DetailPanel` of the focused game, the `PreviewSlot` (video, else cover, status line) and the one facts line `system · year · players` (only the parts that are known). Left/Right turn a page of the visible rows, Up on the first row leaves the list, the row of a jumped-to letter goes to the top. Used by Home's results, Games, Arcade, Favorites. |
| Removal question | `ConfirmRemove.h` | The one "Remove from favorites?" / "Remove from Recently played?" dialog (words in `Strings.cpp`). It remembers the game it was asked about, never the cursor. `ModalInput.h`: `toModalEvent(Event, ModalEvent&)` is the one mapping from the shell's events to a modal; no tab writes its own switch. |
| Notice (toast) | `Notice.h`, `ui::Toast` | `Services::notice` is the one sink a page calls ("X added to favorites", "X removed from favorites", "X removed from Recently played", "Games starting with S"; `noteFavorite()`, `noteRemovedRecent()`, `LetterJump::announce()`). The shell (`PatsGui`) owns one `ui::Toast` and draws it above the bottom bar, right-aligned, over the page and never over a dialog or a running hold; it stays 3.5 s (+1 s per further 40 characters), takes no focus and never carries the only copy of something the user must act on. |
| Letter jump | `LetterJump.h` | Classic's `Alphabet` rule (first character, non-letters one group "0-9", back goes to the top of the group first), with the same notice, on Home, Favorites, Games, Arcade and the Arcade status table. `Library::jumpLetter` answers with the same rule (the adapter calls `Alphabet` itself). |
| Game tab base | `GameTab.h` | What Home, Games and Favorites share: the overlays (Game options, removal question, keyboard: `overlayOpen()`, `handleOverlay()`, `overlayHints()`, `drawOverlays()`), `locked()`, the hold keys, `play()` (a version choice opens the Game options), `requestToggleFavorite()`, `applyRemoval()`, the preview glue (`updateGlue()`, `redrawPreview()`), `onShow/onHide`. A tab says only `focusedGame()`, `libraryChanged()` and `listShowing()`; each keeps its own focus model (Home has four zones, the others two). Arcade composes `GamesTab`. |
| Empty page | `ui::drawEmptyPage` (`EmptyPage.h`), `EmptyPages.h` | The one frame for "nothing to show": the faint panel with why and what to do next, optionally a tone icon, a Details line and one more small line (Disc's notices). `searchEmptyPage()` is the shared "Searching..." / "No games found". Systems, Arcade, Home, Favorites, Games, Disc all use it. |

**GridPage rules** (owner decisions, 2026-11-23): the tile's second line (the system) is drawn only where the list
mixes systems (Home, Favorites, All games, a search over all games; not inside one system); the favorite mark is left
off only where every tile is a favorite (Favorites, Home's favorites); Systems says "No core" on tiles without a core;
a list page always draws its header, also when empty; List with details pages by the visible rows (`pageStep()`).
Each tab keeps its own cursor and items:

```cpp
GridPage grid_;                                              // member
GridPage::Options o;  o.secondLine = system_.empty();
grid_.draw(p, t, body, small, count, cursor_, focused, o,
           [&](int i, bool onCursor, GridTile &tile) { /* label, sublabel, favorite, art */ return true; },
           [&](int first, int last) { /* fetch the window */ });
```

**SearchHeader rules**: the search runs when the user presses Done on the keyboard, never per key (D2); B on the field
clears a query, the next B leaves (D3); Start opens the search from anywhere on the tab, A while the field has the
cursor (D4); each header has its own Library search (D8: `Library::setSearch(query, scope, owner)`), so Home, Games and
Favorites never replace each other's answer; the placeholder says what is searched (D14). A tab calls
`search_.handle(event, cursorIsOnField, out)` after its overlays, and `search_.fill(header, focused)` while drawing. A list
the tab holds itself (the favorites) filters with `search_.matches(game)` = `Library::matchesSearch`, the library's own
rule, not a copy.

## Parity with the Classic GUI: the rules the PARITY files share

Each tab has a table of Classic's behavior against the new tab's (`PARITY-home.md`, `-favorites.md`, `-systems.md`, `-games.md`,
`-arcade.md`, `-disc.md`; Settings has `PARITY-settings.md` with the row checks and the inventory of every Classic sub-screen). A row is
*Classic behavior | new tab | status*; the status words are **done** (same behavior), **different** or **intentional** (on purpose, with
the reason), **added** (new, no Classic equivalent), **shell** (belongs to `PatsGui`/the host, not the tab) and **open**/**not possible yet**
(says what is missing). The files keep what is specific to a tab; these rules hold for every tab, so a file does not repeat them:

- **One rule on every tab.** Left/Right turn a page of the visible rows (D10), Start opens the search and A plays (D4), the first B in
  the search field clears the query and the next leaves (D3), Y changes the view, L2/R2 jump to a letter, the view name is in the header
  (so no "Game view: X" toast), the header is always drawn (D16), an empty page says why and what to do next.
- **Removing asks, adding does not.** Hold X (2 s) adds a favorite and says so with a notice; removing a favorite or a recent (hold X on a
  favorite, hold B in Recently played) asks first (`ConfirmRemove`) and then says what happened.
- **Nothing to read before the screen changes:** no "Starting X" notice (the host takes over at once); a launch error is the host's
  error dialog.
- **Toasts never carry what must be read or acted on.** Where Classic used a toast for that (the Disc tab's blocked Start, a failure), the
  new tab shows a state or a dialog that stays until read (ACCESSIBILITY 2.4).
- **Framework look instead of Classic's decoration:** a border for focus (no focus growth, no fade-in, no animation), left-aligned grids,
  framework tables and lists, flags as glyphs or words (never color alone).
- **Bounded work:** search results show the first 500 matches in title order (Classic lists all; the Library keeps the sorted first 500 of
  every match and `SearchState::more` shows "500+" only when more exist); nothing blocks in `draw()`/`handle()`; the Library groups game
  versions, so a row is a title and the version choice opens the Game options.
- **Entries outside the tab:** preferences (Show Arcade tab, Show Games tab, Show Arcade in Consoles) and the flows behind Settings rows live in
  the Settings tab; the shell follows them (`State::hidden[]`).

### Verified against Classic code on the host (2026-11-23)

The tabs were compared with the real Classic screens (`HomeScreen`, `GamesScreen`, `ArcadeGamesScreen`, `DiscScreen`, `SystemsScreen`, `App`
rules) compiled on the host, on generated game libraries (three seeds), through the real `AppLibraryAdapter`: `t1_systems`, `t2_lists`,
`t3_letters`, `t4_favs`, `t4b_flows`, `t5_launch`, `t6_arcade`, `t7_disc`, `t7b_discseq`, `t8_tabs`, `t9_hints`, `t10_options`, `t11_misc`.
What still differs on purpose is listed in the tabs' PARITY files (Arcade: grouped rows; every tab: the framework and owner decisions above).
The tests are throwaway scripts outside the repository; a row that says **done** means the test above found no difference.

## Tabs

A tab is a `patsgui::TabView` (`src/app/Tab.h`): `update`, `onShow`, `draw`, `handle`, `hints`. It receives
`Services&` (the `Library`, the Settings services and the `Notice` sink) in its constructor and owns its own state (cursor, scroll,
dialogs). Back that the tab does not use goes to Home (on Home it does nothing); Tab-prev/next that it does not use switch tabs.

To add a tab (Disc, Home, Systems, ...) create `src/app/MyTab.cpp` (and `.h` if wanted) only; no shared file
changes. The root `Makefile` and `src/app/Makefile` pick up `src/app/*.cpp`.

```cpp
#include "app/Tab.h"
#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace {

class DiscTab : public TabView {
public:
    explicit DiscTab(Services &s) : library_(s.library) {}
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override {
        const DiscStatus disc = library_.disc();
        // ... components from src/ui-framework/kit, text via ui::Text::key / ui::Text::data
    }
    bool handle(Event e) override { /* return true when used */ return false; }
    std::vector<ui::Hint> hints() const override { return {}; }
private:
    Library &library_;
};

void strings(ui::Catalog &c) { c.set("disc.title", "Physical disc"); }

}  // namespace
PATSGUI_REGISTER_TAB(Tab::Disc, DiscTab, strings);
}  // namespace patsgui
```

The registration runs before `main()`. A tab with no registered view shows the placeholder. Tab
strings are registered by the `strings` function (may be `nullptr`).

## Render and test on the host (no device)

```sh
make -C src/app run                                  # build/out/app-<tab>.png for every tab, 1080p
make -C src/app run ARGS="--tab disc --disc nodisc"  # one tab; disc state nodrive|nodisc|checking|readable|unsupported|unreadable
make -C src/app run ARGS="--height 720"
make -C src/app run ARGS="--tab home --hold favorite"   # the bottom bar with a running hold (or --hold remove)
```

The host runner (`src/app/host/main.cpp`) uses `SampleLibrary` ("Example system A..H", "Sample title 0001",
generated art, sample favorites/recents/disc). Fonts: Arial on macOS, DejaVu on Linux, or
`UI_FONT_REGULAR` / `UI_FONT_BOLD`. For states that need input, drive a `TabView` through `handle()` in the
runner before drawing, as `src/ui-framework/demo/main.cpp` does for its dialogs. `make -C src/ui-framework run` still builds the
framework demo.

`SampleLibrary` has test hooks (`setDisc`, `setFavorite`); add more there as tabs need states
(empty library, a failing launch).

Golden images and layout rules: `make -C src/app golden-check` renders every Settings page and the main tabs (plus a matrix of
pseudo-language, Largest text, light and high contrast), compares them pixel by pixel with `tests/golden/` and checks the text
layout (overlap, cut, elided); `make -C src/app golden-update` accepts intended changes. Details in `tests/README.md`.

Flow tests (behaviour, no pixels): `make -C src/app test` drives every converted Settings flow with scripted key sequences against the
sample services and asserts what happens (which page, which hints, which service call, how often), plus generic rules over every
page and a seeded random walk over every flow (B always leaves, hints from the closed vocabulary, Result pages complete, the Wi-Fi
rollback exactly once, the raw-input flag follows the capture page). `TARGS="wifi scan" make -C src/app test` runs only the cases
whose name contains a word. A new flow or a changed flow needs a case there: how to add one is in `tests/README.md`.

## Rendering and damage

The device has no GPU: a 1080p frame is 2 M pixels of software drawing, and copying it twice (onto Classic's canvas, then to the
framebuffer) costs about as much as drawing it. So a frame never repaints, copies or presents more than what changed.

**How (`src/ui-framework/render/soft/RetainedFrame.*`).** Pages and components draw as always through `ui::Painter`; nothing in
`src/app/` or the kit knows about damage. The host (`PatsGuiScreen::paintFrame`, `app-render --bench`) hands them a `ui::RetainedFrame`
instead of a `SoftPainter`. It records the frame's draw calls (a display list; layout, text measuring and eliding still run, pixels
do not) and keeps, per 32 px tile, one hash over the ordered calls that can paint into it (kind, geometry, colour, text, font size,
clip, image identity). `commit()` compares the tile hashes with the previous frame, merges the changed tiles into disjoint
rectangles and replays every call that touches them, clipped to the rectangle (so alpha scrims, shadows and gradients are applied
exactly once per pixel). The rectangles are the damage: `PatsGuiScreen` copies only them onto App's canvas (`blitRect`), so App
presents only them to the framebuffer. This is the retained layer for free: a dialog, the keyboard or the Input test changing does
not repaint the page behind or the scrim (their calls are unchanged), and a list cursor move repaints only the two rows.
Pixels are identical to a full repaint (`golden-check` and `tests/DamageTests.cpp` guard that).

**What invalidates what.**
- Any change in a call's parameters (position, colour, text, size, clip, image) changes the hash of the tiles it touches: those tiles
  repaint. Moving content, animations, a changed value, a new toast, a theme or colour change are therefore covered without code.
- `RetainedFrame::invalidate()` repaints everything on the next frame: the bridge calls it for a look or font change (`syncLook`), at
  `open()`, and when App asks for a full redraw (`renderFull(canvas, true)`: the error dialog or the screensaver drew over the canvas).
  A resize builds a new frame (`ensureResources`).
- Not visible to the hash, so the caller must handle it: pixels drawn outside the painter on the same canvas and images whose pixels change
  in place (images are keyed by `Image::id()`). The video picture is the one case: it is still repainted on its own
  (`redrawPreview`, see "Select, Game options and the video preview") on top of the repainted tiles, and everything is repainted when
  that is not possible (a toast over the video).
- Stateful drawing code is fine as long as it draws through the painter every frame (it does: the display list is rebuilt every dirty frame).

**Measure.** `app-render --tab T [--category C --page P] --keys k1,k2,... --bench N` replays the keys N times and prints, per key, the
cost of a full repaint (BEFORE: ms on this machine, pixels touched = filled + blended + copied, glyphs) next to the damage path (AFTER:
record + repaint ms, pixels, glyphs, share of the screen repainted, draw calls, full repaints) and checks every frame pixel for pixel.
The pixel and glyph counts do not depend on the machine (`ui::Canvas::setWork`); the milliseconds are the Mac's, the A9 is much slower.
On the device `PatsGuiScreen` writes one line per ~5 s window that saw key presses to `/media/fat/mister-pat/logs/debug.log`:
`pats-gui input: N keys, M redraws (K whole screen, repainted X % ...); draw a/m ms avg/max, paint, copy, present; key to screen a/m ms;
sounds N calls T ms ...`. Read "key to screen" first (handle() to the presented frame), then which of draw / paint / copy / present is largest.

**Tests.** `tests/DamageTests.cpp` renders a scenario through both paths after every step and compares pixels: keyboard moves and typing,
the Input test, Settings list and tab switches, Grid cursor, dialogs opening and closing, the page behind a dialog changing (Busy clock,
shell clock, disk usage, network, toast), look changes (text size, contrast, theme, font face) and two window sizes. It also asserts
that an idle frame repaints 0 % and a keyboard move less than 5 %.

## Settings tab: categories, pages and services

The Settings tab is the Classic Settings screen, ported 1:1 category by category (charter: `src/app/settings/PORTING.md`).
`src/app/SettingsTab.*` only hosts: three panes (categories, rows, help), a `PageStack` and the seven categories; it knows no row and no service.

```
src/app/SettingsMenu.*            row model (MenuRow, Cond, MenuState), table builders (namespace menu), visibleRows()
src/app/SettingsTab.*             the 3 panes, the PageStack; hosts the categories
src/app/settings/SettingsCategory.*   interface SettingsCategory, SettingsEnv, registry, PATSGUI_REGISTER_SETTINGS_CATEGORY
src/app/settings/SettingsPage.h, PageStack.cpp   SettingsPage (Content or Modal), PageStack
src/app/settings/StandardPages.*  ChoicePage, ConfirmPage, ResultPage, ProgressPage, TextInputPage, InfoPage (src/ui-framework/kit only)
src/app/settings/SettingsServices.h + <Category>Service.h   the services seam: one abstract interface per category, null = unavailable
src/app/settings/<Category>Settings.*  Library Controllers Interface Accessibility Updates System Developer: table, rows, behavior, pages
src/app/settings/Sample*.h, SampleSettingsServices.*  fake services with settable state machines (host runner)
src/legacy/bridge/settings/                 host side: App<Category>Settings (the adapters on Classic objects), SettingsAdapters, AdapterEnv
```

**Rows.** Each category owns its table (label key `settings.<rowId>`, help `settings.<rowId>.help`, kind `Action/Switch/Value/Info/Heading`, option
list, `show`/`enabled` conditions evaluated against `MenuState`) and answers `rows(state)`. Switches and cycling choices keep their value in memory
unless the category's `current(rowId)` supplies the real one; `changed(rowId, value)` writes it through the service; `activate(rowId, pages)` runs
or opens a page. `SettingsTab::setState(MenuState)` and the empty first-level seam `SettingsActions` (`activate` / `changed`, reached only for what a
category does not handle) still work for a host without services. Hints are Classic's fixed line (Move, A Choose, Change column, B Back); a
category may replace it (`hintsHook`, Updates does while a download runs) or claim B (`backActivates`).

**Pages.** A row opens a `SettingsPage` on the tab's `PageStack`: it owns every event while open, draws in the content rectangle (`Content`) or as
a dialog over the panes (`Modal`), names its own hints, locks the tabs and is closed by Back unless it uses Back. Built from framework
components only: `ChoicePage` (`ui::Modal::choose`), `ConfirmPage` (`Modal::confirm`), `ResultPage` (`Modal::info/result/error`), `ProgressPage`
(`ProgressPanel`), `TextInputPage` (`ui::TextEntryDialog`), `InfoPage` (`KeyValueList` or a scrolling text viewer). A flow of several steps is a `WizardFlowPage` (a wizard), not a chain of these pages.
`PageStack::toast()` is Classic's `notify()`: it forwards to `Services::notice`, the shell's one `ui::Toast` (the page draws nothing). Classic's seconds are kept as a minimum (`Notice::toast(..., minSeconds)`); the shell holds the toast back while a Modal page is open (`TabView::dialogOpen()`), not over a Content page. The pages' own modal mapping is `toModalEvent(event, me, true)` from `src/app/ModalInput.h` (Start acts like A there).

**Services.** `Services::settings` (`SettingsServices*`, null = nothing available) holds one interface per category; `PatsGuiScreen` creates the
adapters (`src/legacy/bridge/settings/`) and sets it, the host runner sets the sample services. A category uses `usable(env().services.xService)` and falls back to
the plain first level when it is null or `available()` is false (a host without the object behind it). The worked examples are *Organize game-versions* (preference row, `LibrarySettings`), *Reboot MiSTer* (page, `SystemSettings`) and the update
phase polling (`UpdatesSettings::update`).

**What Classic's `App` does on every tab** (the update snapshot is polled every frame; the restart prompt switches to Settings; the bottom bar names an
available update): a category may implement `background(dt)` (runs while the Settings tab is hidden), `wantsAttention()` (the shell then shows the tab:
`TabView::wantsAttention()` is checked in `patsgui::update`) and `footerNote()` (a text the shell adds to the footer: `TabView::footerNote()`). Updates
uses all three; its two dialogs (release notes, restart prompt) are functions of the update snapshot, opened and closed from it (`UpdatesSettings::syncPages`,
`src/app/settings/updates/UpdatePages.*`). Hosted by Classic, `App` still owns the startup auto check and Back-cancels-download on every tab.

Registering a category is one line in its `.cpp`, `PATSGUI_REGISTER_SETTINGS_CATEGORY(Class, stringsFn)`, so finishing a category touches only its own
files; the Makefiles build `src/app/settings/*.cpp`, `src/app/settings/*/*.cpp` and `src/legacy/bridge/settings/*.cpp`.

**Interface settings reach the shell** (`patsgui::State`, `src/app/PatsGui.*`): the tab row follows `InterfaceService::gamesTabVisible()` / `arcadeTabVisible()` every frame
(`State::hidden[]`; `visibleTabs()`, `highlightedTab()`), LB/RB cycle over the visible tabs, `resolveStartTab()` picks the tab the GUI opens on when
`State::useStartTab` is set (`PatsGuiScreen::open()`), and the shell plays the tab-change sound through `InterfaceService::playSound()`;
`State::ownsSounds` (false while hosted by Classic's `App`, which plays Navigation, launch and start itself) makes it play Navigation, start and
the launch sound too (the launch sound plays once, from `Library::standingDown()`, in `update()`). The Disc tab follows `Library::discTabAvailable()` the same way
(`State::hidden[]`; the active Disc tab gives way to Home when it turns false). Settings > Library > Arcade game status opens the Arcade tab's status screen
(`Services::openArcadeStatus`); closing it returns to Settings (`State::backTo = Tab::Settings` while it is open).
Host runner: `--start` opens the Start tab, `--own-sounds`, and `--set interface.<key>=<value>` (`--list-samples`: tabs, arcade games, theme index 0..21, dark,
box art, sounds with `interface.soundlog=1`, game view, systems). The color theme and dark mode are applied by the look bridge (`src/app/Look.*`); the framework
carries Classic's 22 color themes in Classic's order (`src/ui-framework/kit/Palettes.cpp`).

Host test options: `--tab settings [--category ID] [--page [CATEGORY/]PAGE] [--set category.key=value ...] [--keys ...] [--after N] [--list-pages] [--list-samples]`
(`--page standard/choice` shows each helper page). See `src/app/PARITY-settings.md` for the row checks, the inventory of every Classic sub-screen and its owner.

## First start: Welcome, Jump Start, overview (`FirstRun.*`)

The first-run guide that Classic shows as `ScanScreen` pages (Welcome, JumpStart, the jump-start Working and ArtworkWorking, Overview,
and the failed Done page) is a wizard flow of one Medium frame in the new GUI: `src/app/FirstRun.{h,cpp}`. It is not a Settings page.

- **Hosting.** `State::firstRun` (a `FirstRun`: a `PageStack` with one `WizardFlowPage`) is drawn over the shell by `drawTab` (the wizard dims
  the whole screen itself), takes every event in `handle()` before anything else, supplies the hints in `shellFor` (the whole help line; tabs
  locked, no "change tab"), is stepped in `update()` (it steps the Library service first, as the Settings tab does while shown), and
  keeps the hold keys, the toast and the video repaint quiet while it is open. `startFirstRun(state)` opens it; the host calls it once
  after `attach()` when the guide is not complete: `App::init` -> `PatsGuiScreen::startFirstRun()`. When the guide ends, the screen reopens
  (`PatsGuiScreen::update`) because the Jump Start has just built the library the first open did not have. `--classic` (and the other Classic
  options) still open Classic's own `ScanScreen` pages: `App::init` decides with `startNewGui`.
- **Pages.** Welcome (centred: headline, two sentences; A Continue, B Skip) - Jump Start offer (centred; A Start, B Skip) - Assets (one Busy page, only when something is missing: "Getting the font" first - Space Grotesk and its licence, enabled by App when it arrived and applied by `PatsGuiScreen::syncLook` - then "Getting icons" - system and tab icons, both through the existing Accessibility / System services; B Cancel leaves the step, a failure never stops Jump Start and adds a line to the Overview) - Working (the
  scan's Busy page of "Build game list", `library::addScanWorking`) - Covers (the box-art Busy page, `library::addArtworkWorking`) -
  Overview (the shared Result layout: what was found, Classic's three "When you're ready" rows, A/B Done) - Failed (Result with Try again /
  Close, only after a failed or stopped scan). No games: the Overview says so and offers the same rows. One implementation per concept: the
  working pages are the ones of the Library flows, only their title and the next page are parameters.
- **Rules (Classic's).** Skip on Welcome or the offer, Close on the failed page and A/B on the Overview mark the guide complete
  (`OnboardingService::setComplete`, exactly once) and call `libraryTaskClosed()` (App::closeScan). A GUI that closes in the middle
  completes nothing and stops a running scan or cover job (the guide starts again at the next start). The quick scan is
  `LibraryService::startJumpStart()` (20 games per system, 20 Arcade; `scanWatched_` reloads the library when it ended well); after it,
  games found start `startJumpStartArtwork()` (boxart only, no backgrounds), no games go straight to the Overview.
- **Seams.** `LibraryService::startJumpStart / startJumpStartArtwork` (additive), `OnboardingService` (`complete()`, `setComplete()`; bridge
  `AppOnboardingSettings`, sample `SampleOnboardingService`, `--set onboarding.complete=0|1`). App still marks the guide *started* before it opens.
- **Host runner / tests.** `app-render --first-run PAGE` (`--list-first-run`): welcome, offer, working, covers, failed, stopped, overview,
  overview.nogames, overview.nocovers, overview.noassets, assets (the font), assets.icons (golden: `first-*`; sample states in `tests/golden.sh`). `tests/FirstRunTests.cpp` (full path, skips, B at
  every step, no games, failure and retry, covers failing, GUI closing, nothing underneath reacts, every page) and the flow `first.run` in
  `Flows.cpp` (generic invariants and the random walk; the `Rig::openFirstRun()` rig drives the shell's `State`).

## On the device

Settings -> Developer -> MiSTer Pats GUI (UI-Framework). `src/legacy/bridge/PatsGuiScreen` opens the GUI with
`AppLibraryAdapter` (Classic `Library`, `Favorites`, `History`, `HiddenSystems`, `ImageCache`, disc services)
when the Classic library has systems, and with `SampleLibrary` otherwise. Classic code paths are untouched;
the plumbing is `Context::libraryRevision`, bumped by `App::reloadLibrary()`, and `Context::settingsHost` (the App objects and operations the Settings adapters use,
filled in `App.cpp` next to `Context::input`).
