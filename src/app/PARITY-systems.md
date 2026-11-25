# Systems tab: parity with the Classic GUI

Sources: `src/legacy/SystemsScreen.*`, `src/legacy/SystemVisibilityScreen.*`, `src/legacy/HiddenSystems.*`, `src/legacy/App.cpp`,
`src/legacy/SettingsScreen.cpp`, `src/legacy/Preferences.*`, `src/legacy/SystemIconDownload.*`, `src/legacy/bridge/AppLibraryAdapter.*`.

| Classic behavior | New tab | Status |
| --- | --- | --- |
| Grid of systems that have games, tile = system icon, name | shared grid style (standard tile, icon centered, peek row), `systemArt(ArtSize::Grid)` | done |
| Order: catalog (grouped) order | sorted by `SystemInfo::order` (stable, name order inside) | done (new field) |
| Header "Consoles"; right side: focused system's group and total | `PageHeader` "Systems"; info "Grid . n of total" like every grid | intentionally different (the group text is other information than the other grids show) |
| Sublabel "no core" for systems without a core | `SystemInfo::noCore` -> "No core" | done (new field) |
| Hidden systems (by name, `hidden-systems.txt`) not shown | `hidden` flag, `setSystemHidden` | done |
| Arcade systems excluded unless "Show Arcade in Consoles" | `Library::showArcadeInSystems()`, re-read every frame, grid reloads when it changes | done (new virtual) |
| A opens the system in Games | `takeOpenRequest()`, fetched by the shell after `handle()` | done |
| Visibility screen: every system with games (arcade too), sorted by name, A toggles, B back, "n of m shown", Shown/Hidden pill | list of all systems, switch rows On/Off, "n of m shown", A / B | done; list order is the grid order, not name order (intentional) |
| Visibility entry point: Settings "Manage systems" | also X on the Systems tab | intentionally different (extra entry; Settings entry stays in the shell) |
| Hidden state changes apply at once on return | list closes, grid reloads | done |
| Empty: "No games found yet. Build the game list in Settings -> Library." | "No systems yet" + why + next; separate "All systems are hidden" state | done (all-hidden is new, Classic showed the first text) |
| Visibility empty: "No systems yet" | X is not offered when there are no systems | done |
| Hints "A Open console / D-pad Move / LB RB tab" | Move, Open, X Show or hide systems (list: Move, Page, Show or hide, Back) | done |
| Name only under the tile | name only; a second line only on tiles without a core: "No core" (D17; the grid gets the second line only when a shown system has no core) | done |
| Placeholder while the icon loads | letter tile | intentionally different (framework look) |
| Paced scan with "Searching for games" progress (live-scan mode only, 3 s settle) | none: `Library::systems()` answers at once | intentionally different (Library has no scan state; if live scanning returns, the adapter needs a progress API) |
| Missing system icons: download | not in the tab; Settings > System (icon download row, `SystemSettings`) | done (Settings) |
| Start tab "Consoles" | `resolveStartTab()` (shell) | done (shell) |
| A system's games opened from Consoles keep the Consoles tab highlighted; LB/RB go on from it (next is Arcade, or the next visible tab) | `highlightedTab()` returns `backTo` while the Games list was opened from Systems, also when the Games tab is shown in the row | done (shell; `t8_tabs`) |
| `--system` command-line option, opening Games on that system | not handled by the new GUI (Classic `main.cpp` option) | open (shell/host) |
| Favorites / all-games entries on Systems | Classic Systems has none | n/a |

Counts: 14 done, 5 intentionally different, 0 missing in the tab; 1 item open in the shell/host (`--system`).

Verified against Classic code on the host (2026-11-23): `t1_systems` (list, order, counts, flags), `t8_tabs` (tab order and Back/LB/RB behavior
with the Games list opened from Systems), `t9_hints`.
