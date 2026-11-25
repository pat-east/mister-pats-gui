# Games tab: parity with the Classic GamesScreen

Legacy sources read: `GamesScreen`, `GamePreview`, `PreviewDisplay`, `GameSearch`, `GridView`, `Tile`, `Alphabet`,
`GameScreenshotService`, `App` (Game options panel, dispatch), `AppLibraryAdapter`. Status: done / intentional
(different on purpose) / needs shell or framework.

| Classic behavior | New tab | Status |
| --- | --- | --- |
| Three views (Grid, Boxart small, List), Y cycles | "Grid", "Small grid", "List with details" (D11), Y cycles | done |
| Remembered view (shared default or "individual" per tab: all games / one system) | `Library::gamesView/setGamesView` (adapter uses Preferences) | done |
| Header: title, `n / total · view` | `ui::drawPageHeader`: scope ("All games" or the system) at the left, the shared search field in the centre, `view · n of total` at the right | done |
| All games sorted by title across systems; per-system list | empty `SystemId` = all; `openSystem(id)` | done |
| Windowed access for 10k+ libraries, lazy art | `games(offset,count)` window, art asked for visible tiles only | done |
| Tiles: art, caption (grid), favorite mark, placeholder | `GridPage` tile: Cover/Placeholder, favorite mark; the system under the title in "All games" and the search over all games, not inside one system (D6) | done |
| Box art setting off: no art | adapter `gameArt` returns nothing when the setting is off | done (adapter) |
| List: 52 px rows, favorite `*`, detail panel at the right | List with dense rows, DetailPanel | done |
| List Left/Right move 10 | one page: the visible rows, like every list (hint "Page", D10) | intentional (owner: one rule on every tab) |
| Detail: cover, "Loading cover art", title when no art | `ListDetail`: art, framework placeholder; facts line "system · year · players" (only the parts that are known; rating and versions are in the Game options), the same on every tab | done |
| Video preview in the List detail (ScreenScraper, countdown, status line) | `focusPreview/preview/previewFrame`; frame replaces the cover, status line under it | done (Library, adapter, tab) |
| Preview stops on launch, other tab, dialog | adapter stops on launch; tab stops asking when not drawn or a dialog is open | done |
| Letter jump L2/R2, letter at the top, toast "Games starting with W"; also inside search results (over the result list) | `LetterPrev/Next` through `LetterJump` (the library answers with Classic's `Alphabet` rule; inside results the tab jumps over its result list), row put at the top, toast "Games starting with W"; the hint is shown whenever there is more than one game listed, results included | done (`t11_misc`: 14 cursor sequences equal, in all games, SNES and the Arcade list) |
| Search field above the grid, Up from the top row, Down back | same (`SearchHeader`); the placeholder says what is searched: "Search all games" or "Search {system}" (D14) | done |
| Keyboard: max 64 chars, X delete, Y space, Start searches, B closes without change, whitespace trimmed | `ui::TextEntryDialog` via `SearchHeader` (the search runs on Done; maxLength 64, trim) | done |
| Search rules (word prefixes, all terms, system name when searching all games), empty message | adapter reuses `GameSearch::matches`; empty text with the same meaning | done |
| Search results unlimited | the first 500 matches in title order (the adapter walks all games within the per-frame budget and keeps the sorted first 500), the count shows "500+" only when more exist | intentional (bounded work); the 500 shown are Classic's first 500 (`t2_lists`) |
| B in the focused search field clears the query | B clears it (the tab lists "B Clear search" in place of the shell's "Leave"; the next B leaves) (D3) | done |
| A = launch; Start = launch (except in search) | A launches ("Play"); Start opens the search keyboard on every tab (D4) | intentional (owner decision) |
| Launch: recent version remembered, else the Game options open; error dialog; history; "Starting X" toast (the Game options' Start has no toast) | `launch()` -> NeedsVersion opens the Game options; host shows errors; no toast (the screen changes at once; the adapter posted one until this was fixed) | done / toast intentional |
| Favorite: hold X 2 s, toast, removes every version | hold X (`favoriteKey`, hold hint "Favorite" / "Remove favorite"); adding shows the notice "X added to favorites", removal asks first (`ConfirmRemove`) and then says "X removed from favorites" | intentional (owner: confirm when removing; no short X) |
| Select opens Game options for the focused game (any view) | `Event::Select` -> `GamesTab::openGameOptions()` | done |
| Game options: art or video, title, "AVAILABLE ROMS" list with Selected, Start game, 4 screenshots, viewer, hints | same layout from framework parts (ListItem, ActionButton, Modal viewer); the heading says "Available ROMs" (sentence case is the framework rule; it said "Available versions" until this was fixed) | done |
| Game options selected version = the game's own path; versions sorted by label; "Standard" when single | same | done |
| Screenshots from TheGamesDB, fallback to the game's background picture | `gameScreenshots/gameScreenshot` (adapter wraps `GameScreenshotService`) | done |
| Hints text per state | per state, one word per action | done (wording differs, same meaning) |
| Empty states: nothing indexed, no games for the system, no search results | the same three, with why and next step | done |
| Grid centered, focus growth animation, fade-in of covers | left aligned, border focus, no growth, no fade (framework rules) | intentional |
| Game-view toast "Game view: X" | the header shows the view | intentional |
| Hold progress bar | shown by the shell | done (shell) |

Verified against Classic code on the host (2026-11-23): the real `GamesScreen` and `App::openGameActions` rules against the adapter and `GamesTab`
on generated libraries (three seeds): `t2_lists` (lists, jump positions, search incl. the first-500 rule), `t3_letters`, `t5_launch` (A, version
choice, history), `t10_options` (version list), `t11_misc` (letter jump in results, view preferences, bounded search step), `t9_hints`.
