# Arcade tab: parity with the Classic GUI

Classic has two Arcade surfaces: the **Arcade tab** (`GamesScreen` over the arcade system, shown only when the
library holds arcade games and "Show Arcade tab" is on) and **Settings > Arcade game status**
(`ArcadeGamesScreen` with `ArcadeScan`). `ArcadeSettingsScreen` ("Manage Arcade", one row) is dead code in
Classic (never opened) and is not rebuilt. The manufacturer/category entry points were removed from Classic
(docs/ARCADE.md); the database groups still exist but nothing browses them, so nothing is rebuilt for them.

Status: done = in `src/app/ArcadeTab.*` (directly or through the embedded `GamesTab`), shell = needs a shell or
Settings-tab change, diff = intentionally different.

| Classic behavior | New tab | Status |
| --- | --- | --- |
| Tab shows the arcade system's games: Grid / Small grid / List, art, detail, header | embedded `GamesTab` opened on the arcade system | done |
| Y changes the game view | `GamesTab` | done |
| Search (Start), results, empty search state | `GamesTab` | done |
| L2/R2 jump to letter | `GamesTab` (LetterPrev/Next) | done |
| Select opens the Game options (versions, screenshots, video, Start game) of the focused game | embedded `GamesTab` (`Event::Select`, shared `GameOptionsView`); `locked()`, `favoriteKey()`, `hints()` forward | done |
| Select on a row of the status table | Classic does not support it (`ArcadeGamesScreen` has no selected game); the event is ignored there | done (same) |
| Leaving the tab | `onHide()` closes the status screen (cancels a running check) and stops the games view and its preview | done |
| Hold X (2 s) favorites; removing asks first | `GamesTab` + `favoriteKey()` | done |
| A plays; several versions ask which | `GamesTab`; the status screen opens the shared Game options (`GameOptionsView`) for the version choice | done |
| Hints text names current buttons | `GamesTab.hints()` plus "X Game status" while the focus is not on a game | done |
| Tab hidden when the library has no arcade games | the shell hides the tab (`InterfaceService::arcadeTabVisible()`: preference and an Arcade system); the empty state remains for an empty arcade system | done (shell) |
| Preferences "Show Arcade tab", "Show Arcade in Consoles" | Settings > Interface (`InterfaceSettings`); the shell and the Systems tab follow them | done (Settings, shell) |
| Settings > Arcade game status | `ArcadeTab::openStatus()`; X in the tab when the focus is not on a game; from Settings through `Services::openArcadeStatus` | done |
| Back from the status screen opened in Settings returns to Settings (Classic: a Settings sub-screen); opened with X in the tab it stays in the Arcade tab | shell: `State::backTo = Tab::Settings` while the screen is open (`PatsGui.cpp`); the tab row highlights Settings meanwhile | done (shell) |
| Opening the status screen starts the check (restart each time) | `openStatus()` calls `startArcadeScan()` | done |
| Check progress: "Checking Arcade game n of m", "Searching Arcade folders... k found", bar; the header counts "n of m checked so far, k look ok" | progress panel with the same two states and "k look ok so far"; k is Classic's count (the entries that might work among those read so far; the scan's own `workingFound()` is only filled when a limit is given, which the adapter does not use, and showed 0 until this was fixed) | done (`t6_arcade`: the count is equal in every frame of the check) |
| B cancels the check and goes back | `cancelArcadeScan()` (new, Library.h) then back | done |
| Table columns Works?, Name, Year, Manufacturer, Category, Core?, Core file, ROM?, CRC?, ROM file, same weights | same columns and order, flags as check/cross glyphs; file names via `detail()` for visible rows | done |
| Header "n of m might work, showing: filter" (m = the .mra files that were read) | the shared `PageHeader`: title "Arcade game status", the summary as its info (D16); n and m are Classic's numbers (`ArcadeScanState::working`, `::entries`), not the number of rows below | done (`t6_arcade`: "28 of 57 might work" on the test library) |
| Y cycles All / Might work / Broken | same, through the `Works` argument | done |
| L2/R2 letter jump in the table | `jumpLetter(..., works)`, notice "Games starting with X" like every tab | done |
| A launches the row | `launch()` | done |
| Empty: "No Arcade game files found on connected drives"; "Nothing in <filter>" | `ui::drawEmptyPage` with why and next step | done |
| Hints: "A Play game, Move, L2/R2 Jump, Y Filter: x, B Back"; during the check "B Cancel scan and go back" | same wording in `hints()`; Left/Right turn a page of the visible rows (`ui::ListLayout::pageStep`, as in every list); the status screen is `locked()` and its B hint replaces the shell's "Leave" | done |
| Rows listed as individual .mra files; the Name column is the MRA's <name> | rows are the library's titles (versions grouped; the title is the library's, not the MRA <name>); a grouped row shows the verdict of its representative file, so a group whose alternatives disagree shows one of them; an .mra the library does not list (for example `mister-demo.mra`) has no row | diff: Library groups versions; the version choice replaces extra rows (`t6_arcade` measures: 41 rows for 57 files, 26 names differ, 4 groups with mixed verdicts, 1 file without a row) |
| Zebra rows, 36 px header | framework `Table` | diff: framework look |
| Games the check has not seen | flags become the words Yes / No / Not checked (no false cross) | diff: accessibility |
| Name is the second column | Name second as in Classic | done |
| At 130 % text | Category and Core file columns drop | diff: space |

## Not possible without outside changes

- When no arcade system exists yet, the status screen can run the check and report counts, but cannot list
  the games until the library holds them (Classic reads the scan entries directly).

Verified against Classic code on the host (2026-11-23): the real `ArcadeGamesScreen`/`ArcadeScan` against the adapter and `ArcadeTab` on generated
libraries (three seeds): `t6_arcade` (counts while checking and at the end, header numbers, column data of every row, the Might work / Broken
filters, B cancels), `t2_lists` and `t11_misc` (letter jump and search in the Arcade list), `t5_launch` (A on a row), `t9_hints`.
