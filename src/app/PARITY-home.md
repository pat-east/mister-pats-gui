# Home tab: parity with the Classic Home screen

Classic: `src/legacy/HomeScreen.*`, `src/legacy/GameSearch.*`, `src/legacy/App.cpp`. New: `src/app/HomeTab.cpp`.
Status: done / different (intentional, with the reason) / open (needs shell, framework or adapter work).

| # | Classic behavior | New tab | Status |
| --- | --- | --- | --- |
| 1 | "Recently played" strip (newest first), "Favorites" grid (alphabetical), versions of a game shown once | Same two sections, built by Classic's own rules in the adapter (`recents()`, `homeFavorites()`; a port of `HomeScreen::refresh()`): with "Organize game versions" on, the games of one group are ONE tile (the newest history entry), titled by the group and carrying the group's representative path; off, every entry is its own tile titled by its file name. No check that the file still exists (a deleted game keeps its tile, and A reports the launch error, as in Classic); system files and unknown systems are skipped, and so are favorites with an empty system. Favorites are sorted by title. The tiles are `GridPage` tiles like every other tab | done (checked against the Classic code, `t4_favs`: 114 same, 0 differ) |
| 2 | Tile: box art, title, system name, favorite mark | Same (`GridItem`, art `Home` size; the system line because Home mixes systems, D6; the favorites grid has no favorite mark, D7) | done |
| 3 | Section titles with the number of games | Title and count; the focused section shows "n of total" | done |
| 4 | Empty hints per section ("Nothing started yet", "Hold X ... to mark it") | One-line hints per section; a centered empty state when both are empty. The hint avoids button names: the help bar names them | different (framework empty-state rule) |
| 5 | Search field above the sections; Up from the first row focuses it | The shared page header (title left, field centered, info right); Up/Down move between field and content | done |
| 6 | A or Start opens the on-screen keyboard; Start submits; B closes; X delete, Y space | The shared `TextEntryDialog` through `SearchHeader` (the search runs when the user presses Done, not on every key; 64 characters, trimmed); B closes it and changes nothing. Start opens the search from anywhere on the tab (D2, D4) | done (D2: Classic searches on Done too) |
| 7 | B clears an active search while the field has the focus | Same (`SearchHeader`, D3); the help lists "B Clear search" instead of the shell's "Leave"; the next B goes to Home (on Home nothing) | done |
| 8 | Search over all visible systems, title and system name, multi-term | `SearchHeader::run` = `Library::setSearch(query, all, owner)`; results fill in over several frames; "Searching..." state; the search belongs to this tab (D8) | done |
| 9 | Results view (Grid, Grid small, Grid list), Y changes it; the list view shows the focused game's video preview | Same three views ("Grid", "Small grid", "List with details", D11), Y cycles while there are results; the list's detail panel uses `PreviewSlot` (video frame, status line, else cover art) | done |
| 10 | The chosen view is remembered (Preferences home view) | `Library::homeView()/setHomeView()` (adapter: Preferences home view, same rule as the others) (D15) | done |
| 11 | No results / "Preparing matching games..." | "Searching..." and "No games found" empty states | done |
| 12 | A plays; several versions: ask which, remember the last | `Library::launch`; on `NeedsVersion` the shared Game options open; remembering is the adapter's job. A tile of a game with versions starts the most recently played version (Classic `HomeScreen::launch`), not the file of the history line it came from | done (`t5_launch` L12) |
| 12b | Select opens the Game options of the focused game | Select on any game (recents, favorites, results) opens `GameOptionsView` | done |
| 13 | Launch failure shows an error dialog | The host shows it | done (host) |
| 14 | Hold X 2 s toggles favorite (no confirm) | Hold X only (no short press on a game) adds; removing asks first | different (owner decision, phase 3) |
| 15 | Remove a recent (no confirm); removes the tile's path and every version of the game | Hold B, asks first; then removes every version, as Classic (`removeRecent` in the adapter) | different (owner decision, phase 3); removal itself done (`t4b_flows` C1) |
| 16 | L2/R2 jump to the previous/next first letter in a row | `LetterJump` (Classic's `Alphabet` rule) in Recently played, Favorites and results, with the notice "Games starting with S"; the row goes to the top | done |
| 17 | Notices: added to / removed from favorites, removed from Recently played, letter jump | `ui::Toast` above the bottom bar through `Services::notice` (3.5 s), same wording as Games and Favorites | done |
| 18 | "Starting <game>" notice, then the core starts | The host takes over at once; the adapter no longer posts the notice (it did, against this row, for every start) | different (nothing to read before the screen changes) |
| 19 | Coming back to Home (`refresh()`) resets focus to the first recent | `onShow()` does the same; the search stays | done |
| 20 | Hints: "A Play game, Hold X, L2/R2, D-pad", search-focused hints, "Y Change view" in results only when Settings > Default view is Individual | Hints for each state (field, games, keyboard, modals); hold hints use the shell's hold bar; "Y Change view" only with the Individual default (`Library::viewsIndividual()`); Y itself still cycles the results' view | done (`t9_hints`) |
| 21 | Box-art fade-in, focus growth animation | None: calm design, focus is a border | different (framework rule) |
| 22 | Library or favorites changed elsewhere refreshes Home | Reloads when `Library::revision()` changes | done |
| 23 | "Home" page title and clock/side info | Shell tab bar | different (shell) |
| 24 | Screensaver / idle interplay, resume after a game ends | Nothing in Classic Home itself (App handles both) | n/a |

Counts: 25 rows; 17 done, 1 done by the host, 6 intentionally different, 1 not applicable.

Search results: Classic lists every match; the tab shows the first 500 in title order (the adapter walks all games within the per-frame
budget and keeps the sorted first 500), "500+" appears only when more matches exist (`SearchState::more`).

Verified against Classic code on the host (2026-11-23): the real `HomeScreen`, `GamesScreen` and `Favorites`/`History` against the
adapter and `HomeTab`, on generated libraries (three seeds): `t4_favs` (favorites and recents lists, 19 scenarios, tiles, titles, paths,
favorite flags), `t4b_flows` (hold X / hold B results in the files), `t5_launch` (A on a tile), `t9_hints`, `t2_lists` (search).

## Requests (not in this file's remit)

1. ~~Notices from tabs~~ Done (D9): `ui::Toast`, `src/app/Notice.h`; the header info only says where the user is.
2. ~~Shared search state~~ Done (D8): the Library search takes a caller id (`setSearch(query, scope, owner)`), `SearchHeader` gives each tab its own.
3. ~~B has two meanings~~ Done (D3): the first B clears the query, the next leaves; a tab that lists its own B hint replaces the shell's "Leave" (`shellFor`).
4. ~~Remembered results view~~ Done (D15): `Library::homeView()/setHomeView()`.
