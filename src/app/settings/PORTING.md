# Porting the Settings categories (charter for the category agents)

The new GUI's Settings tab shows the first level of the Classic Settings screen. Each of the seven categories (Library,
Controllers, Interface, Accessibility, Updates, System, Developer) now gets its real functions, one agent per category, in
parallel. This file is the charter. Read `AGENTS.md`, `src/app/README.md` ("Settings tab"), `src/ui-framework/docs/UI-FRAMEWORK.md`,
`src/ui-framework/docs/ACCESSIBILITY.md`, `src/ui-framework/docs/I18N.md` and the component pages in `src/ui-framework/docs/components/` first.

## 1. The rule: 1:1, derived from the source

The owner's words: *not an arbitrary implementation, a 1:1 port; only the underlying UI framework changes.*

- Derive every behavior from `src/legacy/SettingsScreen.*` and the code it calls (`src/legacy/App.cpp`, `src/legacy/ScanScreen.*`,
  `src/legacy/ControllersScreen.*`, `src/legacy/SystemVisibilityScreen.*`, `src/legacy/ArcadeGamesScreen.*`, `src/legacy/UpdateService.*`,
  `src/legacy/CoreMraUpdater.*`, `src/legacy/Wifi.*`, `src/legacy/Preferences.*`, ...), **function by function**. Read the Classic function, port what it
  does. The inventory of everything reachable from Settings, with its source functions, is the last section of
  `src/app/PARITY-settings.md`.
- No invention, no "improvement", no screenshot-based guesses. If Classic does something odd, the port does it too and the odd
  behavior is noted in the parity file; the owner decides later.
- Same texts: labels, help texts, titles, messages, hints, error strings, status words, units, toasts, in the same English words.
  Same conditions (when a row appears or is enabled), same enable/disable rules, same confirmation steps (what asks, with which
  words, which button confirms), same persistence (the **same `Preferences` setters** and the same files: `Preferences`,
  `ScreenScraperSettings`, the Wi-Fi config, the controller `.map` and deadzone files, ...), same edge cases and same timing
  (toast seconds, the 2 s network re-check, the 2 s hold, the 10 s idle exit of the input test, ...).
- Where the framework forces a visual or behavioral difference (a dialog layout, a component that does not exist, an accessibility
  rule), do it the framework's way **and list it** in the "Framework-forced differences" table of `src/app/PARITY-settings.md` (number,
  difference, why). Never list a difference you chose.
- When Classic and the framework docs disagree (for example `Toast.md` says failures are Error modals, Classic toasts them), Classic
  wins and the conflict goes to the "Decisions for the owner" list of the parity file. Do not decide it.
- Never touch Classic (`src/legacy/` outside `src/legacy/bridge/settings/`) except what section 6 allows.

## 2. Rules of engagement

1. **Own files only.** A category agent edits only the files in the table below. Everything else is shared: change it only
   additively (a new function, a new field with a default), re-read the file right before the edit and retry if the edit fails.
2. **Shared files** (do not restructure; additive changes only): `src/app/SettingsTab.*`, `src/app/SettingsMenu.*`, `src/app/Tab.h`, `src/app/PatsGui.*`,
   `src/app/settings/SettingsPage.h`, `src/app/settings/PageStack.cpp`, `src/app/settings/StandardPages.*`, `src/app/settings/SettingsCategory.*`,
   `src/app/settings/SettingsServices.h`, `src/app/settings/SampleSettingsServices.*`, `src/app/settings/SampleControl.h`, `src/app/host/main.cpp`,
   `src/legacy/bridge/settings/SettingsAdapters.*`, `src/legacy/bridge/settings/AdapterEnv.h`, `src/legacy/Screen.h`, `src/legacy/App.cpp`, `src/legacy/bridge/PatsGuiScreen.*`, both Makefiles,
   `src/app/README.md`, `src/app/PARITY-settings.md`, `_vnext/vnext-0.9.0.md`. A missing helper page, a missing field in `MenuState`, a missing
   `SettingsHost` member: add it, tell the coordinator in your report, do not work around it.
3. **No deploy, no ssh/scp, no `tools/shot.sh`, no git (commit, add, stash, checkout), no AI/Claude attribution anywhere.** The owner tests on the
   device. You build and test on the host and with the ARM build.
4. **Layers:** `src/app/` includes only `src/ui-framework/` and `src/app/` headers and the standard library. `src/ui-framework/` stays application-agnostic (no games, systems,
   discs, settings). `src/legacy/bridge/settings/` is the only place that includes both worlds.
5. Nothing blocks in `draw()`, `handle()` or `update()`: no file or network access, no decoding, no scans, no sleeps. Slow work is a state
   machine behind a service that the host advances (AGENTS.md: the SD card is precious, no tight polling, no automatic retry loops).
6. Strings: every user-visible word is a `ui::Text::key`, registered in the category's strings function (section 5.5).
7. Build checks before you report: `make -C src/ui-framework`, `make -C src/ui-framework run` (`ui-demo --contrast` must say 0 pairs fall short), `make -C src/app run`,
   and the ARM build `make -j6 build/mister-pats-gui-0.8.10.so` (exit 0, no new warnings).

## 3. File layout and ownership

```
src/app/SettingsTab.*              hosts the categories and the PageStack (3 panes). Shared. Knows no row, no service.
src/app/SettingsMenu.*             the row model (MenuRow, Cond, MenuState, VisibleRow), the table builders (namespace menu), holds(), visibleRows(). Shared.
src/app/settings/
  SettingsCategory.h/.cpp      the category interface, SettingsEnv, the registry and the macro. Shared.
  SettingsPage.h, PageStack.cpp  SettingsPage, PageStack (toasts go to the shell's Services::notice). Shared.
  StandardPages.h/.cpp         ChoicePage, ConfirmPage, ResultPage, ProgressPage, TextInputPage, InfoPage. Shared.
  StandardPagesDemo.*          the helper pages with sample content, for the host runner. Shared.
  SettingsServices.h           the aggregate of the seven service pointers. Shared.
  <Category>Settings.h/.cpp    one pair per category: table, strings, rows, activate, changed, update, pages.      OWNED by that category
  <Category>Service.h          the service interface of that category.                                            OWNED
  Sample<Category>Service.h    the fake service for the host runner (header-only).                                OWNED
  <category>/                  any further files of that category (pages, flows): *.cpp here are built automatically. OWNED
  SampleSettingsServices.*, SampleControl.h   the aggregate of the samples. Shared.
  PORTING.md                   this file.
src/legacy/bridge/settings/
  AdapterEnv.h, SettingsAdapters.*   what an adapter is built from, and the aggregate. Shared.
  App<Category>Settings.h/.cpp  the host side of that category's service on the Classic objects.                 OWNED
```

`<Category>` is `Library`, `Controllers`, `Interface`, `Accessibility`, `Updates`, `System` or `Developer`; the sample members of the aggregate are
`library`, `controllers`, `interfaceUi`, `accessibility`, `updates`, `system`, `developer`. Both Makefiles build `src/app/settings/*.cpp`,
`src/app/settings/*/*.cpp` and `src/legacy/bridge/settings/*.cpp`: a new file needs no Makefile edit.

Cross-category dependency: **System's "Read full changelog" uses `UpdatesService`** (`snapshot().changelog*`, `startChangelog()`). The Updates
agent keeps those two signatures stable. Likewise Accessibility and Interface both apply theme changes to the framework theme through the
adapter environment (`AdapterEnv::uiTheme`); nobody else touches the theme.

## 4. How the pieces fit

```
SettingsTab ── hosts ──► SettingsCategory (x7, registered by PATSGUI_REGISTER_SETTINGS_CATEGORY)
   │                         rows(state) · valueOf · current · activate(row, PageStack&) · changed · update(dt) · backActivates · hintsHook · openPage
   ├── PageStack ──────► SettingsPage (Content or Modal): update · draw · handle · hints · locked · onClose
   │                         built from StandardPages (ChoicePage ConfirmPage ResultPage ProgressPage TextInputPage InfoPage)
   └── SettingsEnv ────► SettingsServices ─► LibraryService … DeveloperService  (abstract, src/app/)
                                                  ▲ implemented by
                                       src/legacy/bridge/settings/App<Category>Settings  (on Context, Context::settingsHost, AdapterEnv)
```

- **First level.** `SettingsCategory::rows(state)` returns the visible rows (table plus the category's own conditions). The tab draws them.
  A on a Switch toggles it, on a cycling Value steps it; the tab keeps the value in memory unless `current(rowId)` supplies the real one, then
  calls `changed(rowId, value)`. A on anything else calls `activate(rowId, pages)`; return `true` when you handled it (an unhandled row reaches
  the empty first-level seam `SettingsActions`, as before).
- **Pages.** `activate` pushes a `SettingsPage` on the `PageStack`. While a page is open it owns every event; B closes it unless the page uses B;
  the tabs are locked; the page's hints are the whole help line; `update(dt)` of the top page runs every frame.
- **Services.** A service is "what the host can do", an abstract interface in `src/app/settings/<Category>Service.h`. `available() == false` (or a null
  pointer) means the host has no implementation yet: the category behaves as the plain first level. Use `usable(env().services.xService)`.
- **State.** `MenuState` (shared) is what the row conditions read. A category refreshes **its own fields** of it in `update(dt)` from its service
  (see `UpdatesSettings::update`); dynamic value texts go to `state.status[rowId]`.

## 5. Recipes

### 5.1 Add or change a row (`<Category>Settings.cpp`)

Rows are the table at the top of the file, written with the builders in `src/app/SettingsMenu.h` (`menu::action`, `toggle`, `cycle`, `status`, `info`,
`heading`, `shown`, `enabledWhen`, `switchedBy`, `dynamic`). The Classic rows already are there with their labels, help texts, kinds, conditions
and order; **verify each against `buildRows()` and correct it in place** (the checks are in the parity file). For a preference row:

```cpp
int LibrarySettings::current(const std::string &rowId) const {          // the real value, or kNoValue = keep it in memory
    LibraryService *s = service();                                       // usable(env().services.libraryService)
    if (!s) return kNoValue;
    if (rowId == "library.versions") return s->organizeGameVersions() ? 1 : 0;
    return kNoValue;
}
void LibrarySettings::changed(const std::string &rowId, int value) {   // the same setter Classic calls
    if (LibraryService *s = service())
        if (rowId == "library.versions") s->setOrganizeGameVersions(value != 0);
}
```

A cycling row's option list must have the same options in the same order as Classic's cycle code, and the stored value mapping (`"playstation"`,
`300`, ...) belongs in the service or here, never in the tab. A live text ("Not configured", "Download font") is `state.status[rowId]`, set in `update()`,
or `valueOf()` for a computed one. A condition that the shared `Cond` list does not have: apply it yourself in `rows()` after `visibleRows()` (drop or
disable rows), or add a `Cond` additively with its case in `holds()`.

### 5.2 Add a page (open it from a row)

```cpp
bool SystemSettings::activate(const std::string &rowId, PageStack &pages) {
    if (rowId == "system.reboot") return openPage("reboot", pages);      // the same path the host runner uses
    return false;
}
bool SystemSettings::openPage(const std::string &pageId, PageStack &pages) {
    SystemService *s = service();
    if (pageId == "reboot" && s) {
        ConfirmSpec spec;
        spec.title = ui::Text::key("settings.system.reboot.title");
        spec.body = ui::Text::key("settings.system.reboot.body");
        spec.confirmHint = ui::Text::key("settings.system.reboot.confirm");
        spec.onConfirm = [s](PageStack &stack) { std::string error; if (!s->reboot(error)) { /* push a ResultPage */ } };
        pages.push(std::unique_ptr<SettingsPage>(new ConfirmPage(spec)));
        return true;
    }
    return false;
}
```

Also override `pageIds()` so `--list-pages` names the page. This **is** the worked example, already in `SystemSettings.cpp` (try
`make -C src/app run ARGS="--tab settings --category system --page reboot"`). A page that is not one of the standard pages is a class derived from
`SettingsPage` in `src/app/settings/<category>/`; the interface and a skeleton are at the top of `SettingsPage.h`. Page rules: a single `Content` page (a list or a viewer, never a step of a flow) draws
its own frame with `drawSettingsPageFrame()`; `Modal` pages draw their own scrim (use `ui::ModalStack` or `ui::TextEntryDialog`); a page returns
`true` from `handle()` for what it used; Back that it does not use closes it; `onClose()` stops what only the page started.

### 5.3 The standard pages

| Page | Use for | Spec fields (all `ui::Text` keys and callbacks) |
| --- | --- | --- |
| `ChoicePage` | pick one of a list: time zone region, then city | `title`, `items`, `current` (marked and the cursor start), `closeOnChoose`, `onChosen(index, stack)`, `onCancel`, `chooseHint`, `cancelHint` |
| `ConfirmPage` | a yes/no with a body: reboot, install | `title`, `body`, `confirmHint`, `cancelHint`, `onConfirm`, `onCancel`, `closeOnConfirm` |
| `ResultPage` | what happened: info, done, warning, error ("Try again" when `onRetry` is set) | `kind`, `title`, `status`, `body`, `whatToDo`, `details`, `onRetry`, `onClose` |
| `ProgressPage` | work with a bar: download, install, scan, a flow's "working" step | `title`, `status`, `detail`, `fraction`, `cancellable`, `onCancel`; `onUpdate(page, stack)` polls the service |
| `TextInputPage` | the on-screen keyboard: ScreenScraper login, Wi-Fi name, password, country | `entry` (`ui::TextEntrySpec`: `title`, `placeholder`, `value`, `masked`, `maxLength`), `validate`, `onSubmit`, `onCancel` |
| `InfoPage` | read-only content: `facts(spec, provider, refreshSeconds)` for label/value rows, `viewer(spec)` for scrolling text | `title`, `subtitle`, `actionHint`, `onAction`, `onClose`; viewer: `setText`, `setLoading`, `setError`, `setEmpty`, `onRetry` |

Each is documented with a code example at its declaration in `StandardPages.h`, and every one can be looked at with
`make -C src/app run ARGS="--tab settings --page standard/<id>"` (`--list-pages` prints the ids). If a page you need is missing or too limited, add the
smallest additive option to `StandardPages` and say so in your report; do not copy it.

### 5.4 Flows (a wizard)

A flow is a sequence of **two or more steps** (Wi-Fi setup, build the game list, prepare box art, core updates, the mapping wizard). It is a
`WizardFlowPage` (`WizardFlowPage.h`): every step is a `ui::WizardPage` in the wireframes' dialog frame (`src/ui-framework/docs/components/Wizard.md`).
Port **the Classic state machine** unchanged: its steps, its transitions (`handleWifiFlow`, `handleCoreUpdater`, `ScanScreen::handle`), its Back
rules (`cancelWifiFlow(restore)` is `leave(true)` of the Busy page), its messages. Branching is fine (the page that knows the next step calls
`w.go(id)`; no "n / m" counter on a branching flow). What the wizard takes over is the frame, Back by page kind, the hint bar, the Result layout
and the reason line; do not draw your own frame, text page, progress page or message pill (the Library's old text and progress pages are gone).

Rules:

1. **One size for the whole flow.** `WizardFrame::Medium` (1120 x 760) or `Large` (1812 x 860), chosen once from the flow's largest page: a list of
   more than about 8 rows, filter tabs or a review that must show many lines is Large; everything else Medium. Every page, Busy and Result included,
   is drawn in that frame, content at the top; never a different size per page.
2. **Page kinds decide Back.** Step (B = previous page; first page = close), Busy (B = cancel, `leave(true)` restores; not cancellable = no hint),
   Result (A primary, B close), Capture (raw input; the page names its own way out). Do not write B handling by hand.
3. **Body parts are framework parts** (`WizardParts.h`): `drawWizardParagraphs` / `WizardScrollText` for text (reading column), `ChooseList` for
   single and multiple choice (status word per row), `drawWizardStatus` for a one-line status, `BusyPage` for work (bar, one status phrase, one detail
   line; no counters per file, no ETA, no names), `ResultPage` + `WizardResult` for every outcome (status icon and word, plain headline, what changed,
   next step, the service's text only as `details`), `EntryPage` for the keyboard. A text that is not a framework component is a bug in the page.
4. **Outcomes are Result pages**, never toasts (a toast may add a short "saved" notice after a Done result). Failure pages say what happened in plain
   words and what was or was not changed; the raw service string goes into `WizardResult::details`; give Try again a `retry` page when one exists.
5. **Hint words** come from the closed set (`hint.*`, `Hints.md`): Continue, Start, Select, Choose, Back, Close, Cancel, Skip, Later, Done, Try again.
   The wizard adds B; a page lists its own movement, A and Start hints. A step that cannot continue says why in `canContinue(reason)`.
6. **Keep a flow's data in one `std::shared_ptr<Flow>`** captured by the page lambdas (see `ScanFlow.cpp`'s `Visit`); pages re-read it in `enter()`.
   Poll services in `update()` of the page, never block; undo side effects in `leave(cancelled)`; the flow ends with `w.close()` and the page pops itself.
7. A single question or a notice is **not** a flow: use `ConfirmPage`, `ResultPage` (standard), `TextInputPage`. Lists, viewers and diagrams stay pages.

```cpp
enum { kIntro, kWork, kDone };
struct Flow { bool ranOnce = false; };

void openMyFlow(PageStack &pages, MyService *svc) {
    std::shared_ptr<Flow> flow(new Flow);
    std::unique_ptr<WizardFlowPage> page(new WizardFlowPage(ui::WizardFrame::Medium));
    WizardFlowPage *self = page.get();          // callbacks reach the stack and the toast through it

    std::unique_ptr<ui::FnPage> intro(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("settings.my.title")));
    intro->onDraw = [](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        std::vector<ui::WizardParagraph> paras(1);
        paras[0].text = ui::Text::key("settings.my.intro");
        ui::drawWizardParagraphs(p, t, body, paras);
    };
    intro->onHints = [] { return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true)}; };
    intro->onEvent = [svc](ui::WizardEvent e, ui::Wizard &w) {
        if (e == ui::WizardEvent::Confirm && svc->start()) w.go(kWork);
    };
    page->add(kIntro, std::move(intro));

    std::unique_ptr<ui::BusyPage> work(new ui::BusyPage(ui::Text::key("settings.my.title")));
    work->status = ui::Text::key("settings.my.working");
    work->onUpdate = [svc, flow, self](float, ui::Wizard &w) {          // polls; never blocks
        if (svc->finished()) { flow->ranOnce = true; w.go(kDone); }
        return true;
    };
    work->onLeave = [svc](bool cancelled) { if (cancelled) svc->cancel(); };
    page->add(kWork, std::move(work));

    std::unique_ptr<ui::ResultPage> done(new ui::ResultPage(ui::Text::key("settings.my.title")));
    ui::ResultPage *rp = done.get();
    done->onEnter = [svc, rp](ui::Wizard &) { /* build a ui::WizardResult from svc state, then rp->set(result) */ };
    page->add(kDone, std::move(done));
    WizardFlowPage::launch(pages, std::move(page), kIntro);              // opens kIntro and pushes the flow
}
```

The hosting API: `WizardFlowPage::add(id, page)`, `launch(stack, flow, first)`, `stack()` (push, replace, pop other `SettingsPage`s from a callback), `toast(text)`,
`handOff(nextPage)` (the flow ends and another page takes its place), `wantsRawInput()` (a Capture page that reads the pad), `wizard()`. `onClose()` (the GUI
closes, the tab is left) calls `leave(true)` on the current page. Look at a flow on the host with `make -C src/app run ARGS="--tab settings --page standard/wizard --keys a,a"`
(`--keys` drives it, `--after N` lets time pass).

### 5.5 Strings

`PATSGUI_REGISTER_SETTINGS_CATEGORY(Class, stringsFn)` calls your `strings(ui::Catalog &)` at startup. In it: `registerTable(c, table())` (the category
label `settings.cat.<id>`, every row `settings.<rowId>` and `settings.<rowId>.help`, and every option of the row lists) and your own page texts. Key
rules: page texts are `settings.<categoryId>.<page>.<what>` (`settings.system.reboot.title`); option labels `settings.opt.*`; hint words reuse the shared
`hint.*` keys (`hint.close`, `hint.cancel`, `hint.tryAgain`, ...) before adding one; placeholders are `{1}`, `{2}` filled with `ui::format(key, {...})`;
counted texts use `ui::plural`; data (a Wi-Fi name, a path) is `ui::Text::data`. A default `ui::Text()` is not a text: never pass one to a component that
always draws (the catalog would show its missing-key marker); use `ui::Text::data(std::string())`.

### 5.6 Add or extend a service (the three files)

1. `src/app/settings/<Category>Service.h`: add the method, named after the Classic call it wraps (the comment names it). Types are plain structs; no `src/legacy/`
   type leaks in. Long work is a state machine: a `start...()`, a snapshot struct, a `cancel...()`.
2. `src/app/settings/Sample<Category>Service.h`: implement it with settable state (`set("category.key", value)` and a line in `help()`), so every state can be
   shown on the host without a device (section 7).
3. `src/legacy/bridge/settings/App<Category>Settings.h/.cpp`: implement it on the Classic objects, calling the same code Classic calls. The list of what each adapter needs
   from `Context`, `Context::settingsHost` and `AdapterEnv` is in section 6. Set `available()` to true only when the whole interface works.

### 5.7 Poll a service

`SettingsCategory::update(dt)` runs every frame while the Settings tab is shown (also while a page is open). Call `service->update(dt)` (the host
steps its state machines there: `LibraryScan::step()`, `Wifi::update()`, `SystemInfo::update()`), copy what the conditions need into `env().state`
and return `true` when something changed. See `UpdatesSettings::update`. Pages poll in their own `update(dt, stack)` or `ProgressPage::onUpdate`.
Mind the SD card: poll memory, not files, and do not re-read a file every frame (Classic reads once and caches: `refreshScreenScraperLogin()`).

## 6. Plumbing the host provides, and what each adapter needs

Already in place (this foundation): `Services::settings`, `SettingsAdapters` (created by `PatsGuiScreen::open()`), `AdapterEnv`
(`context`, `uiTheme()`, `invalidate()`), `Context::settingsHost` (filled by `App`, `src/legacy/App.cpp` next to `context_->input`), and `rawInputWanted`
honored by `App` (no pad-to-Action translation while it is set and the new GUI is active).

| Adapter | From `Context` | From `Context::settingsHost` | Owns / other |
| --- | --- | --- | --- |
| Library | `preferences`, `preview.service()` (ScreenScraperService), `theGamesDB`, `library`, `discArtwork`, `discDrive` | `scan`, `scraper`, `reloadLibrary` | a `CoreMraUpdater`; `ScreenScraperSettings` (file); `internetRouteAvailable()` (copy it: it is private to `SettingsScreen.cpp`) |
| Controllers | `preferences` (faceButtonStyle), `setFaceButtonStyle()` (ButtonLabels.h) | `input` (writable), `rawInputWanted` | a `Bluetooth`; `ControllerMap` / `ControllerId` files |
| Interface | `preferences`, `hiddenSystems`, `library`, `theme`, `onThemeChanged` | `toggleGamesTab`, `toggleArcadeTab`, `cycleDefaultView` | `AdapterEnv::uiTheme` for color theme and dark mode |
| Accessibility | `preferences`, `theme`, `onThemeChanged` | `spaceGroteskStatus`, `activateSpaceGrotesk` | `AdapterEnv::uiTheme`; the font reload on the framework side (see below) |
| Updates | `preferences` (checkForUpdates) | `updates` (UpdateService), `startUpdateCheck` | `SystemInfo::formatBytes` for `UpdateInfo::bytesDoneText`; reboot is `::sync(); ::reboot(RB_AUTOBOOT)` as `App::restartMisterAfterUpdate` |
| System | `preferences` (clock, time zone, screensaver), `discDrive`, `discCores`, `discInstaller`, `library`, `favorites`, `images`, `theme` | `backup`, `updates` (commitRunning), `startAssetDownload`, `assetDownloadStatus`, `startMisterCore`, `framebufferInfo` (the Resolution fact) | a `Wifi`, a `SystemInfo` |
| Developer | none | `openWireframes` | |

Known gaps (additive work, tell the coordinator):

- **Framework theme bridge.** Classic's theme setters (`context.theme.setFontSizeScale`, `setHighContrast`, `setDarkMode`, ...) change the Classic theme; the new GUI
  has its own `ui::Theme` (`PatsGuiScreen::theme_`, text scale through `setTextScale/setHintScale`, `setHighContrast`, `setDarkMode`, `setColorTheme`). The adapters
  get it from `AdapterEnv::uiTheme()` (null before the first frame); the Interface and Accessibility agents apply both themes, then call `invalidate()`. Today
  `PatsGuiScreen` reads only `tabDisplay` from the preferences; reading the others at start is theirs to add in `PatsGuiScreen::ensureResources` (additive).
- **Space Grotesk in the framework fonts.** `PatsGuiScreen::ensureResources` loads the Classic font paths once; enabling the font later needs a reload there.
- **Raw input** for the Controllers wizard and input test: `SettingsHost::input` is the writable `Input`; the page sets `rawInputWanted` while it needs it and
  clears it on close (`SettingsPage::onClose`).
- **Classic `notify()` / `showError()`** have no counterpart in the new GUI: a toast is `PageStack::toast(text, seconds)`, an error is a `ResultPage` of kind
  `Error`. Keep Classic's words and seconds (section 1).
- **A running App-level task** (`App::update` reports the backup result, selects the Settings tab when the restart prompt appears) still runs in Classic's
  `App`; the new GUI polls the same objects and shows its own result. Check `App::update` for each task you port so nothing is reported twice.

## 7. Testing

**On the host (no device):**

```sh
make -C src/app run ARGS="--tab settings"                                   # the first level
make -C src/app run ARGS="--tab settings --list-pages"                      # category/page ids, including standard/*
make -C src/app run ARGS="--tab settings --category system --page reboot"   # one page: build/out/app-settings-system-reboot.png
make -C src/app run ARGS="--tab settings --category updates --set updates.phase=downloading"
make -C src/app run ARGS="--tab settings --category system --keys down,down,a --after 2"    # drive it, let time pass
make -C src/app run ARGS="--tab settings --list-samples"                    # every sample state key
make -C src/app run ARGS="--height 720"   # also try 720p, and a large text scale and high contrast where the page has layout
```

`--set category.key=value` sets a state of the sample services (`SampleSettingsServices`): scan, artwork, core updater, TheGamesDB, account, update phase,
pinned, changelog, disc, disc package, disc installer, Wi-Fi (present, test result, country prompt, save failure), backup, controllers, Bluetooth, wizard,
font. `--keys` sends events (`up down left right a b x y start xhold bhold lprev lnext select tabprev tabnext`), `--seconds` lets time pass before the
keys, `--after` after them (sample tasks advance by themselves unless `<category>.auto=0`). Check every state of every page: empty, working, success,
error, offline, unavailable, Largest text, high contrast, 720p. Add the states your pages need to your sample service.

Test on the host: the state machine (each transition of the Classic flow, each Back), the texts, the conditions of the rows (`--set` a state, look
at the rows), the hints of every page, that nothing draws outside its box at Largest. Fake services cover logic; they cannot show device behavior.

**What only the owner can test on the device:** real files and threads (the adapter against `Preferences`, `UpdateService`, `Wifi`, `BackupTask`,
`DiscCoreInstaller`), the controller wizard with real pads, Bluetooth, the Wi-Fi test and rollback, an actual update install and restart, reboot,
timing and responsiveness on the ARM CPU, the SD card I/O, the theme change on the real framebuffer, and how it looks on the TV. Say in your report
exactly which of these your category needs, as a checklist for the owner. The device pin and the deploy are the coordinator's.

## 8. Parity checklist (per category, in `src/app/PARITY-settings.md`)

For every Classic function that the category touches, one row:

| Classic function (source) | New location | Status | Verification |
| --- | --- | --- | --- |
| `SettingsScreen::handleTimeZoneChooser()` Confirm on a region (`SettingsScreen.cpp` 2162) | `SystemSettings::openTimeZone()`, `ChoicePage` (closeOnChoose = false) | done / partial / todo / diff Fn | host: `--page system/timezone --keys a`; device: set a zone, reboot, clock |

Status words: **todo**, **partial** (say what is missing), **done**, **diff F<n>** (a framework-forced difference, numbered in the table there). Verification
names the exact command or the device step. A row is not **done** until its text, condition, confirmation, persistence and timing were compared with the
Classic code, line by line. Keep the existing first-level rows of the file as they are and add your category's function table under your category heading;
extend the "Framework-forced differences" and "Decisions for the owner" tables instead of deciding.

## 9. Accessibility, theme, i18n (the framework's rules apply to every page)

`src/ui-framework/docs/ACCESSIBILITY.md`, `THEMING.md`, `I18N.md` are binding. In short: colors and sizes only from `ui::Theme` (`t.px()`, `t.palette`, `TextRole`, `TextTier`),
never light/dark or high-contrast checks; every state has a word or icon besides the color (success is the accent, warning orange, error red); text elides or
wraps and nothing clips at Largest (130 %) text or at 720p; Back always leaves, focus is never trapped, a flow always has a cancel; hints name every action and
follow the button layout (`ui::HintKey`, never literal button names; Classic's `formatButtonLabels()` strings become hint keys); destructive or hard-to-reverse
actions ask first and say what changes; long work is cancellable where Classic lets it be; no new colors; no component of your own where the framework has
one (list it as a gap instead). The contrast tests (`ui-demo --contrast`) must stay at 0 failing pairs.

## 10. Reporting

Your report lists: files added and changed (shared files separately), the Classic functions ported (the parity table), framework-forced differences and
decisions (as added to the parity file), what the owner must test on the device, build results (host, ARM), and anything that blocked you or needs another
category. Do not commit.
