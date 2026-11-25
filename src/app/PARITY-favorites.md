# Favorites tab: parity with the Classic Favorites tab

Classic: `GamesScreen` in favorites mode (`src/legacy/GamesScreen.cpp`), `GameSearch`, `Favorites`, `App` hold handling.
New: `src/app/FavoritesTab.cpp`. Status: done / intentional difference (reason) / not possible yet (needs).

| Classic behavior | New tab | Status |
| --- | --- | --- |
| Three views (list with details, grid, small grid), Y cycles, cursor kept | Grid, Small grid, List with details, Y cycles | done |
| View remembered in preferences (`favoritesView`, only when the default view is "individual") | `Library::favoritesView()/setFavoritesView()`, adapter applies the same rule | done (new Library calls) |
| "Game view: X" toast on Y | the view name is always in the header | intentional (the view name is in the header, no toast needed) |
| Sorted by name, case-insensitive, across systems | same | done |
| The tab's list (`GamesScreen::reload()` in favorites mode): the stored paths are bucketed by their system field (an empty or unknown system is skipped, there is no hidden-systems check); with "Organize game versions" on, the games of the library that hold one of the paths (a group counts when its own path or any version is stored), so ONE entry per game, titled by the group, and a deleted file is not in the library and not listed; off, every stored path once, titled by its file name, system files skipped, a deleted file still listed | `Library::favorites()` (adapter, a port of that code; the favorite flag of every entry is true). Home's strip uses `homeFavorites()` because Classic's Home differs in one rule: it lists a favorite whose file is gone (with the file's name) | done (checked: `t4_favs` F01-F12, 114 same, 0 differ) |
| Header "Favorites" and "n / total . view" | "Favorites", "view . n of total"; the header is drawn also when the list is empty (D12) | done |
| Up from the first row focuses the search field; A or Start opens the on-screen keyboard | same | done |
| Keyboard: D-pad, A press, X delete, Y space, Start submit, B cancel | shared `ui::TextEntryDialog` through `SearchHeader` (same keys, hints from the dialog); the list is filtered when the user presses Done (D2), B closes without change | done |
| Search rule: each term starts a word of title or system name | same rule: `Library::matchesSearch` (the adapter calls Classic's `GameSearch::matches`); no private copy in the tab | done |
| Query kept when the list reloads | kept | done |
| B clears the query while the field is focused; Down returns to the games | same (`SearchHeader`, D3); the tab lists "B Clear search" (replaces the shell's Leave); the next B leaves | done |
| No-match message "No games match this search..." | "No favorites found" + why + what to do | done (reworded, same meaning) |
| Empty favorites "No favorites yet. Hold X for 2 seconds on a game to add it here." | same meaning as why / next | done (button named "the favorite button": no layout token in the text catalog) |
| A and Start launch | A launches ("Play"); Start opens the search on every tab (D4) | intentional (owner decision) |
| Version choice, remembered most recent version, history, error dialog | `launch()` returns NeedsVersion only without a recent choice; then the shared Game options view opens for that game; host shows errors | done |
| Select opens the game's options (versions, Start, screenshots, viewer) | Select opens `GameOptionsView` for the game under the cursor | done |
| Hold X 2 s removes the favorite (all versions), toast, cursor stays in place | X hold, `ConfirmRemove` with the one removal text (D5); removes the asked game (never the cursor row), cursor keeps its index, notice "X removed from favorites" (`ui::Toast`) with the group's title, as Classic | done (Confirm is intentional; `t4b_flows` B1-B3) |
| Short X does nothing on a game | same (hold only) | done |
| L2/R2 jump to letter, "0-9" is one group, row put at the top, toast "Games starting with X" | `LetterJump` (Classic's `Alphabet` rule); the letter's row goes to the top (`GridPage` / `ListDetail::showRowAtTop`); toast "Games starting with X" | done |
| List view: Left/Right move 10 rows | page step: the visible rows (`ListLayout::pageStep`, D10), as on every tab | intentional (framework rule) |
| Hints "A Play game / Hold X 2s to favorite / Y Change game view / L2-R2 Jump to letter / D-pad Move", search hints | same set, per zone (games, search field, keyboard) | done |
| Cursor goes to the first game each time the tab is shown | cursor kept | intentional (less surprising; owner may switch) |
| List view: box art, title, system name, video preview of the selected game | `ListDetail`: video frame or box art, preview status line, title, facts line "system · year · players" (the same on every tab) | done |
| Preferences "show box art" and "small box art in grid" | not read by the tab | not possible yet (belongs to the adapter: return no art when off) |
| Focus animation of the tiles | none | intentional (framework: focus is instant) |

Verified against Classic code on the host (2026-11-23): the real `GamesScreen` in favorites mode and `HomeScreen` against the adapter and
`FavoritesTab` on generated libraries (three seeds): `t4_favs` (the list, titles, paths and flags in 19 scenarios, organize on and off),
`t4b_flows` (hold X on the Favorites tab and the resulting files), `t2_lists` (search rule), `t9_hints`.
