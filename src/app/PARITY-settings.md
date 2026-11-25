# Parity: Settings (Classic `src/legacy/SettingsScreen.*` -> `src/app/SettingsTab`, first level)

Scope of the tables below: the first level (categories, rows, kinds, value texts, help texts, conditions, navigation, hints).
The submenus and flows are inventoried in the last section and are ported category by category. Status: **menu** = row present with Classic's label, order, value texts and help; **cond** = menu
with a condition; **diff** = intentionally different. "Later" names what Classic opens there, so the function work
is traceable. Row ids are the keys of the category tables (`src/app/settings/<Category>Settings.cpp`; the row model and builders are in
`src/app/SettingsMenu.h`) and of the first-level seam (`SettingsActions`). The functions behind the rows are ported per category;
the charter and the per-category checklists are in `src/app/settings/PORTING.md`. The sub-screen inventory is at the end of this file.

Classic behavior reproduced: three panes (categories, rows, "what this does"); a category opens on its first
selectable row; headings are skipped; Right or A enters the rows, Left or B returns, B on the categories leaves;
no wrapping; disabled rows stay focusable and A does nothing on them; a switch is On/Off.
Differences (first pass; see the source-derived check below): Classic cycles every choice with A and draws "A" as text for actions: here actions show the A glyph,
and A on an action row calls `activate` (nothing yet). Choices cycle in memory like Classic. Status values are
sample texts held in `MenuState::status` until the host supplies them.

## Library

| Classic row | id | kind | Classic value | Status | Later (Classic opens) |
| --- | --- | --- | --- | --- | --- |
| Build game list | library.build | Action | A | menu | starts the scan |
| Prepare box art | library.boxart | Action | A | menu | starts the artwork job |
| Reload saved game list | library.reload | Action | A | menu | reloads, toast |
| Core and MRA updates | library.coreupdates | Action | A | menu | modal flow: Intro, Checking, List (type/status filters, select all, Start = review), Review, Installing, Result |
| Arcade game status | library.arcade | Action | A | menu | Arcade status screen |
| Organize game-versions | library.versions | Switch | On/Off (default On) | menu | none |
| TheGamesDB | library.tgdb | Value (status) | check status ("Not configured", "Configured", "API key works", ...) | menu | key check (toast); no key: toast with the ini path |
| heading ScreenScraper.fr | library.ss.heading | Heading | | menu | none |
| Video previews | library.ss.video | Switch | Off by default | menu | enabling re-checks the account |
| Account | library.ss.account | Value (status) | Not checked / Checking... / detail | menu | checks the login again |
| User name | library.ss.user | Value (status) | Not set / the name | menu | on-screen keyboard |
| Password | library.ss.password | Value (status) | Not set / ******** | menu | on-screen keyboard, masked |
| Video sound | library.ss.sound | Value, cycles | Off, 100%, 75%, 50%, 25%, 10% | menu | none |
| Default region | library.ss.region | Value, cycles | EU, US, JP | menu | none |
| Preview delay | library.ss.delay | Value, cycles | 5..30 seconds, step 5 | menu | none |
| Video storage limit | library.ss.storage | Value, cycles | 256 MB, 512 MB, 1, 2, 4, 8 GB, Unlimited | menu | none |

All seven ScreenScraper rows are always shown in Classic (no dependency on Video previews).

### Library: function parity (Classic `src/legacy/SettingsScreen.cpp`, `src/legacy/ScanScreen.*`, `src/legacy/LibraryScan.*`, `src/legacy/MediaScraper.*`, `src/legacy/CoreMraUpdater.*`, `src/legacy/ArcadeGamesScreen.*`, `src/legacy/ScreenScraper*.*`, `src/legacy/TheGamesDBClient.*`, `src/legacy/Preferences.*`, `src/legacy/App.cpp`; port in `src/app/settings/LibrarySettings.*`, `src/app/settings/library/`, adapter `src/legacy/bridge/settings/AppLibrarySettings.*`)

Source-derived spec: the Library agent's `settings-spec-Library.md` (section numbers below are its sections). Status: done = ported and checked on the host; diff = a numbered difference (F = framework-forced, LD = deliberate deviation, see the lists below); device = needs the owner on the MiSTer.

| Classic function (source) | New location | Status | Verification |
| --- | --- | --- | --- |
| `buildRows()` Library rows, order, labels, values, help (`SettingsScreen.cpp` 286-326, 660-735, `settingHelp` 61-185) | `LibrarySettings.cpp` table, `current()`, `changed()`, `update()` (status texts) | done | host: labels, order, kinds, enabled and the 15 help texts compared with Classic's source text; 200 scripted press sequences (4160 presses, 26160 value comparisons) against the real `Preferences` and Classic's lambdas: values, `preferences.txt` bytes and forget/check/stop calls identical |
| Organize game-versions (310-313) | `AppLibrarySettings::setOrganizeGameVersions` | done | same sequences; the adapter calls `onGameVersionSettingChanged` and bumps `libraryRevision` for `AppLibraryAdapter` |
| Reload saved game list (292-300, `App::reloadLibrary`) | `LibrarySettings::activate` + `AppLibrarySettings::reloadSavedGameList` (`setScanningAllowed(true)`, `settingsHost.reloadLibrary`, `false`) | done, device | host: the service is called once, toast "Saved game list loaded"; blocking, see LD6 |
| Build game list: `ScanScreen` Intro, Working, Done (`ScanScreen.cpp` 19-173, 222-281, 492-617) | `library/ScanFlow.cpp`, `Widgets.*` | done, device | host: events drive Intro, Working, Back/cancel texts, failure, no-drive; hints verbatim |
| Build game list: presentation changed, function unchanged | `library/ScanFlow.cpp` (`WizardFlowPage`, Large frame shared with the box-art pages) | done, host | Intro, Working and Done are pages of one Wizard in a dialog frame; Classic's steps, transitions and Back rules are unchanged (B on Intro closes = "Do this later", B on Working cancels the scan and the page ends with it, a finished scan goes to the box-art offer, a failed or stopped one to Done). Wording: hints use the closed vocabulary (Start, Close, Cancel, Done, Try again); the Working page shows one status phrase, one counter line and the bar, "Stopping the scan" after B; the result has status word, headline, what changed, next step and the service text only as Details. "Scan stopped" is split: stopped by the user (Warning, nothing saved, old list kept) vs failed (Error, Details = service text, A Try again returns to the Intro) |
| Build game list: Drives step added (owner), Start/Done presentation | `library/ScanFlow.cpp` (Start, Drives, Working, Done), `LibraryService::driveSpace` (bridge: `statvfs`) | done, host; deliberate change | Classic scans every detected root without asking; the Drives step lists `scanRoots()` (path, used of total), all checked, A toggles, Start searches with the checked roots (`startScan(roots)`); nothing checked: Start is off with a reason; no drive: a centred "No drives found" page, A re-reads the roots, B goes back to Start. Start is a centred page (symbol, headline, two sentences), Working a centred Busy page. Done: title "Build game list" after a scan, "Prepare box art" in the standalone box-art flow. Everything else (Working, cancel, offer, Done results, retry) is Classic's |
| Prepare box art: ArtworkOffer, ArtworkWorking, Done (`ScanScreen.cpp` 30-53, 100-118, 382-444) | `library/ScanFlow.cpp` | done (FL1, FL2, LD1-LD3), device | host: picker wraps both ways, key passed, stop text, no-match, no systems |
| Prepare box art in the Build game list style: presentation changed, function unchanged | `library/ArtworkFlow.cpp` (Offer, Working), `ScanFlow.cpp` (`drawQuestionList`, Drives page, box-art Done outcomes) | done, host; deliberate change | Offer is one page like Drives: question, one muted line, system list (rules unchanged), scope line under it; no extra explanation page (one muted line says it; the chain is long enough). Working is a centred Busy page, counters in one detail line, B stops. Done: stopped by the user (`Visit::artworkCancelled`, Warning, no retry) is told apart from failed and from "no covers added" (Error, Try again). Drives and Offer lists are top-aligned under the title instead of centred as a block. Sample: `library.roots=one\|six` added |
| Long waits tell what happens (owner feedback on the scan) | `ui::BusyPage` (`WizardParts.*`), `ScanFlow.cpp`, `ArtworkFlow.cpp`, `CoreFlow.cpp`, `UpdatePages.cpp`, `WifiFlow.cpp`, `BackupFlow.cpp`, `IconsFlow.cpp`, `DiscInstallFlow.cpp`, `FirstRun.cpp` | done, host; deliberate change | Every Busy page shows the phase phrase, the service's own "what now" text as detail (once a second at most), "Running for 1:25", and after 10 s without a visible change a wait note ("... Nothing is stuck."). The scan has ONE bar for the whole job (systems 0..0.60, Arcade 0.60..0.85, saving 0.85..1.0, never backwards), the phrases Looking for game folders / Reading your game folders / Reading the Arcade games / Saving the game list, and counts "46 systems, 10485 games"; the cover job keeps one phrase and shows the current system and game as detail. Function unchanged; the "one detail line" rule of ProgressPanel is extended on purpose (docs/components/ProgressPanel.md) |
| Scan and box-art jobs (`LibraryScan`, `MediaScraper`, stepped once per frame by `App`) | `Context::settingsHost.scan/scraper`; the adapter never steps them; reload when a started scan ends well (`ScanScreen::update` 145-149) | done, device | review: no `step()` in the adapter |
| Core and MRA updates, six dialogs (`SettingsScreen.cpp` 1209-1650) | `library/CoreFlow.cpp`; `CoreMraUpdater` owned by the adapter, unchanged | done (FL3-FL6), device | host: offline/online Intro with the 2 s re-check, Checking, List (filters, select, X, Y, Start), Review, Installing (no input), Result, failed check and Try again |
| Arcade game status (`ArcadeGamesScreen`) | `SettingsEnv::openArcadeStatus` -> `Services::openArcadeStatus` -> `ArcadeTab::openStatus()` + Arcade tab (owner decision) | done (FL7) | host: the hook is called once; without a hook the row is not handled |
| TheGamesDB row, key check, success toast (317-326, 1002-1012) | `LibrarySettings::activate/update`, `AppLibrarySettings` (+ `discArtwork.refresh` on success) | done, device | host: the four notices with their seconds (8, 4, 5, 7), failure shows no notice |
| Video previews, Account check (664-686) | `changed("library.ss.video")`, `activate("library.ss.account")`, adapter forgetAccount + checkAccount, `preview.stop()` | done, device | host: calls and texts for every account state |
| User name, Password keyboards (753-784) | `library/AccountFlow.cpp` (`TextInputPage`, masked, 64, no trim, empty accepted, wiped on close) | done (FL8, FL10), device | host: Start = Done saves the preloaded name, B discards, empty password saved, failed save shows the error page |
| Video sound, Default region, Preview delay, Video storage limit (693-735) | `current()`/`changed()` with `library/Values.h` | done | the sequences above, plus every valid start value x 8 presses |

#### Framework-forced differences (Library)

| # | Difference | Why |
| --- | --- | --- |
| FL1 | Box-art system chooser: a Value row "System" with Left/Right (wraps both ways) instead of the drawn "<  All systems  >" | no Picker component |
| FL2 | No moving dot on the progress pages (scan, box art); Classic's `LoadingIndicator` draws one | framework pages do not animate |
| FL3 | The hint sentences with a state ("Checking official files...   B Stop", "Installing updates...   Keep MiSTer powered on") are the page text plus the hints of the real keys; "Keep MiSTer powered on" is a line on the Installing page | hints are key + action |
| FL4 | The Checking page's live list is `ResultItem` rows in a surface, the filter row of the update list is underlined labels, update rows are `ListItem` `Choice` rows (size and the second status line are in the detail line), files that cannot be picked are `Info` rows | no `TabRow`, no Result container, no size column |
| FL3a | Core and MRA updates is a Wizard flow (Large frame, one title "Core and MRA updates" on every page): Intro, Checking and Installing are centred pages; the list is a Multiple ChooseList with a filter tab row (Left/Right) and a summary line; Review is a scrolling text; Result and the failed check use the one Result layout. Presentation changed, function unchanged: same states, filters, selection rules (X selects missing/outdated over all files, only A on selectable rows), Y checks fresh, B closes from Intro/Checking/List, B on Review goes back to the list, no cancel while installing, Try again returns to the Intro | wizard conversion |
| FL3b | Presentation changes of the core pages: no live list of checked files while checking (one status line and one "N files checked" line), no per-file counters, KiB or file names while installing (status line, power note; the bar is overall), the heading rows of the list (Arcade cores, Console cores, ...) became the first words of each row's second line, rows that cannot be picked (up to date, cannot be checked) show a muted row with an empty box instead of a different layout, the installed/failed counts moved to the Result (two item rows; the service has no list of failed files), sizes are MB/KB everywhere (the Review said MiB), "Select all" is worded "Select needed" (it selects what needs an update), Offline Intro says the page checks again by itself, Review hint "Install" | audit library-updates, owner premise |
| FL5 | Review is a framework `Table` with a header row ("File", "Size") that Classic does not draw | the table component has a header |
| FL6 | The Core updater's pages lock the tabs (LB/RB do nothing); Classic's modal leaves LB/RB active and keeps the modal and the work running behind the other tab | framework page rule |
| FL7 | Arcade game status is the Arcade tab's status screen (owner decision); Back leaves into the Arcade tab, Classic returns to the Library row | owner decision, shell |
| FL8 | The keyboard shows the Classic body line ("The user name of your ScreenScraper account") as a pill under the dialog, the trailing "_" is the framework caret, the in-panel footer is the hint bar | `TextEntryDialog` |
| FL9 | Long row values are elided by `ListItem`; Classic squeezes the label out | framework list item |
| FL10 | Results and errors are `ResultPage` modals (Done, Warning, Error): "Scan stopped" and "Cover search stopped" are Warning, "Update check failed" is Error with Details; Classic prints them on a panel (and `showError` for the login) | framework dialogs; `showError` is invisible in the hosted GUI (charter section 6) |

#### Deliberate deviations (Classic quirks that are plain bugs, not ported)

| # | Classic | New | Why |
| --- | --- | --- | --- |
| LD1 | The scan Done page prints the box-art counters whenever the shared `MediaScraper` is not Idle, so a visit that never fetched covers shows the numbers of an earlier job (`ScanScreen.cpp` 539-551) | the counters appear only when a box-art job was started in this visit | stale numbers are wrong information |
| LD2 | `App::closeScan` reloads the library when the last job's counters are non-zero, also in a visit without a job (for example "Do this later" on the Build intro) (`App.cpp` 490-495) | `libraryTaskClosed()` reloads only after a box-art job started in this visit | the reload is blocking and was useless |
| LD3 | "Skip for now" on the box-art offer shows an empty Done page (only "A Finish", plus stale counters) | the visit ends at once | an empty page |
| LD4 | A hand edit of `screenscraper-fr-settings.ini` shows in User name/Password only after a restart (read once) | same (read once, after a save): kept, not a deviation | listed so it is not mistaken for a bug |

Prepare box art (`library/ArtworkFlow.cpp`) is a wizard flow in the Large frame (offer, working, Done); the presentation changed, the function is Classic's plus the owner's multi-select: the offer's systems are a checkbox list with "All systems" first (chosen by default, exclusive with the others, back again when the last one is cleared), A toggles, Start downloads with the chosen keys (empty = all), B = Skip (Done after a scan, otherwise the visit ends). The working page shows one status phrase, one detail line (downloaded / prepared) and a bar; the four counters and the finish estimate appear only on the Done page; B stops the job (what arrived stays).

Kept Classic behavior that looks odd (ported on purpose): the first "Reloading the saved game list..." toast is replaced before a frame is drawn; Installing's "failed" count lags until the next file succeeds; A on Build game list starts although no drive was found (the scan then fails with its message); Prepare box art with no database ends in "Cover search stopped / No matching covers were found in the online artwork library."; X in the update list sets the default selection (it also clears manual picks); the box-art job fetches "-BG" backgrounds (default options) while the offer text mentions covers only.

#### Blocking work behind the seam (flagged, as in Classic)

`reloadSavedGameList()` blocks the UI thread (the whole library reload, and with no database a walk of the game folders); `onGameVersionSettingChanged` rebuilds Classic's lists synchronously; `saveScreenScraperLogin` writes the ini synchronously; `TheGamesDBClient::checkState/checkStatus/configured` read the key file at most once a second while the Library category is shown (as Classic's row does); the scan and box-art steps run in App's frame loop (one system / one game per frame); the update check and install, the TheGamesDB check and the account check run on their own worker threads.

## Controllers

| Classic row | id | kind | Classic value | Status | Later |
| --- | --- | --- | --- | --- | --- |
| Controllers | controllers.controllers | Action | A | menu | Controllers screen (test, stick threshold, button setup) |
| Controllers (wizard flows) | Mapping wizard and Bluetooth pairing are WizardFlowPage flows (settings/controllers/ControllersFlows.cpp): mapping Open -> Capture x19 (raw input via `WizardFlowPage::onRawInput` -> `setRawInputWanted`, "n / 19" in the frame, pad diagram in the body, hold Start 5 s via HoldProgress) -> Save -> Result (saved with skipped count / cancelled / controller lost / save failed with "Try again"); Bluetooth Intro -> Scan (Busy, timeout bar) -> Found -> hand-off to the mapping flow, Timeout / Start failed / No adapter as Results. List, input test and deadzone stay pages (EmptyPage, header info, hints from the focus, one reading width). | presentation changed, function unchanged: same state machines (MappingWizard, btpair device-list diff, 120 s timeout, Classic's cursor rules). Deliberate: success/cancel end in a Result (not a toast), no "Choose wisely", no double space, Bluetooth has no found-device list (btpair pairs by itself; Found shows the one new pad), raw btpair text only as Details of a timeout, `ControllersService::bluetoothTimeoutSeconds()` (default 120 s) so the host can reach Timeout, the first capture step has no hint (its page says how to cancel) | host: all steps, hold, cancel, save failure, Bluetooth Found/Timeout/failed/none, pseudo, text 130, themes |
| Face-button labels | controllers.faces | Value, cycles | XBOX, PlayStation, Nintendo | menu | applies the labels at once |

### Controllers: function parity (Classic `src/legacy/SettingsScreen.cpp`, `src/legacy/ControllersScreen.*`, `src/legacy/ButtonLabels.*`; port in `src/app/settings/ControllersSettings.*`, `src/app/settings/controllers/`, adapter `src/legacy/bridge/settings/AppControllersSettings.*`)

Both first-level rows are real now. Texts are the exact Classic English strings under `settings.controllers.*`; button names inside texts are `{A}`, `{START}`, ... placeholders
that the framework prints in the user's layout. The state machines (`PadLogic.*`) are pure and run against the sample service on the host; the pages draw with framework
primitives. "Host" = `make -C src/app run ARGS="--tab settings --page controllers/<id> ..."` (ids: list, test, deadzone, wizard, bluetooth; `--list-samples` names the `controllers.*` states).

| Classic function (source) | New location | Status | Verification |
| --- | --- | --- | --- |
| Row `Controllers` (`SettingsScreen::buildRows`, `onManageControllers_`) | `ControllersSettings::activate` -> `openPage("list")`, row value is the framework's A glyph (layout aware) | done | host `--category controllers`; device: A opens the list |
| Row `Face-button labels` cycle (xbox -> playstation -> nintendo) + `Preferences::setFaceButtonStyle` + `::setFaceButtonStyle` | `ControllersSettings::current/changed`, `AppControllersSettings::setFaceButtonStyle`; the new GUI follows through `classicLook()` (`LookSettings::buttonLayout`) the next frame | done | host test: cycle order, strings; device: cycle, hints and glyphs change at once, restart keeps it |
| `App::openControllers`, `ControllersScreen::refresh/rebuildRows` (files read once per refresh) | `openPage` -> `ControllersService::refreshControllers()` (adapter reads MiSTer.ini and `.map` once), `ListPage` | done | host test: reads = 1 per refresh |
| `handleListAction` (Up/Down, B, A, Start, X, Y; no-stick toast) | `ListPage::handle` | done | host test `testListPage` |
| `renderList` (rows, `*`, detail panel, empty text, Bluetooth row and its text, shared-mapping line) | `ListPage::draw` | done, diff F7 | host `controllers/list`, `--set controllers.pads=0 / 3`, `controllers.shared=1` |
| `enterInputTest`, `anyInputActive`, `updateInputTest` (Start 5 s, idle 10 s, 15 % axes with rest-chasing, disconnect) | `InputTestLogic` + `InputTestPage` | done | host test `testInputTestLogic`, `testListPage` (every event swallowed, 10 s exit) |
| `renderInputTest` (D-pad, face diamond, shoulders, triggers as bars, Select/Start/Home, sticks, generic-pad raw line, idle warning, Start-hold bar) | `drawInputTestPad`, `InputTestPage::draw` | done, deviation CD5 | host `controllers/test` with `controllers.pad.keys/stick/hat`, `controllers.script=test`, `analogTriggers=1`, `generic=1` |
| `enterDeadzone`, `handleDeadzoneAction`, `renderDeadzone` (default 25, 0..100, `MisterIni::writeDeadzone` with backup, toast, error stays) | `DeadzonePage`, `AppControllersSettings::saveDeadzone` | done | host test `testDeadzonePage`; device: save, inspect MiSTer.ini and the backup |
| `beginWizard` (19 steps, SNES-position slots, read-modify-write) | `wizardSteps()`, `MappingWizard::begin` | done | host test: steps, slots, bytes equal to `ControllerMap::write` (throwaway equivalence test) |
| `updateWizard`, `captureWizardStep`, `advanceWizardStep`, `primeButtonBaseline/AxisBaseline`, `hatEdge` (edge trigger, reuse lock, Start reserved, hold 5 s abort, tap skip, 60 s skip, 75 % sticks, hat encoding, OK/Back exempt) | `MappingWizard` (`PadLogic.cpp`) | done, deviation CD2 | host tests `testSkipAndPreserve`, `testReuseLockAndStartReserved`, `testHatsAndStep0Guard`, `testSticks`, `testTimersAndAbort` |
| `renderWizard` (opening screen, press heading, countdown and hold bars, `n / 19`, holding text) and `renderPadDiagram` | `WizardPage::draw`, `drawWizardDiagram` | done, deviations CD1, CD3, CD4 | host `controllers/wizard` (`--keys a --set controllers.script=wizard --after 3`), all layouts, 130 % text, `--pseudo` |
| `saveWizardResult`, `abortWizard` (toasts, `Could not save`, back to the list) | `WizardPage::update` | done | host test `testWizardPage`; device: write and compare the `.map` bytes |
| `wantsRawInput()` and `App`'s suppression of the pad-to-Action translation | `ControllersService::setRawInputWanted` -> `SettingsHost::rawInputWanted` (set when capture starts, cleared in `WizardPage::onClose` and `ControllersSettings::onHide`, and by the adapter's destructor) | done | host test: switch true only while capturing, cleared on every exit; device: no press leaves the wizard |
| `enterBluetooth`, `handleBluetoothAction`, `updateBluetooth`, `leaveBluetooth`, `renderBluetooth` (btpair, device-list diff, 120 s, Found/Timeout, wizard on the paired pad) | `BluetoothPage`, `AppControllersSettings` (`Bluetooth`) | done, diff F10, deviation CD6 | host test `testBluetoothPage`; device: real adapter |
| `hints()` of every mode | `hints()` of each page | done, diff F8, F9, deviation CD3 | host renders |
| `ErrorModal` for `Could not save` (x2) and `Could not start` | `ResultPage` Error | done, diff F10 | host `controllers/deadzone --keys a --set controllers.deadzone.fail=1` |
| `formatButtonLabels()` / `buttonName()` in every hint and text | framework: `ui::ButtonLayout`, `Theme::setButtonLayout`, `ui::buttonName`, `ui::formatButtons` (`src/ui-framework/kit/Buttons.*`); `Hints` and the action-row glyph read it | done | host test: names equal Classic's `ButtonLabels` for all three layouts and ten buttons; renders with `--layout playstation|nintendo` |

#### Deliberate deviations from Classic (Controllers; each is a Classic oddity, the port keeps the intent)

| # | Classic | Port | Why |
| --- | --- | --- | --- |
| CD1 | `formatButtonLabels` matches case-insensitively: step 16 reads "Open the menu from inside (A) running game" | placeholders are upper case and case-sensitive (`{A}`); the step reads "Open the menu from inside a running game" | a lowercase article is never a button |
| CD2 | the hat fallback also answers step 0, so a D-pad press can become "Start"; that code is above `KEY_MAX` and `keyState()` reads out of range | step 0 accepts keys only | Start must be a real key (it is read back as a key for skip and cancel) |
| CD3 | during step 0 the hint line says "Press Start to skip this button   Hold Start 5s to cancel setup", but step 0 has no skip and no cancel | no hints during step 0 | the hint named actions that do nothing |
| CD4 | the opening paragraph is one unwrapped line | it wraps (`drawTextBlock`) | long line clips |
| CD5 | the input test draws the face buttons as fixed Xbox letters; the wizard diagram follows the layout | the test follows the layout too | otherwise ✕ is lit under the letter A |
| CD6 | A on "Found" with no matching row (cannot normally happen) leaves a wizard mode without steps | back to the list | no dead mode |

#### Open points for the owner (Controllers)

C1. Hint words that are not in the closed vocabulary of `src/ui-framework/docs/components/Hints.md` are kept as Classic has them: "Begin button setup", "Cancel (nothing saved yet)", "Stick deadzone", "Set up buttons", "Test controller", "Scan for Bluetooth controllers", "Stop scanning", "Set up buttons now", "Do this later", "Scan again", "Skip this button", "Cancel setup". The vocabulary table suggests Cancel for "Stop scanning" and Skip/Later for "Do this later": decide.
C2. The list shows Classic's one static hint line for every focus (also on the Bluetooth row and on an empty list, where X, Y and A do nothing); the framework rule is "a hint only for an action the state offers". Kept Classic's.
C3. When a saved deadzone or `.map` takes effect (live at the next core load or after a reboot) is unresolved (docs/CONTROLLER.md open question 1); Classic says nothing and so does the port.
C4. The input test and the wizard read the raw `BTN_START`; a pad without `BTN_START` (generic pads) cannot use the Start rules and relies on the 10 s idle exit (test) or the 60 s step timeout (wizard): as Classic.
C5. The pad drawings (`src/app/settings/controllers/PadDiagram.*`) are an application composition from rounded rectangles and labels. If another screen ever needs a pad diagram it could become a framework component: confirm that it stays application-level.
C6. Classic's error dialogs have only a headline and one sentence; AGENTS.md asks for "what to try" and a Details line. Kept Classic's sentences.

## Interface

All 13 rows are in Classic's order with Classic's labels and help texts (compared with `src/legacy/SettingsScreen.cpp` `buildRows()` and `settingHelp()` by a throwaway script: 13 of 13 identical). None is hidden or disabled in Classic, none is here. A cycles forward with wrap; there is no Left/Right change. Every setter is the `Preferences` setter Classic calls, so the file (`preferences.txt`, `hidden-systems.txt`) is the same.

| Classic row | id | kind | Classic value | Status |
| --- | --- | --- | --- | --- |
| Manage Consoles | interface.consoles | Action | A | done: Content page (below) |
| Show Games tab | interface.gamestab | Switch | On | done; the tab row follows it every frame |
| Show Arcade tab | interface.arcadetab | Switch | On | done; the tab shows only with an Arcade system in the library |
| Show Arcade in Consoles | interface.arcadeconsoles | Switch | Off | done (the Systems tab polls it) |
| Start tab | interface.starttab | Value, cycles | Home, Favorites, Consoles, Games, Arcade | done; applied when the GUI opens |
| Tab display | interface.tabdisplay | Value, cycles | Names, Icons, Both | done |
| Color theme | interface.theme | Value, cycles | all 22 Classic themes, same order | done; the framework has all 22 (owner decision), the names say the color (FI5) |
| Dark mode | interface.dark | Switch | On | done |
| Show box art | interface.boxart | Switch | On | done |
| Load small boxarts in grid-view | interface.smallgrid | Switch | Off | done |
| Start sound | interface.startsound | Switch | On | done |
| Menu sounds | interface.menusounds | Switch | On | done |
| Game view | interface.gameview | Value, cycles | Grid, Boxart small, List, Individual | done, Classic's words, toast "Game view: <name>" |

### Interface: function parity (Classic `src/legacy/SettingsScreen.cpp` 347-460, `src/legacy/App.cpp` `toggleGamesTab/toggleArcadeTab/cycleDefaultView/openVisibility/closeVisibility/visibleTabs/arcadeTabShown/selectTab/dispatch/init`, `src/legacy/SystemVisibilityScreen.*`, `src/legacy/HiddenSystems.*`, `src/legacy/UiSounds.*`, `src/legacy/Preferences.*`, `src/legacy/Theme.cpp`, `src/legacy/TopBar.cpp`, `src/legacy/SystemsScreen.cpp`, `src/legacy/GamesScreen.cpp`; port in `src/app/settings/InterfaceSettings.*`, `src/app/settings/interface/ManageConsolesPage.*`, service `InterfaceService`, adapter `src/legacy/bridge/settings/AppInterfaceSettings.*`)

| Classic function (source) | New location | Status | Verification |
| --- | --- | --- | --- |
| `buildRows()` Interface rows: labels, order, values, cycle orders | `InterfaceSettings` table, `current()`, `changed()` | done | host: label and help of all 13 rows equal Classic's text (script); `eq_test` A on every row: switch flips and flips back, Start tab Home-Favorites-Consoles-Games-Arcade-Home, Tab display Names-Icons-Both-Names, Game view Grid-Boxart small-List-Individual-Grid, Color theme 22 presses return to the first |
| `settingHelp()` Interface texts | `settings.interface.*.help` | done | same script |
| `App::openVisibility()` + `SystemVisibilityScreen::refresh()` (every system with games, `strcasecmp` order, built on open) | `ManageConsolesPage`, `InterfaceService::systemsWithGames()` read on open | done | host: `--page interface/consoles --set interface.systems=N`; `eq_test`: order, rebuilt on reopen, cursor 0 |
| `SystemVisibilityScreen::handle()` Up/Down no wrap, A toggles, B closes, nothing else | `ManageConsolesPage::handle()`, every other event swallowed (tabs locked) | done | `eq_test`: no wrap at both ends, Left/Right/TabNext do nothing, B closes |
| `HiddenSystems::toggle()` saves `hidden-systems.txt` at once | `AppInterfaceSettings::setSystemHidden()` (contains != wanted -> toggle) | done | real `HiddenSystems` on the host: insertion order, un-hide removes the line |
| `SystemVisibilityScreen::render()` header "Systems", "N of M shown", 700 px list, "No systems yet", hint "A Show or hide this console   B Back" | page header, Dense Switch rows, `drawEmptyPage`, hints | done / diff FI2, FI3 | host render, 130 %, 720p, pseudo-locale, high contrast |
| `App::closeVisibility()` refreshes the Systems tab | the Systems tab reloads in `onShow()` and polls the library revision | done | the tab is not shown while the list is open |
| `App::toggleGamesTab()` / `visibleTabs()` / `TopBar::visibleTabs()` | `patsgui::State::hidden[]`, `refreshTabRow()` every frame from `InterfaceService::gamesTabVisible()`; `shellFor()` leaves hidden tabs out | done | `eq_test`: row "012346" without Games |
| `App::toggleArcadeTab()` / `arcadeTabShown()` (preference and an Arcade system) | `InterfaceService::arcadeTabVisible()`; adapter `showArcadeTab() && library.arcadeSystem()` | done (owner decision D4: hidden like Classic) | `eq_test`: hidden without arcade games, back with them |
| Tab cycling over the visible tabs with wrap (`App::dispatch`) | `handle(TabPrev/TabNext)` over `visibleTabs()` from `highlightedTab()` | done | `eq_test`: Next from Settings wraps to Disc; skips the hidden Games tab |
| A current tab that vanishes gives way to Home (`toggleGamesTab`, `toggleArcadeTab`, `reloadLibrary`) | `refreshTabRow()` on the visible-to-hidden change | done | `eq_test` |
| The Games list opened from Consoles keeps the Consoles tab highlighted (`detailActive_`) with Games hidden | `highlightedTab()` returns `backTo`; TabNext from there continues after Consoles; also when the Games tab is shown in the row (it highlighted Games and went on to Settings until this was fixed) | done | `eq_test` + host render; host test `t8_tabs` |
| `App::init` Start tab rule (`favorites`/`systems` always, `games` only with the tab, `arcade` only with the tab, else Home) | `resolveStartTab()`, `State::useStartTab` set by `PatsGuiScreen::open()` | done | `eq_test`: 8 cases; host `--start --set interface.starttab=games --set interface.gamestab=0` opens Home |
| `TopBar::render` tab display, fallback to Names without icons | `PatsGuiScreen::update()` (already) | done / diff FI4 | host `--set interface.tabdisplay=icons|both` |
| `Theme::setColorTheme/setDarkMode` + `onThemeChanged`, applied at start (`App::init` 149-150) | adapter writes `Preferences` and the Classic `Theme`, calls `onThemeChanged`; the framework theme follows through `PatsGuiScreen::syncLook()` (`classicLook()`, same index) | done | contact sheets of all 22 themes in dark and light, high contrast; `ui-demo --contrast` 0 of 52 configurations fall short |
| `GamesScreen`/`HomeScreen` honor "Show box art" (tiles, list detail); Game options do not (`App::openGameActions`) | `AppLibraryAdapter::gameArt()` gated, `gameOptionsArt()` (new `Library` call, used by `GameOptionsView`) not gated | done | `eq_test` with the sample library |
| `artworkVariantFor()`: Grid -> Small with "Load small boxarts" | `AppLibraryAdapter::gameArt()` maps `ArtSize::Grid` to `Small`; Home and Detail unchanged | done | `eq_test` |
| `App::cycleDefaultView()`: `setDefaultView`, toast "Game view: <name>" | `InterfaceSettings::changed()` (`setGameView`, `PageStack::toast` 3.5 s); Classic's live screens are not reset (owner decision) | done | `eq_test`: toast shown after each press |
| `App::init` initial views and `rememberView` (Individual mode stores per-tab views) | `AppLibraryAdapter::favoritesView/homeView/gamesView/set...` (already) | done | unchanged |
| Arcade list view: global view, Grid in Individual mode, never remembered | `Library::arcadeView()`, `GamesTab::setArcade()` (no `setGamesView` for it) | done | by reading; the device test below |
| `UiSounds`: navigation on the six movement actions before routing, tab change in `selectTab`, launch on `standDown`, start after the splash; gated by Menu sounds / Start sound | `InterfaceService::playSound(UiSound)`; the shell plays TabChange on every tab switch; Navigation and Start only when `State::ownsSounds` | partial: see "Sounds" below | `eq_test` with a sound log (6 movement events, tab switches, gates) |
| `Preferences::save()` semantics (defaults omitted, name-based theme, non-atomic) | same setters | done | real `Preferences` compiled on the host: file after changes and after reset to defaults |

#### Sounds (Interface)

While the new GUI is hosted by Classic's `App`, `App::dispatch` already plays the navigation sound for every movement action (it runs before the screen is chosen), `App::run` plays the launch sound when a game starts (`Context::standDown`) and `App::init` the start sound. The new GUI therefore plays only what App cannot: the **tab-change** sound (the tab switch happens in `patsgui::handle()`, not in `App::selectTab()`). `State::ownsSounds` (default false) makes the shell play Navigation (same six events, before routing) and `playStartSound()` (call after the splash) itself; it is for the day the new GUI is the primary program. The launch sound is not wired in the shell yet (nothing in the new GUI calls `UiSound::GameLaunch`): needed then, in the tab code that starts a game.

#### Framework-forced differences (Interface)

| # | Difference | Why |
| --- | --- | --- |
| FI1 | A switch shows the On/Off word with the framework's checkbox, a cycling choice its value in the accent color | `ListItem` kinds; Classic draws a checkbox for any row whose value reads On or Off (same look for the switches) |
| FI2 | Manage Consoles: the Shown/Hidden pill is a Switch row (On = shown); no eased focus animation; the dimmed text of hidden rows is not drawn | `ListItem` Switch, no animation in the framework |
| FI3 | Manage Consoles empty page has a why and a next line besides "No systems yet": "No games were found, so there is no console to show or hide." / "Build the game list in Settings, then come back." | `drawEmptyPage` wants why and next (AGENTS.md empty screens) |
| FI4 | The tab icons are built in, so Classic's "icons missing -> Names" fallback never happens; the tab row falls back to icons itself when it is too narrow | framework tab row and bootstrap icons |
| FI5 | The color theme names are the framework's: Classic's brand names became color names (Nintendo Red, PlayStation Cobalt, XBOX Green, MiSTer Pat Blue, Batman Yellow, Superman Navy, Flash Maroon, Green Lantern Emerald, Iron Man Scarlet, Barby Pink; the others keep their names). The stored value is Classic's name, so both GUIs agree | `src/ui-framework/` stays application-agnostic |
| FI6 | The palette is derived by the framework (contrast repair, its own light mode): colors are not pixel-identical to Classic's. 19 of 22 themes use Classic's source colors unchanged | `src/ui-framework/kit/Palettes.cpp` |
| FI7 | Classic's `notify()` text "Game view: <name>" is the Settings page toast | no Classic bottom bar in the new GUI |

#### Deliberate deviations (Interface)

| # | Deviation | Reason |
| --- | --- | --- |
| DI1 | Three themes had a source color changed so that every pair passes `ui-demo --contrast` (numbers in `src/ui-framework/kit/Palettes.cpp`): Navy (Classic Superman) `surfaceHi` 0x183765 -> 0x16335F (error text on surfaceHi 4.30:1, needs 4.5); Maroon (Flash) light page 0xD98B8B/0xC97575 -> 0xEECBCB/0xDEBABA (focus color on the page 1.75:1, needs 3.0); Ruby accent 0xB51F3D -> 0xB92A47 (accent on surface 2.80:1, needs 3.0). The new GUI shows the changed colors; Classic keeps its own | owner rule: every theme must pass the contrast run |
| DI2 | Search no longer skips hidden consoles (the adapter did; Classic's search never looks at the hidden list) | Classic wins |
| DI3 | The Game options cover shows even with "Show box art" Off (the adapter hid it; Classic's Game options request the cover directly) | Classic wins |
| DI4 | The Arcade list opens as Grid in Individual mode and a view change there is not stored (the new tab used the Consoles view and stored it) | Classic wins |

#### Open points for the owner (Interface)

1. The tab-change sound is the only sound the new GUI plays while hosted; Navigation, launch and start stay with Classic's `App` until the new GUI is the primary program (`State::ownsSounds`, the launch hook is still to be written).
2. Classic's own live screens are not reset when Game view is changed in the new GUI (owner decision): Classic shows the new view after its next start.
3. A held direction with Menu sounds On starts about 12 sound processes a second (Classic's behavior, kept).
4. Disc tab visibility (drive, package) is the Disc tab's request; the shell row mechanism (`State::hidden`) is ready for it.

## Accessibility

| Classic row | id | kind | Classic value | Status | Condition / later |
| --- | --- | --- | --- | --- | --- |
| Space Grotesk font | accessibility.grotesk | Switch when the font is installed (value On/Off), else Value (status) | Download font / Try font download again / progress / On / Off | done | `MenuState::fontAvailable` (= the status text is On or Off); A toggles the font, or downloads it when missing |
| Font size | accessibility.fontsize | Value, cycles | Standard (100), Larger (115), Largest (130) | done | none |
| Hint size | accessibility.hintsize | Value, cycles | Standard, Larger, Largest | done | none |
| High contrast | accessibility.contrast | Switch | Off | done | none |
| High contrast color | accessibility.contrastcolor | Value, cycles | White, Pink, Yellow (default), Cyan, Lime Green, Electric Blue, Orange, Lavender (the framework's list is the same) | done | disabled (greyed, still focusable, A does nothing) while High contrast is Off |

### Accessibility: function parity (Classic `src/legacy/SettingsScreen.cpp` 462-511 and 1037-1041, `src/legacy/App.cpp` `activateSpaceGrotesk()`/`spaceGroteskStatus()`/the download's completion in `run()`, `src/legacy/SpaceGroteskDownload.*`, `src/legacy/Theme.*`, `src/legacy/Preferences.*`; port in `src/app/settings/AccessibilitySettings.*`, adapter `src/legacy/bridge/settings/AppAccessibilitySettings.*`, framework `src/ui-framework/kit/Hints.cpp`, look bridge `src/app/Look.*` / `src/legacy/bridge/LookBridge.*`)

No page, dialog or flow: every row acts at once. Details are in the owner's spec (the Classic source read function by function). Throwaway equivalence tests (host): 313 checks for the category and the
framework, 46 for the adapter against the real `Preferences` and the real Classic `Theme`, all passing. "Host" = `make -C src/app run ARGS="--tab settings --category accessibility --keys right,... --set accessibility.<key>=<value>"`
(`--list-samples` names them: `accessibility.font=on|off|missing|downloading|failed|failing|nodir`, `.auto`, `.step`, `.fontsize`, `.hintsize`, `.contrast`, `.contrastcolor`; `--look text=130,hint=115,contrast=2` starts the sample from a look).

| Classic function (source) | New location | Status | Verification |
| --- | --- | --- | --- |
| Five rows, order, labels (`buildRows` 462-511) | `AccessibilitySettings` table | done | test: every label is a Classic row label; host render |
| Help texts (`settingHelp` 124-133) | table help keys | done | test: each text equals the Classic source string byte for byte |
| Row 1 value text (`spaceGroteskStatus` 455-461): `Downloading n/m`, `Try font download again`, `Download font`, `On`, `Off` | `AccessibilityService::fontStatus()` -> `Context::settingsHost.spaceGroteskStatus` (App's own function); cached 0.5 s (the file reads) | done | host `--set accessibility.font=...`; device |
| A text "On"/"Off" is drawn as a switch (`renderOptions` 2319) | `state.fontAvailable = (status is On or Off)`, row kind Switch | done | host render |
| A on row 1 (`activateSpaceGrotesk` 463-487): running -> toast status; installed -> toggle + `setUseSpaceGrotesk` + `setSpaceGroteskEnabled` + toast; missing -> `start()` + toast | `activateFont()` -> `settingsHost.activateSpaceGrotesk` = App's function, unchanged (no second `SpaceGroteskDownload`) | done | host sample replicates the texts (test); device for the real download |
| Download stepping and completion (`run()` 1485-1497: one `step()` per frame behind scan, scraper, icon jobs; Done -> enable + "Space Grotesk font installed and enabled", else error) | stays in `App`, untouched | done | device |
| Toast texts: `Space Grotesk font enabled` / `disabled`, `Downloading the Space Grotesk font…`, `cannot create the font directory`, `Space Grotesk font installed and enabled`, `could not install <file>[: <reason>]` | Classic `Context::notify` -> `PatsGuiScreen` shows each as a toast | diff FA1 | test (sample texts and seconds); device |
| Font size / Hint size A (472-492): Standard -> Larger -> Largest -> Standard, `setFontSizeScale` / `setHintSizeScale` on `Preferences` and Classic `Theme`, `onThemeChanged` | `current()` (percent -> index), `changed()` (index -> 100/115/130) -> `AppAccessibilitySettings::setFontSizeScale/setHintSizeScale` | done | test: cycles 100/115/130; adapter test: Preferences, Theme, saved `fontSizeScale=` / `hintSizeScale=`, one `onThemeChanged` |
| High contrast A (497-502) | `changed()` -> `setHighContrast` (Preferences, Theme, `onThemeChanged`) | done | adapter test, saved `highContrast=1` |
| High contrast color A (506-511), cycles all 8 and wraps | `changed()` -> `setHighContrastColor` | done | test: from Yellow: Cyan ... Lavender, White, Pink, Yellow; adapter test: saved by name, Yellow not saved |
| Color row enabled only while High contrast is on, re-read every frame (1037-1041); A inert and silent when disabled | `Cond::HighContrastOn` + `update()` copies `highContrast()` into `MenuState` every frame | done | test: inert while off; enabled by a preference that was already on |
| Live text scale (`Theme::fontPx`, `sizeHint`) | `src/app/Look.cpp` `applyLook` (foundation), read from `Preferences` every frame by `PatsGuiScreen::syncLook` | done | test: `ui::Theme` sizes equal Classic `Theme` for text x hint in {100,115,130}, at 720p, 1080p, 2160p (Heading, Tab, Body, Small, Clock, Title, Hint) |
| Live high contrast and its color (`Theme::setHighContrast*`) | `applyLook` (same call carries the highlight) | done | test: the 8 highlight names, order and rgb equal Classic's; default Yellow |
| Font reload (`Theme::setSpaceGroteskEnabled`, 90 % glyph scale for Space Grotesk) | `applyLookFonts`, from the Classic theme's resolved paths | done | foundation; device |
| Hint line steps its size down to the Small size when it would not fit (`App.cpp` 1243-1247), then drops hints | `src/ui-framework/kit/Hints.cpp drawHintLine` | done | test: widths at each text x hint, largest size that fits, never below Small |
| High contrast hint line: bold, Primary, framed (`App.cpp` 1276-1290: surfaceHi, 2 px focus outline, radius 6, padding 10 x 4, 10 px beyond the margin; not around the hold hint) | `src/ui-framework/kit/Hints.cpp drawHintLine(frame)`; `drawHoldProgress` draws none | done | test: edge and fill pixels for all 8 colors, no frame outside high contrast; host renders at 100/115/130 hint size |
| Persistence keys `useSpaceGrotesk=0`, `fontSizeScale=`, `hintSizeScale=`, `highContrast=1`, `highContrastColor=<name>` | same `Preferences` setters | done | adapter test reads the written file |

Known Classic oddities carried over: the licence file is downloaded last, so a failure on it alone leaves both TTFs installed, the row showing `On` and the built-in face still drawn (A1); an unknown stored
size cycles to 100 in Classic, to 115 here (not reachable: `Preferences` accepts only 100/115/130).

## Updates (rows are rebuilt from the update snapshot; `MenuState` carries it)

| Classic row | id | kind | Condition (shown) | Enabled | Later |
| --- | --- | --- | --- | --- | --- |
| Check for updates automatically | updates.auto | Switch | always | yes | none |
| Version running now | updates.running | Info "v<version>" | always | inert | none |
| Version used on next start | updates.next | Info "Pinned v<x>" or "Newest v<x>" | always | inert | none |
| Newest installed version | updates.newest | Info "v<x>" | pinned and newest installed differs from the selected version | inert | none |
| Use newest installed version | updates.usenewest | Action | pinned | only in phase Idle, Current, Available, Blocked, Error, RestartRequired | removes the pin |
| (status text) | updates.statuswait | Info, label is the service text | phase WaitingForInternet or Checking | inert | none |
| Update progress | updates.progress | Info "42%", "Checking downloaded files...", "Installing update...", "Downloaded <size>" | phase Downloading, Verifying or Installing | inert | none |
| Cancel update download | updates.cancel | Info (greyed, no value, no glyph) | phase Downloading or Verifying (not Installing) | never: Classic gives it no value and no action; B cancels (App.cpp 990) | decision: wire the row or drop it |
| (status text) | updates.statusrestart | Info | phase RestartRequired | inert | none |
| Restart options | updates.restart | Action | phase RestartRequired and the success prompt is not showing | yes | the restart prompt (A restart, B later, X use newest and restart when pinned) |
| Check for updates now | updates.check | Action | phase is none of WaitingForInternet, Checking, Downloading, Verifying, Installing, RestartRequired (so also Confirming) | yes | starts the check |
| Install v<target> update | updates.install | Action, label from the target version | phase Available and the update can be installed | yes | release-notes confirmation (A install, B cancel, Up/Down read), then download |
| (status text) | updates.statusmisc | Info | phase Blocked, Error, or Available and not installable | inert | none |
| (repair paths, one row each) | updates.repair | Info | phase Blocked | inert | none |
| (status text) | updates.statuscurrent | Info | phase Current | inert | none |

Classic modals that belong to the update flow and are not first level: the install confirmation with release notes
(phase Confirming) and the restart-success prompt (phase RestartRequired with the prompt visible); while one is
open, Classic routes all input to it.

## System

| Classic row | id | kind | Classic value | Status | Condition / later |
| --- | --- | --- | --- | --- | --- |
| Disc drive | system.disc | Value | "A Status", or "No drive" | done | disabled with value "No drive" when no optical drive is present (re-evaluated every frame in Classic). Later: Disc drive status panel (drive, disc state, Physical-Disc Main, scoped [CD-*], PlayStation/Saturn MGL and RBF, project URL; A or B closes). Value text is "Status" here (Classic prints the A glyph inside the text) |
| Install/update Disc cores | system.disccores | Value (status) | "Install v0.7.0", "Repair/update v...", "Installed v...", "Working - n KB - A cancel", "Installing - finishing safely", "Recovery failed - package actions disabled", or the installer message | done | confirmation (A install, B cancel), then download, verify, install; A again during download cancels |
| Info | system.info | Action | A | done | System information panel: Machine, Processor, In use, Memory, Running for; Networking (Ethernet, Wi-Fi, Wi-Fi network); Storage per volume; Library (database, systems, favorites, imported list); Display (resolution, font, images cached); B closes |
| Time format | system.timeformat | Value, cycles | 24-hour, 12-hour AM/PM | done | none |
| Time zone | system.timezone | Value (status) | System default, or the zone | done | two-step chooser, region then city (A choose region / set zone, B back to regions / cancel); toast when the list cannot be loaded |
| Screensaver | system.screensaver | Value, cycles | Disabled, 30 seconds, 5, 10, 20, 30, 60 minutes | done | none |
| Screensaver style | system.saverstyle | Value, cycles | Dimmed, Boxart slideshow, Boxart collage | done | always shown and enabled in Classic, also with the screensaver Disabled (open decision: grey it out then) |
| Back up GUI settings | system.backup | Action | A | done | starts the backup to a USB drive |
| Download missing icons | system.icons | Value (status) | "N icons missing", "All icons ready", "Downloading icons...", "Try download again" | done | starts the download |
| Connect to Wi-Fi... | system.wifi | Action | A, or "No adapter" | done | disabled with value "No adapter" when no adapter. Later: Wi-Fi flow Scanning, Networks, name entry, password, country, test prompt, testing, review, failed |
| Return to MiSTer menu | system.mistermenu | Action | A | done | leaves the GUI at once |
| Reboot MiSTer | system.reboot | Action | A | done | confirmation (A restart, B cancel) |
| Read full changelog | system.changelog | Action | A | done | scrolling changelog viewer (B close, Up/Down read, A try again on error); toast when another online task runs |

Classic's Updates rows are built between "Back up GUI settings" and "Download missing icons" in the code but live in
their own category; the System order above is the displayed one. Classic's System list is long: it scrolls (no
wrapping).

## Developer

Classic pushes the category unconditionally (`SettingsScreen::buildRows`, `SettingsScreen.cpp:272`, rows `:639-643`); both rows are always enabled
and have value "A". The category stays visible in the new GUI (`Cond::Always`; `Cond::DevBuild` is the opt-in if the owner decides D44).

| Classic row | id | kind | Classic value | Status | Condition / later |
| --- | --- | --- | --- | --- | --- |
| Wireframes | developer.wireframes | Action | A | done | always shown and enabled. A (and Start) calls `DeveloperService::openWireframes()` |
| MiSTer Pats GUI (UI-Framework) | - | - | A | not ported (owner decision) | opens the new GUI from Classic; inside the new GUI it does not exist (opening the GUI from itself would rebuild it while its own handler is running) |

Function table (Developer category and the shell items that came with it; spec: `settings-spec-Developer.md`):

| Classic function (source) | New location | Status | Verification |
| --- | --- | --- | --- |
| `SettingsScreen::buildRows` Developer rows (`:272`, `:639-643`), help `settingHelp` (`:152-153`) | `DeveloperSettings` table, label and help text identical | done | host: `--tab settings --keys down,down,down,down,down,down,a` shows one row `Wireframes` with the help text |
| `App::openWireframes()` / `closeWireframes()` (`App.cpp`) | `DeveloperSettings::activate` -> `DeveloperService::openWireframes()` -> `AppDeveloperSettings` -> `Context::settingsHost.openWireframes` | done | host: `... ,a,a` or `...,right,start` prints "Wireframes opened 1 time(s)"; device: Wireframes opens over the new GUI and B returns to it |
| `App::openPatsGui()` | not ported | decision | the row and `SettingsHost::openPatsGui` are removed |
| `App::dispatch` Start -> Confirm unless `usesStartAction()` (`App.cpp` ~979) | `PatsGuiScreen::usesStartAction() = true`; `patsgui::handle`: Start is offered to the tab, else it is a Confirm; App converts Start only while an error dialog is up and ignores it during `commitRunning()` like Confirm | done | host: `--keys ...,right,start`; device: Start in a keyboard and the search (needs a pad: `--press` now has `start`) |
| `App::dispatch` Back: other tab -> Home, Home -> nothing (`App.cpp` ~1156) | `patsgui::handle` Back: other tab -> Home, Home -> the GUI closes (owner decision); hint "Leave" only on Home, "Back" elsewhere | done | host: `--tab systems --keys b` (tab home), `--tab home --keys b,b` ("closed") |
| `App::dispatch` update preamble: confirmation and restart prompt get every event | skipped while the new GUI is open (Quit still cancels the install); `commitRunning()` and the download-cancel on B stay | done | device: start an update from the new GUI, A/B/Up/Down reach its confirmation |
| `ErrorModal` drawn in the frame loop (`Context::showError`) | the takeover frame branches draw the Classic `ErrorModal` over the new GUI / Wireframes (painted when it appears or changes), input is already routed to it first | done | device: a failed game start from the new GUI shows the dialog; A, B or Start closes it |
| Frame loop order of the takeovers | Wireframes before the new GUI, as in `dispatch()` / `activeScreen()` (Wireframes opened from the new GUI is visible) | done | device |
| `Context::settingsHost.rawInputWanted` | cleared when the new GUI opens and closes | done | device: leave the new GUI from the controller wizard, the pad works in Classic |
| Look: `Theme::setFontSizeScale/HintScale/HighContrast/DarkMode/ColorTheme`, font files (`Theme.cpp`) | `src/app/Look.*` (`LookSettings`, `applyLook`, `applyLookFonts`), `src/legacy/bridge/LookBridge.*`, `PatsGuiScreen::syncLook()` every frame, `WireframesScreen::syncFonts()` | done (all 22 of Classic's color themes, same index: the framework carries them, `frameworkColorTheme()` is the identity) | host: `--look text=130,hint=130,dark=0,theme=3`; device: change Font size / High contrast in Classic, open the new GUI |
| `Context::notify()` toasts | not built here (waits for `ui::Toast` / `src/app/Notice.h`, see the report) | open | - |

## Source-derived behavior check (second pass, from `src/legacy/SettingsScreen.cpp` and `src/legacy/App.cpp`)

Every difference found between the first skeleton and the code, and what was done. "Shell" means it belongs to
`src/app/PatsGui.*` / `src/ui-framework/`, not to this tab.

| # | Classic (source) | First skeleton | Now |
| --- | --- | --- | --- |
| 1 | Column titles "CATEGORIES" (left) and "WHAT THIS DOES" (right), small bold muted, 34 px band; the rows column has no title but reserves the band (`render`, lines 2708-2720) | none | drawn as `settings.col.categories` / `settings.col.help` (framework Small, bold, Muted); rows column keeps the band |
| 2 | Three equal columns, `(w - 2 gap) / 3` (2701) | 24 / 44 / 32 % | equal thirds |
| 3 | Hint line is one fixed string for every focus and row kind: "Move  A Choose  Change column  B Back  LB/RB Change tab" (2824); while an update downloads or verifies: "B Cancel download  Change column  LB/RB Change tab" (2823) | per row kind (Turn on, Change, Open, ...) | exactly Classic's words: Move, A Choose, Change column, B Back; download phase: B Cancel download, Change column. The framework orders hints by key rank (Change column before A); LB/RB comes from the shell |
| 4 | Start acts as A in Settings (`App::handleAction` 961-963 turns Start into Confirm) | ignored | `Event::Start` = Confirm |
| 5 | While an update downloads or verifies, Back cancels the download on any tab (`App.cpp` 990-994) | B went back to the categories | Back in those phases calls `SettingsActions::activate("updates.cancel")` and is consumed. The same rule applies on every tab in Classic: shell |
| 6 | Developer category is pushed unconditionally (`buildRows` 272) | shown only in development builds | always shown; `Cond::DevBuild` kept as an opt-in |
| 7 | "Cancel update download" has no value and no action (869) | Action row with an A glyph | greyed Info row, no glyph |
| 8 | Space Grotesk row value is "On"/"Off" when the font is installed, which `renderOptions` draws as a switch (2319-2341), else a status text | always a status text | Switch while `MenuState::fontAvailable`, else status Value |
| 9 | Value "On"/"Off" is drawn with a checkbox for ANY row whose text is On/Off (2319), so Video sound "Off" gets a checkbox | Value text | not copied: Video sound stays a plain Value ("Off") |
| 10 | Back on the categories is not consumed (`wantsBack` = optionsFocused, 1151), and the app then selects the Home tab (App.cpp 1156) | not consumed; the shell leaves the GUI | tab unchanged. Shell: the new shell closes the GUI instead of going to Home; owner to decide |
| 11 | Up/Down skip headings only; disabled rows are reachable (1159, 1170) | same | verified, unchanged |
| 12 | Category change resets the row cursor to the first selectable row; no per-category memory (1163) | same | verified, unchanged |
| 13 | Right and A on a category enter the rows; a heading under the cursor moves to the first selectable row (1185); Left and Back leave them (1177, 1189) | same | verified, unchanged |
| 14 | A on a row runs `activate` only when the row is enabled and has an action (1197); Info/status rows have none | same | verified, unchanged |
| 15 | No wrap-around at either end | same | verified (`ListLayout::move`) |
| 16 | Help column: row label in Heading size, then help text; rows without help (status rows) show the label only; drawn only when the column is wider than 200 px (2723, 2370) | same | verified. The label wraps in the framework's text block |
| 17 | Rows rebuilt from the update snapshot only when phase, versions, status or paths change; the cursor is clamped (911-918) | rebuilt every frame from `MenuState` | same result; clamped in `clampRow()` |
| 18 | `update()` re-evaluates High contrast color (follows the preference), Wi-Fi (adapter) and Disc drive (drive state) every frame (1037-1052) | `MenuState` fields | same; the host supplies them through `setState()` |
| 19 | Options pane scroll is stateless: `first = max(0, cursor - visible + 1)`, so the cursor sits on the bottom row once the list scrolls (2283); no scrollbar or other position cue | framework `ListLayout` (smallest scroll) and a scrollbar | kept as the framework's look |
| 20 | Row look: rest fill alpha 110 on categories, 190 for the remembered row; disabled label is muted, value stays accent; Info rows with a value are drawn like other rows | framework ListItem | kept as the framework's look; disabled rows use the framework's Disabled tier |
| 21 | While the update confirmation (phase Confirming) or the restart prompt (RestartRequired with the prompt visible) is up, all input goes to them (App.cpp 967-989), and so do Core-updater, Disc, Info, Changelog, Time-zone, Reboot and Wi-Fi modals | not first level | not built; they belong to the submenu and flow work |
| 22 | Key repeat is the input layer's, not the screen's | n/a | not checked; host `Input` |

Not matched on purpose: the colors, fills and the A glyph (the framework draws them); the "A Status" text of the Disc
drive row (the framework cannot put a glyph inside a value, so the value reads "Status"); Classic's own color-theme
names (the framework's 8 are used).

## Navigation and hints

| Classic | New |
| --- | --- |
| Up/Down moves in the focused pane; changing category resets the row cursor to the first selectable row | same |
| Left/Right change column | Left: back to the categories. Right: into the rows (stays on the categories when the category has no rows) |
| A on the categories enters the rows; A on a row acts | same |
| B on the rows returns to the categories; B on the categories is unused, so the host leaves | same (`handle(Back)` returns false there) |
| Hint line "Move  A Choose  Change column  B Back  LB/RB Change tab" for every state | same words (see check 3) |
| Column captions "CATEGORIES" and "WHAT THIS DOES" | same |
| Help panel: row label and help text; rows without text (status rows) show only the label | same |
| Row values on the right in the accent color; On/Off with a round checkbox | framework ListItem: Value in the accent text color, Switch with checkbox and On/Off word |

## Sub-screen inventory: everything reachable from Settings

Derived from `src/legacy/SettingsScreen.cpp` and `src/legacy/App.cpp` (the ~30 constructor callbacks of `SettingsScreen`, line numbers as of
this writing). One entry per Classic sub-page, modal or flow, with the function that implements it and the category that owns
its port. "Full screen" means Classic's `App` hands the whole screen to another `Screen` subclass; in the new GUI every one
of them is a `SettingsPage` or a chain of them (`src/app/settings/SettingsPage.h`). Status: **todo** unless noted.

### Library (`src/app/settings/LibrarySettings.*`, service `LibraryService`)

| Row | Classic flow (source) | Pages in the new GUI |
| --- | --- | --- |
| Build game list | `App::openScan(false)` -> full screen `ScanScreen` (`reset(false)`): pages Intro, Working, ArtworkOffer, ArtworkWorking, Overview, Done (`ScanScreen::handle`, `renderIntro/renderWorking/renderArtworkOffer/renderArtworkWorking/renderOverview/renderDone`); at close `App::closeScan()` reloads the library when artwork arrived | scan wizard: ProgressPage / ConfirmPage / ChoicePage (artwork system) / InfoPage-like result; Welcome and JumpStart pages are first-run only (not reachable from Settings) |
| Prepare box art | `App::openArtwork()` -> `ScanScreen::resetForArtwork()` starts at ArtworkOffer (system choice, `refreshArtworkSystems()`), then ArtworkWorking, then Done | part of the same wizard, entered at the artwork step |
| Reload saved game list | no page: toast "Reloading the saved game list…" (6 s), `setScanningAllowed(true)`, `App::reloadLibrary()`, toast "Saved game list loaded" (`SettingsScreen::buildRows`) | none (toasts) |
| Core and MRA updates | modal flow in `SettingsScreen`: `openCoreUpdater()`, `handleCoreUpdater()`, `renderCoreUpdater()`, dialogs Intro (network check `internetRouteAvailable()`, re-checked every 2 s), Checking (progress, `cancelCheck`), List (filter tabs Needs update/All/Up to date/Changed with Left/Right, A select, X select all, Y check again, Start review), Review (scrolling list, A install), Installing, Result (Try again re-opens Intro) | chain of pages over `CoreMraUpdater` state: Intro (ConfirmPage-like), Checking (ProgressPage), List (own Content page: filter tabs, grouped list), Review (own Content page), Installing (ProgressPage, not cancellable), Result (ResultPage) |
| Arcade game status | `App::openArcadeGames()` -> full screen `ArcadeGamesScreen` (table, Y filter All/Working/Broken, letter jump, A launches; toasts "Games starting with X", "Showing: ...", "Starting ..."); `ArcadeSettingsScreen` exists but nothing in Settings opens it | decision pending: the Arcade tab already shows this table (`Library::arcadeScan`); see the decisions list below |
| TheGamesDB | no page: A checks the key; toasts "Add your key to <path>" (8 s), the check status, "Checking TheGamesDB API key…" (4 s); `update()` toasts "TheGamesDB key works: …" (7 s) and refreshes disc artwork when a check succeeds | none (toasts) |
| ScreenScraper -> Video previews | no page: switch; on enables `forgetAccount()` + `checkAccount()`, off calls `preview.stop()` | none |
| ScreenScraper -> Account | no page: A re-checks the login (`forgetAccount()`, `checkAccount()`); the value is the account state | none |
| ScreenScraper -> User name, Password | `openAccountEntry()` -> keyboard modal `renderAccountEntry()` (Password masked, Start submits); on save `ScreenScraperSettings::save`, toast "ScreenScraper user name/password saved"; failure `showError("Could not save the login", ...)` | TextInputPage (+ ResultPage error) |

### Controllers (`src/app/settings/ControllersSettings.*`, service `ControllersService`)

| Row | Classic flow (source) | Pages in the new GUI |
| --- | --- | --- |
| Controllers | `App::openControllers()` -> full screen `ControllersScreen` with modes List, InputTest, Deadzone, Wizard, Bluetooth (`handleListAction`, `enterInputTest/updateInputTest/renderInputTest`, `enterDeadzone/handleDeadzoneAction/renderDeadzone`, `beginWizard/updateWizard/captureWizardStep/renderWizard/renderPadDiagram`, `enterBluetooth/updateBluetooth/renderBluetooth`). Toasts: "This controller has no analog stick to adjust", "Stick deadzone saved", "Button mapping cancelled — nothing was saved", "Button mapping saved"; errors "Could not save" (deadzone, mapping), "Could not start" (btpair). Needs raw input while the wizard runs (`wantsRawInput()`) | list page + five page families; the pad diagram is an application composition (not framework); raw input switch `SettingsHost::rawInputWanted` |
| Face-button labels | cycles xbox / playstation / nintendo; `Preferences::setFaceButtonStyle` + `setFaceButtonStyle()` (ButtonLabels.h) | none |

### Interface (`src/app/settings/InterfaceSettings.*`, service `InterfaceService`)

| Row | Classic flow (source) | Pages in the new GUI |
| --- | --- | --- |
| Manage Consoles | `App::openVisibility()` -> full screen `SystemVisibilityScreen` (scrolling list, A toggles a system, B closes; `App::closeVisibility()` refreshes the Systems tab) | one Content page (list with Switch rows) |
| Show Games tab / Show Arcade tab | `App::toggleGamesTab()` / `toggleArcadeTab()` (leave the tab when it was current) | none |
| Game view | `App::cycleDefaultView()` applies the view to every tab's screen and toasts "Game view: X" | none (toast) |
| Color theme / Dark mode | `Preferences` + `Theme` setters + `Context::onThemeChanged` | none; also applies to the framework theme of the new GUI |
| other switches and cycles | `Preferences` setters only | none |

### Accessibility (`src/app/settings/AccessibilitySettings.*`, service `AccessibilityService`)

| Row | Classic flow (source) | Pages in the new GUI |
| --- | --- | --- |
| Space Grotesk font | `App::activateSpaceGrotesk()`: toggles when installed (toast "Space Grotesk font enabled/disabled"), else starts `SpaceGroteskDownload` (toast "Downloading the Space Grotesk font…", errors as toasts); `App::update` finishes it ("Space Grotesk font installed and enabled") | none (toasts); value text from `spaceGroteskStatus()` |
| Font size / Hint size / High contrast / High contrast color | `Preferences` + `Theme` setters + `onThemeChanged` | none; the framework theme and fonts follow through the look bridge (`PatsGuiScreen::syncLook`), the hint line through `src/ui-framework/kit/Hints.cpp` |

### Updates (`src/app/settings/UpdatesSettings.*`, service `UpdatesService`)

| Row | Classic flow (source) | Pages in the new GUI |
| --- | --- | --- |
| Check for updates automatically | `Preferences::setCheckForUpdates`; off also `cancelAutomaticCheck()` | none |
| Check for updates now | `UpdateService::startCheck(false)` (+ `autoCheckDecisionMade_`); the rows below rebuild from the snapshot (`buildUpdateRows`, `setUpdateSnapshot`) | none: rows only |
| Install vX update | `beginConfirmation()` -> phase Confirming -> modal `renderUpdateConfirmation()` (release notes with scroll, A install, B cancel, Up/Down read); `startInstall()` -> Downloading/Verifying/Installing rows (B cancels download wherever the cursor is, `App::dispatch`) | InfoPage viewer with action (release notes), then rows (no page while downloading) |
| Restart options | phase RestartRequired + `successPromptVisible` -> modal `renderUpdateSuccess()`: A restart (`App::restartMisterAfterUpdate(false)`), B later (`dismissInstallSuccess()`), X use newest and restart (only when pinned); `App::update` jumps to Settings when the prompt appears | page with three actions (own page: A/B/X) |
| Use newest installed version | `App::removePinnedVersion()`; failure `showError("Cannot remove version pin", ...)`; success shows the same success modal with "Version pin removed" | ResultPage |

#### Updates: function parity (Updates agent; derived from `src/legacy/SettingsScreen.cpp`, `src/legacy/App.cpp`, `src/legacy/UpdateService.cpp`; all of it was compared with the source)

Files: `src/app/settings/UpdatesSettings.*` (rows, activate, state-driven dialogs), `src/app/settings/updates/UpdatePages.*` (the two dialogs, the error page),
`src/app/settings/UpdatesService.h` (unchanged), `src/app/settings/SampleUpdatesService.h` (every phase and guard of Classic's `UpdateService`),
`src/legacy/bridge/settings/AppUpdatesSettings.*` (the adapter: the same calls Classic makes, on the ONE `UpdateService` of `App`, `Context::settingsHost.updates`).
The update itself (check, download, verify, install, pin) is unchanged Classic code in `src/legacy/`.

| Classic function (source) | New location | Status | Verification |
| --- | --- | --- | --- |
| `buildRows()` first Updates row, `onToggleAutomaticCheck_` (`SettingsScreen.cpp` 625-627, `App.cpp` 275-279) | `UpdatesSettings::current/changed` -> `AppUpdatesSettings::checkAutomatically/setCheckAutomatically` (`Preferences::setCheckForUpdates`, off also `cancelAutomaticCheck()`) | done | host: toggle test; device: `preferences.txt` gains/loses `checkForUpdates=1` |
| `buildUpdateRows()` rows, order, conditions, value texts, enable rules (`SettingsScreen.cpp` 833-891) | `UpdatesSettings` table + `valueOf` + `refresh()` (MenuState) | done | host: the Classic function was compiled verbatim and compared with the new rows for 3072 phase/pin/prompt/progress combinations: labels, value texts, actionable flag, 0 mismatches |
| `settingHelp()` texts (`SettingsScreen.cpp` 160-179) | `strings()` (catalog keys `settings.updates.*.help`) | done | host: all 10 help texts and labels compared with the extracted Classic function; status rows have no help |
| `setUpdateSnapshot()` rebuild and cursor clamp (`SettingsScreen.cpp` 893-919) | `UpdatesSettings::refresh()` + `SettingsTab::clampRow()` (index based, like Classic) | done | host: behave test |
| `onCheckUpdates_` (`App.cpp` 280: `autoCheckDecisionMade_ = true; startCheck(false)`) | `activate("updates.check")` -> `AppUpdatesSettings::startCheck()` -> `SettingsHost::startUpdateCheck` | done | host: guard test (A twice = one check); device: texts per `settings-spec-Updates.md` 2.3 |
| `onConfirmUpdate_` (`App.cpp` 281) and the confirmation modal (`SettingsScreen.cpp` 1143-1151, `renderUpdateConfirmation` 2520-2639, hint 2815) | `activate("updates.install")` -> `beginConfirmation()`; `UpdateConfirmPage` (`InfoPage::viewer`, state-driven on phase Confirming): A `startInstall()`, B `cancelBeforeCommit()`, Up/Down read, title "Install v<target>?", the pin / restart line, the game-list line, the RELEASE NOTES heading, hints "Install update / Read release notes / Cancel" | done, diff FU1 | host: behave test + pictures (100 %, 130 %, pseudo-locale, 720p) |
| Back during Downloading/Verifying (`App::dispatch` 990-994, `SettingsScreen::hints` 2821-2823) | Classic `App` still does it while the new GUI is hosted; in the new GUI `UpdatesSettings::backActivates()` + `hintsHook()` on the Settings tab | partial: Settings tab only; "every tab" is the shell's (see Decisions U1) | host: B cancels from rows, from the categories; hint line "Cancel download / Change column" |
| "Update progress", "Cancel update download" rows (`SettingsScreen.cpp` 859-869) | rows only, no progress page; the cancel row is inert (disabled Info) | done | host: rows per phase (compare test); text "Downloaded N MB" (`bytesTotal` is never set by the real service, so the percent text is dormant) |
| `onShowInstallSuccess_`, `onDismissInstallSuccess_`, `onRestartAfterUpdate_` and the restart prompt (`SettingsScreen.cpp` 1131-1141, `renderUpdateSuccess` 2641-2689, hints 2817-2820; `App::restartMisterAfterUpdate` `App.cpp` 618-627) | `UpdateRestartPage` (state-driven on phase RestartRequired + prompt visible): A `restartAfterUpdate(false)`, B `dismissInstallSuccess()`, X `restartAfterUpdate(true)` only when pinned; "Restart options" row -> `showInstallSuccess()`; adapter: `prepareRestart` + `::sync()` + `::reboot(RB_AUTOBOOT)` | done, diff FU2 | host: behave test (A/B/X, pinned and not); device: the reboot |
| Error dialogs "Cannot restart MiSTer" (`App.cpp` 615, 621, 626) and "Cannot remove version pin" (`App.cpp` 632) | `pushUpdateError()`: `ResultPage` kind Error, title and body = Classic's text, A or B closes | done, diff FU3 | host: pages `updates/error.restart`, `updates/error.pin`; behave test (the prompt stays under the error) |
| `onRemovePinnedVersion_` / `App::removePinnedVersion` (`App.cpp` 629-633) and the "Version pin removed" prompt | `activate("updates.usenewest")` -> `removePinnedVersion()`; the same restart page with `pinRemoved` wording | done | host: success and failure; device: symlink removed |
| `App::update` polls the snapshot every frame and selects Settings when the prompt appears (`App.cpp` 1488-1494) | `UpdatesSettings::background()` (polled while the tab is hidden), `wantsAttention()` -> `PatsGui::update` switches to Settings; the page opens from the snapshot (`onShow`) | done, additive shell hooks (see Shared files) | host: behave test (hide, prompt, show) |
| Bottom bar "v<version>  ·  v<target> available" (`App.cpp` 1221-1222, 1281-1288) | `UpdatesSettings::footerNote()` -> footer text "v<target> available" (accent) next to the shell's own version text | diff FU4 | host: picture |
| Startup auto check after 8 s (`App.cpp` 1481-1487) | stays in Classic `App` (`updateCheckDelay_`, `autoCheckDecisionMade_`); the adapter never starts a check of its own | done while hosted; a standalone shell must provide it (Decisions U1) | device |
| `UpdateService`, `UpdateInstaller`, `UpdateFiles`, `VerifiedDownloader`, `ReleaseInfo`; loader pin (`Loader.c`) | unchanged `src/legacy/`; the adapter only calls them | unchanged | `tests/release_info_test.cpp` (existing) |
| Installing the patched `MiSTer_gui` on update (CLAUDE.md) | not done by Classic either: `UpdateInstaller` installs only the library, disc index and scripts and merely checks that `MiSTer_gui`/`mister-gui` exist (`UpdateInstaller.cpp` 162-374, `UpdateService.cpp` 140-142) | open (0.8.10 goal, `_vnext/vnext.md`) | when implemented it belongs into `UpdateInstaller`; this category needs no change |

### System (`src/app/settings/SystemSettings.*`, `src/app/settings/system/*`, service `SystemService`, adapter `src/legacy/bridge/settings/AppSystemSettings.*`; the changelog uses `UpdatesService`)

Derived from `src/legacy/SettingsScreen.cpp`, `src/legacy/App.cpp`, `src/legacy/Wifi.cpp`, `src/legacy/SystemInfo.cpp`, `src/legacy/DiscCoreInstaller.cpp`, `src/legacy/DiscCoreManager.cpp`, `src/legacy/BackupTask.cpp`,
`src/legacy/SystemIconDownload.cpp`, `src/legacy/TabIconDownload.cpp`, `src/legacy/UpdateService.cpp`, `src/legacy/Preferences.cpp`, `src/legacy/TopBar.cpp`. Line numbers are of the working tree at the time of the port.
The host-side equivalence checks (26 261 comparisons, throwaway, run before this report) compared a transcription of the Classic code with the new category:
row texts and enable rules over every installer state x message x blocked x bytes x package flags (896), the three cycling rows from every start value (12),
the time-zone chooser over 8 scripted sequences, the Wi-Fi flow over 22 scripted sequences (every step, every Back, every message, every service call and toast),
the Disc drive panel over 2 048 package/drive flag combinations, the install/cancel/backup/icon/reboot handlers (toast texts and seconds), the changelog text
preparation over the real `CHANGELOG.md` (946 lines), the Info labels and 3 024 clock/date strings. Result: 0 failures.

| Classic function (source) | New location | Status | Verification |
| --- | --- | --- | --- |
| `buildRows()` System rows: order, labels, help (`SettingsScreen.cpp` 515-649, `settingHelp` 134-183) | `SystemSettings.cpp` table | done | host: 13 rows, labels and help compared; `--tab settings --category system` |
| Disc drive row value/enable, re-evaluated every frame (515-521, 1046-1051) | `SystemSettings::refresh()` copies `disc().media` into `MenuState::discDrive`; row value "Status" / "No drive" | done, diff FS1 | host: 6 media states compared; device: plug/unplug the drive |
| Disc drive panel `renderDiscDriveSettings()` (2433-2488), A or B closes (1087-1091) | `SystemSettings::openDiscPage()`: `InfoPage::facts` (live, 0.25 s) with status tones, footer and URL (`InfoSpec::footer/footerAccent`, additive) | done, diff FS2 | host: all 8 lines over 2 048 flag combinations (text and tone as Classic's color rule: success iff the verified/configured/installed flag); device: against the files on the card |
| Install/update Disc cores row text (522-541) | `discCoresText()` into `state.status["system.disccores"]` every frame; the "A" of "A cancel" is the `{A}` placeholder (button name of the layout) | done | host: 896 combinations |
| Disc cores A handler (528-548), confirmation (1075-1086, 2490-2518), toasts | `openDiscInstall()`: busy -> "Installation is finishing safely; please wait." / cancel + "Cancelling disc package download…" (5 s); blocked -> the message (7 s); else `ConfirmPage` "Install USB CDROM Loader v0.7.0?" (A "Install package", B cancel) -> `startDiscInstall()` + "Installing verified Disc Drive package…" or the message (5 s) | done | host: 6 states, toast texts and seconds, B cancels, A starts |
| `DiscCoreInstaller` (thread, phases, journal, recovery, cancel) | untouched; `AppSystemSettings::discInstall()/startDiscInstall()/cancelDiscInstall()` map `Snapshot`, `busy()`, `recoveryBlocked()` | done | device: a real install, a cancel, the row texts |
| Info `facts()` (921-999), modal (2755-2774), SystemInfo 2 s cache | `SystemSettings::openInfoPage()`: `InfoPage::facts` (0.5 s) over `SystemService::facts()`; the adapter is `facts()` field for field; `SystemInfo::update` per frame while Settings is shown, as Classic | done, diff F5 | host: 25 entries, labels through the catalog; device: against top, free, df, ip |
| Time format (560-566) | cycling row, `current()`/`changed()` -> `Preferences::setUse24HourClock`; the shell clock polls the preference | done | host: both start values; shell clock, 3 024 strings |
| Shell clock (`TopBar::refreshClock`, `TopBar::update`) | `PatsGuiScreen::refreshSystemValues()` (24-hour "09:05", 12-hour "9:05 PM", date "Monday, 23. November"); refreshes at once when the format or `Preferences::timeZone()` changes | done | host: strings compared with TopBar's code; device: toggle the format, set a zone |
| Time zone row value (568-573) and chooser (574-591, 2143-2198) | `openTimeZone()`: two `ChoicePage`s; current region/city is the marked item and the start cursor; toasts "Could not load the time-zone list from MiSTer.", "No cities found in this region. Choose another region.", "Clock time zone set to <zone>", "Could not save the time zone. Check that MiSTer can write its system settings." | done, diff FS3 (D5) | host: 8 scripts compared event by event; device: set a zone, reboot, clock |
| `Preferences::setTimeZone` (`/media/fat/linux/timezone`, `TZ`, preferences file) | untouched; the adapter calls it | done | device |
| Screensaver (592-607) and style (608-620) rows | cycling rows; delays {0,30,300,600,1200,1800,3600}, styles dimmed/boxart/boxart-collage; the style row is enabled with the saver disabled | done | host: 7 + 3 start values compared |
| The screensaver itself (`App::run`: idle timer, trigger, wake, boxart update/draw) | untouched in `App`; works over the new GUI (see "Screensaver" below) | done | device |
| Back up GUI settings (621, `App.cpp` lambda) and the result report (`App::update`) | `activate`: `startBackup()` + "Creating settings backup…" (30 s); `update()`: `unreported` result -> "Backup saved to <folder>" (10 s) or an Error page "Backup not created" / "No settings were changed. Connect one writable USB drive and try again."; `acknowledgeBackup()` so App does not report it again | done | host: start, running (silent), saved (with and without a slash), failed once; device: no/one/two USB drives |
| Download missing icons (630, `App::startAssetDownload`, `assetDownloadStatus`, stepping and completion in `App::run`) | status row through catalog keys; `activate`: toasts per outcome (already running, started 4 s, failed 8 s with the error, nothing missing); completion toast 6 s / 8 s; the adapter reads the outcome back from App's state | done | host: 4 outcomes, completion, failure; device: remove an icon, run it, network off |
| System art cache after the download | `AppLibraryAdapter::update()` drops the `sys|` pictures and bumps the revision when `Icons::missingBitmaps().size()` changes (once a second, from memory) | done | device: console icons appear |
| Connect to Wi-Fi… row (633-636, 1043-1046) | `Cond::WifiAdapter`, value "No adapter" | done | host |
| `startWifiSetup()` (1804-1815) | `openWifiFlow()` / `startScan()` (`system/WifiFlow.cpp`) | done | host: start ok / fails (toast, no flow) |
| `handleWifiFlow()` Scanning, Networks, SsidEntry, PasswordEntry, CountryEntry, TestPrompt, Testing, TestReview, TestFailed (1829-1991), `update()` (1054-1067), `cancelWifiFlow()` | one page per step, one shared flow object; Back rules as Classic (TestPrompt, Testing, TestReview, TestFailed, Scanning, Networks cancel the whole flow; the entries return to the list, Country to the review); rollback in `onLeave` of the page that is still newest | done, diff FS4, FS5 | host: 22 scripts, 222 observations (titles, status lines, bodies, service calls, toasts) |
| `handleWifiFlow()` / `startWifiSetup()` / `cancelWifiFlow()` as a Wizard (`system/WifiFlow.cpp`, `WizardFlowPage`, one Medium frame; supersedes the page-chain row above and FS4) | presentation changed, function unchanged: pages Scan (centred Busy), Networks (ChooseList: network rows with security word + signal word and dBm, then "hidden" and "Scan again" rows as in Classic; unsupported networks are disabled rows with the reason in the status line), Ssid / Password / Country (`EntryPage`, status slot = Classic's message line, masked password), Prompt (centred, A Start / B Cancel), Testing (centred Busy; status from the service, empty = unchanged), Review (centred; A Save / B Cancel), Result pages Failed, SaveFailed, ScanFailed, Done. Classic's Back rules kept: Scan, Networks, Prompt, Failed end the whole flow; Ssid and Password go back to the list; Country to the review; Testing and Review end the flow and restore the previous connection; the restore also runs when the flow ends any other way while a test runs (tab or GUI closes). A wrong password length or country code wipes the typed text and stays on the page (no new page is stacked). Deliberate changes: (1) a scan that cannot start opens a Result page (Try again / Close) instead of a toast; a failed "Scan again" shows the error in the list's status line instead of a toast; (2) a failed save is the Result page SaveFailed (A Try again goes back to the review, B Close restores the previous connection) instead of the review with a message; (3) a saved network ends on a Done page instead of the 6 s toast; (4) A on an unsupported network is not offered (the row is disabled and the status line says why) instead of a message after A; (5) the hints say "Start", "Save", "Cancel" (closed vocabulary; `hint.save` added) | done | host: 15 scripted paths against the sample service (success, wrong password, cancel at every step incl. Testing, hidden, none found, no adapter, scan error, country needed, save fails, GUI closes during test/country, begin fails, stall): service calls and rollbacks counted |
| `Wifi` (wpa_cli, wpa_passphrase, dhcpcd, config file, rollback) | untouched; one `Wifi` in the adapter; poll tools as Classic (synchronous) | done | device: real network, wrong password, hidden, open, cancel at each step |
| Return to MiSTer menu (637, `App::startMisterCore`) | `returnToMisterMenu()` -> `settingsHost.startMisterCore` | done | host: immediate, no page; device |
| Reboot MiSTer (645, 1118-1126, 2732-2750, `App::rebootMister`) | `ConfirmPage` "Reboot MiSTer?" / "The system will restart now." (A "Restart MiSTer"); `reboot()` -> `settingsHost.rebootMister` (sync + reboot, error returned); failure `ResultPage` "Cannot restart MiSTer" | done | host: texts, B, A, failure; device (last) |
| Read full changelog (647-656, 1097-1109, 1645-1802, `UpdateService::startChangelog`) | `ChangelogPage` (an `InfoPage` viewer following `UpdatesService::snapshot()`): Loading / Error + A retry / text; `startChangelog()` false -> toast "Another online task is running. Try again when it finishes." | done, diff F3 | host: states, 946 lines compared; device: network on/off |

#### System: the small sequences as wizard flows, Info and Disc as pages (presentation changed, function unchanged)

Files: `system/TimeZoneFlow.cpp`, `BackupFlow.cpp`, `IconsFlow.cpp`, `DiscInstallFlow.cpp`, `RebootFlow.cpp`, `FlowParts.cpp` (`SystemFlows.h`); all `WizardFlowPage`, one Medium frame each. Classic's state machines, messages and Back rules are kept.

| Classic | New | Status |
| --- | --- | --- |
| Time zone chooser (region, then city; B on the cities = back to the regions; toast "Clock time zone set to X"; toasts for no list / no cities / not saved) | `TimeZoneFlow`: Region -> City; the zone in use is marked and the cursor starts there; set = the same toast and the flow ends; a region without cities = a status line on the region page; no list or not saved = a Result page (Failed, A "Try again" on the city page) | done; deliberate addition: LT/RT jump to the next/previous first letter on the city list (`ChooseList::setJumpByLetter`, hint "Jump to letter"), the list also scrolls with its scroll bar |
| Back up GUI settings (starts at once, no question; "Creating settings backup..." toast; Complete = a 10 s notice, Failed = an error dialog; the task cannot be stopped) | `BackupFlow`: Working (no Cancel) -> Result (Done: where it went; Failed: nothing changed, connect one writable USB drive, A "Try again"); the flow acknowledges the result, `SystemSettings::reportBackup` only catches a result nobody showed | done; no question added |
| Download missing icons (toasts: already running, started, all ready, failed to start; a notice at the end) | `IconsFlow`: Working (no Cancel, Classic has none) -> Result (Done, "All icons are ready", Failed with the service text as Details and A "Try again") | done; `reportIcons` removed |
| Install/update Disc cores: A on the row (busy: "finishing safely" toast or cancel + toast; recovery blocked: toast; else the confirmation, a toast, progress and "{A} cancel" in the row's value) | `DiscInstallFlow`: Confirm (A "Install", B "Cancel") -> Working (B "Cancel" = Classic's second A; no Cancel while installing) -> Result (installed, failed, cancelled, blocked). A on the row while it works opens the flow on Working; with recovery blocked on the blocked Result. The row value drops "{A} cancel" ("Working - N KB") | done; B (not A) cancels now |
| Reboot confirmation "Reboot MiSTer?" and the error "Cannot restart MiSTer" + OS text | `RebootFlow`: Confirm (A "Restart", B "Cancel") -> Failed Result with what changed, next step and the OS text as Details, A "Try again" | done |
| Info and Disc drive panels (A/B close); changelog viewer | stay pages (`InfoPage`): the facts list scrolls (`ui::drawKeyValueList` with `KeyValueView`: scroll bar, hint "Scroll", rows grow with the text size), Info and Disc share one value column, plain values line up with status words, the footer sits under the list; the viewer is a left-aligned reading column with its scroll bar next to it, Loading / Error / Empty centred like Busy / Result pages, hint "Scroll" | done |

#### Screensaver and clock over the new GUI (System)
- The screensaver lives in `App::run` (idle timer reset by any `Action`, trigger from `Preferences::screensaverDelay()`, wake swallows the first press, boxart update and draw), above the
  screen branches, so it already works over `PatsGuiScreen`; the two rows only write the preferences (read live each frame). No new-GUI code is needed. Checked in the source: the dimmed
  style darkens `canvas_`, which holds the new GUI's last frame; after wake `needsFullRedraw_` makes `PatsGuiScreen::renderFull` repaint. Open on the device: a saver that starts in a frame with
  `needsFullRedraw_` set repaints Classic's top and bottom bar under the dimming (`App.cpp` dimmed branch; `PatsGuiScreen::render` is empty); not changed here (shell/App owner).
- The shell's clock follows Time format and the time zone (`PatsGuiScreen::refreshSystemValues`, Classic's `TopBar::refreshClock` text and date line) and refreshes at once on a change.
- `App::update` still reports the backup result and the icon download completion through Classic's `notify` / `showError`; the System category reports them itself while the Settings tab is shown and
  acknowledges the backup result (`SettingsHost::acknowledgeBackup`) so App does not report it a second time; with another tab shown App reports it (the shell's notice bridge and `ErrorModal`).

#### Deliberate choices (System) that follow the owner's decisions on the D-list
- D1 (the password stays masked after a length error), D2 (the network list line is reset to "Choose the Wi-Fi network to join." on Back), D4 (the page stack locks the tabs; a running
  Wi-Fi test is rolled back when a page leaves), D5 (the region page is kept under the city page and the current item is marked): implemented as decided.
- D3 / D13: failures stay Classic's toasts and row texts (backup failure is an Error page as Classic's `showError`).

#### Framework-forced differences (System)
See FS1 to FS7 in the table below.

#### Open points for the owner (System)
See S1 to S5 in "Decisions for the owner".

### Developer (`src/app/settings/DeveloperSettings.*`, service `DeveloperService`)

| Row | Classic flow (source) | Pages in the new GUI |
| --- | --- | --- |
| Wireframes | `App::openWireframes()` -> full screen `WireframesScreen` | none (host hook) |
| MiSTer Pats GUI (UI-Framework) | `App::openPatsGui()` -> the new GUI itself | not ported: redundant inside the new GUI (owner decision) |

### Cross-cutting behavior of the Classic Settings screen to carry over

- `SettingsScreen::wantsBack()` lists every sub-screen that owns Back (options focus, Wi-Fi, account entry, time zone, reboot,
  changelog, info, disc drive, disc install, core updater): in the new GUI an open page owns every event (`PageStack`).
- `wantsStartForWifiPassword()`: Start submits a typed password or account entry; elsewhere Start acts like A (`App::dispatch`).
  The framework's `TextEntryDialog` maps Start to Done for every field (a listed difference).
- `usesStartAction()`: in the core-updater List Start opens the review.
- `App::dispatch` gives the update confirmation and the restart prompt every event, above every other screen, and B cancels a
  running download from anywhere; `App::update` selects the Settings tab when the restart prompt appears.
- The update service, the backup, the icon and font downloads, the disc installer, TheGamesDB and ScreenScraper checks run in
  threads of their own; the screen only polls (`update()`).

### Framework-forced differences so far (extend per category)

| # | Difference | Why |
| --- | --- | --- |
| F1 | A confirmation's hint words are the page's own (`confirmHint`), the layout of the dialog is `ui::Modal` | framework dialog |
| F2 | ChoicePage rows are two-line `Choice` rows (`Modal::choose`), Classic's chooser rows are one line | framework dialog |
| F3 | Text viewers use body text, not small text; blank lines are one line high | accessibility rule 1.3 |
| F4 | `TextEntryDialog` maps Start to Done for every field; a failed validation is a notice modal over the keyboard, not a line in it | framework dialog |
| F5 | Info facts are clipped at the bottom like Classic; the framework KeyValueList has no scroll (Largest text may hide rows) | framework gap, owner to decide |
| FS1 | The Disc drive row's value is "Status" without the A glyph ("A Status" in Classic); the Action rows draw the framework's A glyph | a glyph cannot sit inside a value |
| FS2 | The Disc drive panel is a full-content page (header "Disc drive") with `ResultItem`-style status lines, label column 38 %, step per framework; the footnote and the project URL are the additive `InfoSpec::footer` / `footerAccent`; button words come only from the hint bar (A/B Close) | framework page |
| FS3 | The time-zone chooser is two framework `Choose` dialogs: the current region/city is marked (and the cursor starts on it), Back from the city list keeps the region cursor (owner decision D5), the rows are two-line choice rows; the in-panel footers ("A Set Linux time zone") are the hint bar's words ("Set this time zone", "Back to regions", "Choose a region") | framework dialog |
| FS4 | The Wi-Fi flow's pages: Scanning and Testing are full-content progress pages (no bar fraction, the hint carries "Cancel" / "Cancel and restore the previous connection"; Classic's "Searching for Wi-Fi networks…" is the page's status line); the network list is a `Choose` dialog (rows: SSID and "Secured   -48 dBm" as two lines); its status line, and the message line under the keyboard, are the additive pill under the dialog (`ChoiceSpec::message`, `TextInputSpec::message`); the keyboard's prompt lines ("Wi-Fi network name", "Two-letter country code (for example, US)", "Enter password…") are the field's placeholder, the title is the Classic panel title; Classic's `A Try again   B Cancel` on the failed page reads "A Try again   B Close" | framework dialogs |
| FS5 | In the SSID and country entries Start is Done (Classic: Start types a key there, and submits only in the password entry); a typed SSID stays after a length error (Classic too) but the keyboard cursor starts again at the first key | `TextEntryDialog` (F4) |
| FS6 | The changelog viewer scrolls by one wrapped line like Classic, but shows the framework scrollbar instead of the arrows; heading text is bold body size and body text is body size (Classic: small) | accessibility rule 1.3 (F3) |
| FS7 | Texts that come from Classic's service layer stay as data in English (Info values such as "Offline", "Not connected", "not built"; the installer's messages; Wifi's status lines and errors; the icon-download error): they are not catalog entries. Labels, headings, the disc state words and the icon status words are catalog keys | the services return finished sentences, as in Classic |
| F6 | `PageStack::toast()` shows the shell's `ui::Toast` (`Services::notice`); Classic's seconds are a minimum, the toast's own policy (3.5 s plus 1 s per further 40 characters, elided at 60 % of the width, held back while a dialog is open) is the floor | `ui::Toast` owns duration and width (Toast.md) |
| FA1 | Classic's Accessibility messages (`Context::notify` with 3.5, 4, 6, 6, 8 s) are shown by the shell's toast, which stays at least as long as Classic's seconds (the policy of 3.5 s plus 1 s per further 40 characters is the floor) and elides at 60 % of the screen width | `ui::Toast` owns duration and width (Toast.md, accessibility 2.4) |
| FA2 | The bottom-bar hint icon (D-pad) follows the hint text size (`max(size, 18)`); Classic keeps it at `18 x hint scale`, also while a long line steps the text down | `drawHintLine` icon rule |
| FA3 | In high contrast the palette values (background tiers, muted text) come from `makeHighContrastPalette` (contrast-checked), not Classic's `applyPalette`; the eight highlight colors are identical | framework palette |
| FA4 | With the built-in face (Space Grotesk off or missing) the new GUI uses the same text roles as with Space Grotesk; Classic switches `sizeGameLabel()` / `sizeGameDetail()` to 18 / 34 px for the built-in face | one type scale (no per-face sizes) |
| FU1 | The release-notes confirmation is a full-content `InfoPage` viewer (header "Install v<target>?"), not a modal over the three panes; the RELEASE NOTES caption is a bold heading, inline bold (`**`) in the notes is dropped, the game-list line is not accent colored; scroll arrows are the framework scrollbar; the hint word is "Read release notes" through the additive `InfoSpec::scrollHint` | framework viewer (Updates) |
| FU2 | The restart prompt lists its three choices as words ("Restart MiSTer / Starts v..", "Not now / Continue running v..", "Remove pin and restart MiSTer / ..") and the buttons only in the hint bar (A, B, X follow the layout); Classic prints "A  ", "B  ", "X  " in the panel. Blocks are separated by a one-line gap (a no-break-space line, because `wrapText` drops empty lines); panel size is the framework's | framework dialog (button names only in hints) |
| FU3 | "Cannot restart MiSTer" and "Cannot remove version pin" are `ResultPage` errors (A or B closes), not the Classic `ErrorModal` through `Context::showError`: while the new GUI is hosted by Classic `showError` is invisible and swallows input (`App::dispatch` handles `errorActive` first) | charter section 6 |
| FU4 | The update-available note is its own footer text "v<target> available" (accent); Classic prints `v<version>  ·  v<target> available` as one text | shell footer has its own version text |
| FU5 | The GUI update is a Wizard flow (`updates/UpdatePages.cpp`, one Medium frame, title "GUI update"), presentation changed, function unchanged (supersedes FU1-FU3 where they differ): Confirm (Capture page: "Install vX?", pin/restart notice, rebuild notice, scrolling release notes, `A Install`, `B Cancel`, `Up/Down Scroll`; Start = A; A only starts when `startInstall()` accepts), one centred Busy page for Downloading/Verifying/Installing (status per phase, detail "N%" or "Downloaded N MB" updated once a second, "Keep MiSTer powered on." while installing; B = `cancelBeforeCommit()` while Downloading/Verifying, then "Stopping the download" until the snapshot leaves the phase; no B while Installing), Restart prompt as rows "Restart now" / "Later" / "Use newest version and restart" (pinned only), hints `A Choose  B Later` (the same words; Classic said "Not now" in the text and "Later" in the hint), Start = Restart now and X = remove pin and restart (pinned) still work, B = `dismissInstallSuccess()`. The flow is opened from the snapshot (Confirming, a busy phase, prompt visible) and ends when the snapshot stops saying so. Failures are Result pages with plain words, "Nothing was changed ...", a next step and the raw service text as Details: restart failed (A Try again = the prompt), pin removal failed (A Try again removes again, then the prompt), and a deliberate addition: an update that ends in phase Error shows a Result page (Classic showed only the row's status text). No download size on the Confirm page: the snapshot has none before the download. B on the Confirm page cancels the confirmation also when the GUI is closed under it only for B (closing the GUI leaves the phase Confirming, as Classic does) | owner premise, audit library-updates |
| F7 | The Controllers list scrolls (Classic cut off the rows that did not fit); rows are framework `ListItem` rows; the detail panel is a framework surface | framework list |
| F8 | The input test's instruction "Press buttons or move sticks to test" is a text line under the header and its exit is the hint `Hold Start Exit` (the framework writes holds as "Hold <key> <action>", without "5s"); the wizard's capture hints are `Start Skip this button` and `Hold Start Cancel setup` | a hint needs a key and one wording for holds (Hints.md) |
| F9 | Button names are printed without parentheses (`A Back`, Classic `(A) Back`) and the hints are ordered by key rank (X, Y, A, B; Classic: A, X, Y, B) | Hints.md |
| F10 | The Bluetooth pairing dialogs are framework modals over the list (Classic: a panel on a blank screen); "Scanning..." shows its three lines as one body in one tier and no bar; NoBluetooth closes with A or B (Classic: B only); `Could not save` and `Could not start` are `ResultPage` errors (A or B closes, as Classic's `ErrorModal`) | framework dialogs |

### Decisions for the owner (found while deriving the inventory)

1. **Arcade game status:** Classic opens a full-screen table; the new Arcade tab already is that table. Port as a page in Settings,
   or make the row switch to the Arcade tab? **Decided by the owner:** the row switches to the Arcade tab and opens its status screen (done, FL7); whether Back should return to Settings is still open.
2. ~~**Developer -> MiSTer Pats GUI (UI-Framework)** opens the new GUI from inside the new GUI.~~ Decided by the owner: the row is removed inside the
   new GUI; Wireframes stays; the Developer category stays visible (Classic shows it unconditionally; D44 may hide it later).
3. **Toasts:** `src/ui-framework/docs/components/Toast.md` says to drop progress toasts and turn failure toasts into Error modals; the 1:1 rule keeps
   Classic's messages. The charter keeps Classic's wording and timing and lists each message here; confirm whether the toast
   triage applies.
4. **Release notes** (update confirmation) use inline bold; the changelog does not. The framework has one text viewer without
   inline emphasis: accept plain text for both?
5. **Core-updater List** is a grouped, filterable, multi-select list with two-line rows: there is no framework component for it; it is an
   application composition from `ListItem`, `TabRow` and `ListLayout`: confirm.
A1. **Accessibility, licence last.** The download fetches Bold, Medium, then the licence. If only the licence fails, both fonts are installed but Classic neither enables nor reloads them: the row says `On` while the built-in face is still drawn until the next start or an Off/On. Ported 1:1; keep, or fetch the licence first / treat it as optional?
A2. **Accessibility, the font download blocks.** `SpaceGroteskDownload::step()` runs curl (then `curl -k`, then wget) with `std::system()` on the UI thread: the GUI freezes for the length of one file (about a second; up to 30 s plus the connect timeout offline). One file per frame, only when no scan, artwork or icon job runs. No cancel. Ported 1:1 (decided); a worker thread would be a later change.
A3. **Accessibility, download verification.** The only check on a downloaded font is the TrueType signature (00 01 00 00), and the `curl -k` fallback connects without certificate checks; AGENTS.md says to verify downloads (as `VerifiedDownloader` does for updates). Classic behavior kept: decide whether to pin hashes for the two fonts.
A4. **Accessibility, "Off" is the bitmap face.** Turning Space Grotesk off (or having no files) draws Classic's built-in 8x8 face everywhere, blocky and all caps in the new GUI too; the row's help does not say so. Keep, or word the help?
A5. **Accessibility, failure as a toast** (owner decided: Classic's toast). The shell toast shortens and elides it (FA1); consider an Error dialog later (decision 3).
S1. **System, Wi-Fi: blocking tools.** `Wifi` runs wpa_cli, wpa_passphrase and dhcpcd synchronously on the UI thread (a few hundred milliseconds per call, more when the
   supplicant is slow); `SystemInfo` forks `wpa_cli` every 2 s while the Settings tab is shown; the icon download blocks up to 30 s per file. Ported as Classic does (1:1). An asynchronous
   version is a later optimization: decide.
S2. **System, Info refresh.** Classic updates `SystemInfo` whenever the Settings screen is shown, not only while Info is open (the cost of S1); ported as is. Polling only while Info is open would
   need the first "In use" sample about 2 s after opening: decide.
S3. **System, failures as toasts.** The icon download failure and the Wi-Fi save/test messages are toasts or lines as in Classic; Toast.md would make them Error modals (decision 3 above).
S4. **System, tab icons.** "Download missing icons" counts and downloads Classic's tab-icon BMPs; the new GUI draws its tab icons itself. Kept until Classic is retired.
S5. **System, Info rows that do not fit** are clipped at the bottom like Classic (F5); with many USB drives the Library and Display groups can be hidden. Add scrolling to the Info page?
U1. **Updates, global behavior.** Classic's `App` does five things on every tab, not in the Settings screen: polls the update snapshot, starts the
   automatic check 8 s after start, lets Back cancel a download on any tab (before every other screen and dialog), ignores A and Quit while the
   install commits, and switches to Settings when the restart prompt appears. While the new GUI is hosted by Classic `App` keeps doing the first
   four (so Back during a download cancels it even with a Settings page open, as in Classic). The fifth and the footer note are done in the new
   shell (`TabView::wantsAttention()`, `TabView::footerNote()`, `SettingsCategory::background()`). A standalone new-GUI shell (0.9.0, no Classic
   `App`) must provide all five itself: confirm.
U2. **Raw error texts.** Classic shows the service's raw error text as the status row ("curl failed (28)", "SHA-256 mismatch: <path>", the 180-character
   cut) and as the body of the two error dialogs; AGENTS.md asks for plain words with a Details line. Ported as is: decide whether to add plain-words framing.
U3. **`MiSTer_gui` is not installed by an update** (finding in `CLAUDE.md` vs `UpdateInstaller`): the owner's 0.8.10 goal is still open.
U4. **Restart prompt without a target version:** when the GUI starts with running != selected (loader fell back), `RestartRequired` is set with an empty
   `targetVersion`, and "Restart options" opens a prompt titled "v installed" (Classic bug, ported). Fix?

## Owner changes after the 1:1 port (2026-11-23)

| Classic | New | Why |
| --- | --- | --- |
| Prepare box art: one system or all, chosen with Left/Right | A list with multiple selection; "All systems" is chosen by default, a chosen system takes it off, choosing "All systems" clears the others, clearing the last system chooses "All systems" again; A toggles, Start downloads (`LibraryService::startArtwork(keys)`, `MediaScraper::start` already took a list) | owner request |
| Reading column at the left of the page | The column (about 900 px) is centred with the same padding left and right; the text stays left-aligned (`ui::readingColumn`) | owner request |

## Follow-up pass: hints, captions, Nav with value (owner decision, 2026-11-23)

| Classic | New | Status |
| --- | --- | --- |
| first-level hints `A Choose  <-/-> Change column  B Back` for every focus | by focus: categories `A Open  B Leave`; rows `A Turn on/Turn off` (switch), `A Change` (cycling value), `A Open` (value that opens a page), `A Choose` (action); no A on a disabled or Info row; no column hint; B Back | deliberate change (Hints.md vocabulary) |
| value rows that open a page look like cycling rows | `ListItemKind::Nav` with its value left of the A glyph (Time zone, TheGamesDB, Install v..., Scraped videos...); cycling rows stay Value | deliberate change |
| a disabled row says nothing | the help pane adds "Not available: <reason>" (no Wi-Fi adapter, no optical drive, High contrast off, update running) | deliberate addition |
| column titles upper case, none over the rows | Title Case, all three at the same inset, the middle one names the category | deliberate change |
| scrollbar strip only when a list scrolls (values jumped by 10 px) | the strip is always reserved | deliberate change |
| Manage Consoles: page title "Systems", hint "Show or hide this console" | title "Manage Consoles" (the row's name), faint panel, hints `Move  A Select  B Back` | deliberate change |

## First start (Welcome, Jump Start, overview)

Classic: `ScanScreen::reset(true)` from `App::init` (`openScan(true)` when `!onboardingComplete()` and no older library exists; `setOnboardingStarted(true)` first), pages Welcome, JumpStart, Working and ArtworkWorking in the jump-start variant (`jumpStartFlow_`), Overview, Done (jump-start failure) (`ScanScreen.cpp` 16-26, 58-134, 183-324, 360-444, 517-545, 566-609). Port: `src/app/FirstRun.{h,cpp}`, hosted by the shell (`State::firstRun`), started by `App::init` through `PatsGuiScreen::startFirstRun()` when the new GUI starts; `--classic` keeps Classic's pages.

| Classic | Port | Status | Notes |
| --- | --- | --- | --- |
| Welcome: A -> JumpStart, B -> `onOnboardingComplete_()` + close | Welcome page: A Continue, Start acts as A, B Skip -> `setComplete()` once + `libraryTaskClosed()` + the flow ends | done, host | presentation changed: a centred page (symbol, headline, two sentences) instead of six lines of text; the tab/hints/Settings guide is the two sentences |
| JumpStart: A -> `scan_.startJumpStart(roots_, 20, 20)`, B -> complete + close | Jump Start page: A Start -> `LibraryService::startJumpStart()` (`detectRoots()`, 20, 20), B Skip -> complete + close | done, host | roots are detected when the scan starts (Classic: when the screen opened; an empty list is detected by `LibraryScan` anyway) |
| (none: the guide never downloaded anything itself; on a fresh device the GUI came up with the built-in font and no icons until `tools/install-update.sh` assets or Settings rows were used) | Assets step between the offer's Start and the scan (owner decision): one centred Busy page "Getting the font" (Space Grotesk + licence, `AccessibilityService::activateFont()`; App enables it when it arrived, `syncLook` applies it) then "Getting icons" (system + tab icons, `SystemService::startIconDownload()`); a part with nothing missing is skipped, with nothing missing no page shows; B Cancel leaves the step (the downloads cannot be stopped, App finishes a running one); a failure never stops Jump Start, the Overview adds "Icons and the font could not be downloaded. You can get them later in Settings." | done, host; deliberate change | additive seams: `IconDownloadInfo::missing`, `AccessibilityService::fontDownload()` (bridge hooks `assetsMissing`, `spaceGroteskRunning/Failed/Error`); on the device not tested |
| Working (jump start): bar, status line, "N titles found across M systems", scrolling list of systems with their status (Up/Down), B -> `scan_.cancel()` | the scan's Busy page of "Build game list" (`addScanWorking`): status phrase, bar, one counts line "N systems found - M games found", B Cancel -> "Stopping" | done, host; deliberate change | the list of systems does not fit a Busy page (owner decision): counts only; no scrolling |
| after the scan: games -> `scraper_.start` (boxart, backgrounds off), ArtworkWorking; none -> Overview (`jumpStartNoGames_`); a scan that failed or was stopped -> Done | `afterScan` of the working page: games -> `startJumpStartArtwork()` + Covers; none -> Overview; Failed -> Failed page | done, host | |
| ArtworkWorking (jump start): "N of M covers checked", B -> `scraper_.cancel()`, ended -> Overview | the box-art Busy page (`addArtworkWorking`), B -> Stopping/ends -> Overview | done, host; deliberate change | counts line is the Library flows' "Covers downloaded N - Prepared M" |
| Done (jump start, scan not Done): "The library could not be read", error text; A -> start again, B -> complete + close | Failed page (Result): Error "The library could not be read" with the service text as Details, or Warning "Jump Start stopped" when the user pressed B on the scan; A Try again -> `startJumpStart()` + Working, B Close -> complete + close | done, host | presentation: a stopped scan is told apart from a failure (as in "Build game list") |
| Overview: "Your Jump-Start is ready" / "No games found yet", "When you're ready" (three rows), covers failed note, A -> complete + close (B did nothing) | Overview (Result layout): Done "Your Jump Start is ready" with "N systems and M games are ready" or Info "No games found yet", the same three rows (label, where), covers-failed sentence; A or B Done -> complete + close | done, host; deliberate change | B also ends the guide (a Result's B closes everywhere; Classic ignored B); the Zelda quote is dropped; no list of systems (counts only) |
| `App::closeScan()` at the end (reload when covers arrived) | `libraryTaskClosed()` when the guide ends by the user | done | |
| GUI closes in the middle (App exits): started stays, complete not set | `FirstRun::close()`: nothing completed, a running scan or cover job is stopped | done, host | Classic never stopped the job (the process ended) |
| `Preferences::setOnboardingStarted(true)` before the pages open | still `App::init` | unchanged | the flow does not touch it |
| Hosting: Classic's `ScanScreen` is a full-screen modal of `App` (`scanActive_`) | `State::firstRun` over the shell: tabs locked, the flow's own hints, holds/toast/video repaint quiet underneath; the screen reopens when the guide ends so the real library replaces the sample data | done, host | |

Not tested on the device (the MiSTer was off): the real quick scan, the cover download, `PatsGuiScreen::startFirstRun` (bridge), the reopen after the guide, `libraryTaskClosed` reload.

