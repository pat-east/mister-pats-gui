# UI Inventory

Survey of every view in the GUI, grouped into candidate components from coarse to fine. It is the
input for [UI-FRAMEWORK.md](../src/ui-framework/docs/UI-FRAMEWORK.md) (step 1 of the delivery plan) and records what the
code does today, not what it should do. Line numbers refer to the code on 2026-11-22 and will drift;
use them to find the occurrence, not as a permanent reference.

Scope: `App.cpp`, `TopBar`, `ErrorModal`, `CrashScreen`, `Splash`, `LoadingIndicator`, `GameSearch`,
`OnScreenKeyboard`, `HomeScreen`, `SystemsScreen`, `SystemVisibilityScreen`, `GamesScreen`,
`ArcadeGamesScreen`, `Tile`, `GridView`, `PreviewDisplay`, `SettingsScreen`, `ArcadeSettingsScreen`,
`ControllersScreen`, `ScanScreen`, `DiscScreen`.

Abbreviations: HS Home, GS Games, SS Systems (browse) or Scan (flows), SVS SystemVisibility,
AG ArcadeGames, ST Settings, CS Controllers, DS Disc, APP App.cpp.

## 1. Result in one page

- **Three kinds of "flow"** are hand-coded: modal flags (ST has 11), page enums (Scan, Core updater,
  Wi-Fi, Controllers) and derived state (Disc). None share a state model.
- **Dialog frame exists 20+ times** with different padding, scrim alpha, size, shadow and stroke.
- **Progress bar exists 9 times** (heights 6, 10, 12, 14, 20, 28; tracks `surface`, `surfaceHi`,
  `surface` with alpha).
- **Row with focus exists in 8 variants** (radius 4/6/8, marker 3/4 px, outline present or absent,
  fill alphas 60 to 220).
- **Focus frame** has at least 8 looks: radius 5/6/8/10, stroke 1/2/3 px, colors `focus`, `accent`,
  `lerp(accent, focus, .65)`, white.
- **Hints are written in four places:** `Screen::hints()`, `APP::renderBottomBar` overrides,
  footers drawn inside dialogs, and body text. They disagree (see section 6).
- **Missing components** the DESIGN document already requires: scroll bar on every scrolling list,
  separate `error` color, status never by color alone.
- **Bugs found while surveying** are listed in section 7.

## 2. Components, coarse to fine

Each entry gives: what it is, where it occurs, the variants that are real differences, the
inconsistencies to resolve, and the proposed reference implementation (the one whose values become
the standard unless the survey decision says otherwise).

### Level 1: application structure

**C1 AppFrame.** Top bar (96), content (margins 72 / 32), bottom bar (72). APP 1707-1712, 1835-1866.
Good as is. Reference. Gaps: page background has a hard-coded light-mode color (APP 1141).

**C2 Screen + routing.** `Screen` interface (Screen.h) plus seven "takeover" flags in APP
(`scanActive_`, `visibilityActive_`, `controllersActive_`, `arcadeGamesActive_`,
`arcadeSettingsActive_`, `detailActive_`, `gameActionsActive_`) with a branch each in `dispatch()`,
`activeScreen()` and `tabNavigationAvailable()`; Quit handled five times (APP 1059-1090). The update
prompt forwards input to Settings outside this mechanism (APP 935-964).
→ **ScreenStack** (replaces the flags) and **ModalStack** (section C5).

**C3 Takeover screens.** Splash, CrashScreen (own card, own hard-coded font sizes), Boxart and Dimmed
screensavers. Rare and justified as exceptions, but they should draw through the same Panel and
tokens.

**C3a Splash (startup screen)** — `Splash.cpp`, called once from `App.cpp:153-160`
(`options_.splashMs`, default 1000, `--no-splash`, `--splash-ms N`).
- *Visual:* `background` clear; logo 128×128 (embedded JPEG written to `/tmp`, decoded, image radius
  16) centered, 30 above the middle; progress track 280 × 10, radius 5, 40 below the logo, `surface`
  track, `accent` fill, minimum fill 10; no text, no hints, no version, no error state.
- *Behavior:* it is not a `Screen`. `Splash::show()` is a blocking loop with its own frame pacing
  (`usleep(16000)`), outside `App`'s render, input and dirty-region path. Progress is two phases:
  first 25 % is the drive-settle wait (while `warmUpNextDrive` reads one `/media/usbN` per 250 ms to
  avoid an early un-paced read), then 75 % shared by system-icon loading (one decode per frame) and
  the `startupTask` (box-art index load or scan), which runs after the wait. It stays up until the
  last icon is cached and one 100 % frame has been shown.
- *Coupling to another screen:* it recreates the Systems grid geometry (`GridView.configure(content
  .inset(18), theme, 250, 4, 3, 0)`) so the preloaded icons match `SystemsScreen`'s cache entries.
  These constants exist twice (`SystemsScreen.cpp:147-149` and here). A change in one silently makes
  the preloaded icons miss the cache in the other.
- *Inconsistencies/risks:* its progress bar is the seventh pill (see ProgressBar below); no failure
  path (a startup task that hangs leaves a static splash with a stuck bar); no way to show a message
  such as "Building the game list…" even though the box-art scan can take long; it is the only
  full-screen view with no hint bar and no text, so it cannot use `Label`/`Hints` as designed.
- → keep as a **Takeover** built from `Surface`, `Image`/logo, `ProgressBar` and `Label`, with the
  startup phases reported as data (`{label, fraction}`) so the same `ProgressPanel` text lines (title,
  status) are available; keep the pacing loop but let it own a `ScreenFrame`-free canvas. The Systems
  grid geometry becomes a shared constant/function used by both.
- *Status line rule (design premise: abstraction, not stimulus overload):* a closed set of at most
  three plain phrases, shown only when a phase is long, one muted line, no numbers, no names, no
  flicker. Details in UI-FRAMEWORK.md, "Takeovers".

### Level 2: screen frames and layouts

**C4 PageHeader (+ HeaderInfo).** Bold heading at area origin, height 58, optional right-aligned muted
info text (regular body, `textMuted` alpha 160, y+10). HS 632, GS 436-445, SS 116-161, SVS 59-71,
AG 207-230, CS 212/410/622/1053 (header 58, InputTest 76), DS 218 (offset 62), ST 2681. Copied 10x.
→ one `PageHeader(title, info)`; reference GS.

**C4b SearchHeader.** Header + search field (54, drawn 50, 20 below) + body, keyboard replaces the
body. HS 632-662 and GS 876-902 are copies. Reference GS; HS embeds a GS for results.

**C4c SplitLayout.** Three columns (ST: categories, options, help; gap 26, panels `surface` alpha 70)
and list + detail (GS list view: right half, `backgroundLo` alpha 150, pad 24; CS: list max 560 +
detail pad 26). → `Columns(n)` / `ListDetail`. Reference ST for columns, GS for list/detail.

**C4d EmptyState.** Centered muted line: HS rows (small, alpha 110, left-aligned: violates "never
small"), SS (bold heading, whole area), SVS/AG/GS (regular heading), DS (bold title + body), CS
(regular heading), ST updater ("Nothing to show here" + hint inside the text). No wrapping in most
(GS has two private wrappers). → `EmptyState(title, why, nextAction)`, wraps and elides; reference
the ST updater texts for content, GS for wrapping.

**C4e LoadingPanel / ProgressPanel.** `LoadingIndicator::draw` (title, bar 14, status, counts, extra,
spinner) used by SS (panel 700×200 centered), AG (inset 20), SS Scan. Scan JumpStart draws the same
information a second way (heading title, bar 12 on `surfaceHi`). → one `ProgressPanel`; reference
`LoadingIndicator`.

### Level 3: overlays and flows

**C5 Modal (dialog frame).** Scrim + centered `surfaceHi` rounded panel + bold heading title + content.
Occurrences and their differences:

| Dialog | Scrim α | Size | Pad | Title gap | Shadow | Stroke |
| --- | ---: | --- | ---: | ---: | --- | --- |
| ErrorModal | 150 | 620 × content | 36 | 20 | spread 24, α140 | `warning` α160 |
| Game options (APP) | 175 | 1776 × 800 | 28 | 48 | spread 24, α160 | none |
| Screenshot viewer (APP, DS) | 230 | full | – | – | – | – |
| Core updater (ST) | 215 | 1120×760 / 1812×860 | 28 | 16 | none | none |
| Changelog (ST) | 215 | 1100 × 820 | 30 | 4 + 10 | 24 / α150 | 2 px `focus` |
| Wi-Fi (ST) | 215 | 1000 × 760 | 28/24 | 12 | none | none |
| Account entry (ST) | 215 | 1000 × 760 | 28/24 | 12 | none | none |
| Time zone (ST) | 205 | 900 × (h−56) | 28/24 | 54 | none | none |
| Disc drive (ST) | 210 | 980 × 650 | 30 | 58 | none | none |
| Disc install confirm (ST) | 220 | 850 × 340 | 30 | 18 | none | none |
| Update confirm (ST) | 205 | 850 × 600 | 28 | 12 | none | none |
| Update success (ST) | 205 | 900 × 330 | 30 | 12 | 24 / α140 | 2 px `accent` α160 |
| Reboot confirm (ST) | 205 | 560 × 190 fixed | 28 | 12 | none | none |
| System information (ST) | 205 | 960 × 680 | 28 | 18 | none | none |
| Scan page (SS) | – | 1000×650 / 1120×760 | 34/36/44 | 52–96 | 10 / 26 / α180 | 2 px `accent` α90/220 |
| Bluetooth (CS) | – | w·⅔ × h·½ | 24 | – | none | none; `surfaceHi`, not drawn over list |
| Game search keyboard panel | – | keyboard | 12 | – | none | `surface` α235 |

Real variants: **Info** (read, close), **Confirm** (A act, B cancel), **Error**, **Progress** (busy),
**Result**, **Choose** (list), **Input** (text entry + keyboard). Resolve: one scrim token, one pad,
one title gap, one stroke rule (none by default; `warning`/`error` only for Error), shadow yes/no
decided once. Sizes become named presets (see C6).
→ `Modal(variant)`; reference core updater for structure (it already matches DESIGN), ErrorModal for
the Error variant.

**C5b ModalStack.** Replaces ST's 11 bools, `coreUpdaterModalActive_`, `wantsBack()` (a 12-term OR),
a 25-branch `hints()`, and the render/handle order that must be kept in sync by hand (they do not
match today). Also covers APP's `errorActive`, `gameActionsActive_`, screenshot overlay, and the
update prompt. Modal state sometimes lives in a service snapshot (ST update confirm/success).

**C6 Dialog size presets.** Observed: short 1120×760, list 1812×860, error 620, plus 14 ad-hoc sizes
(see C5). Proposal: `short`, `list`, `small` (confirm), `error`, `full`. To be decided in the
specification step.

**C7 Wizard / WizardPage.** Four flows with the same shape and four encodings:

| Flow | State | Closed is | Pages |
| --- | --- | --- | --- |
| Core updater (ST) | `enum` + `modalActive_` bool + `installResult_` flag | bool | Intro, Checking, List, Review, Installing, Result |
| Wi-Fi (ST) | `enum WifiStep` | `None` | Scanning, Networks, SsidEntry, PasswordEntry, CountryEntry, TestPrompt, Testing, TestReview, TestFailed |
| Account entry (ST) | `enum AccountEntry` | `None` | User, Password |
| Time zone (ST) | two bools | bool | Regions, Cities |
| Scan (SS) | `enum Page` + 5 flags | `onClose_` | Welcome, JumpStart, Intro, Working, ArtworkOffer, ArtworkWorking, Overview, Done |
| Controller mapping (CS) | `Mode::Wizard` + `wizardIndex_` + opening bool | mode | opening + data table `wizardSteps_` |
| Bluetooth (CS) | `BtState` | mode | Idle, Scanning, Found, … → chains into the Wizard |

Common needs: page id (optional, `none` = closed), per-page {title, render, handle, hints,
`canContinue` with reason}, auto-advance from a background task (poll in `update`), a result page
with variants and "Try again", `busy` pages where Back = cancel, per-page Back target, step counter
(none today except CS "n / m"), flow data owned by the flow, cursor/scroll reset on page entry (shared
members reset by hand today), raw-input capture (CS `wantsRawInput`). The route is hard-coded
`page_ = …` assignments (SS has three routes through one enum).
→ Reference for structure: core updater; for data-driven steps: CS `wizardSteps_`.

### Level 4: containers

**C8 TileGrid.** `GridView` (SS, GS) with `configure/cellFrame/clampScroll/move`. Home implements a
third copy (HS 340-368, 457-462, 585-601) and uses pixel paging (`pageScroll_`) instead of rows.
Growth margin policy differs: SS fixed 18, GS focus margin + 3, HS focus margin (reserved though cover
tiles do not grow). → `Grid` on top of `GridView`; reference GridView.

**C8b TileRow.** Horizontal row with peek tile, clip, scroll (HS "Recently played", HS 422-503).
Scroll-follow code is duplicated inside HS (459 and 513).

**C9 List (vertical).** Four implementations of the same scroll clamp: SVS 82-125, GS 607-680,
AG 282-334, ST options (cursor-follows-bottom), ST core updater (line-based with headings), Wi-Fi,
time zone, CS (no scroll: rows beyond the area are unreachable), Scan progress. Row heights 38, 44,
46, 52, 62, 64, 68. **No scroll bar** except Review and Scan progress; DESIGN requires range + bar.
Three jump-to-letter variants (HS, GS, AG; SVS has none although it is alphabetical).
→ `List` with `ScrollView` + `Scrollbar` + range text.

**C9b Table.** AG only: weighted columns, header row, flag cells with check/cross glyphs. Keep as its
own container; derive row look from `ListItem`.

**C10 Result list.** Core updater live list (surface box r9, newest line bright), Review box, Scan
progress (system/status rows), Disc status table (`line(label, value, color)`). Status labels for the
same enum differ ("Not installed" / "Missing", "Update available" / "Update available", "Cannot be
checked" / "Unavailable", "Modified" / "Changed"). → `Result` + `ResultItem`; one status vocabulary.

**C10b KeyValueList.** ST info modal and facts column (label column ≤ 38%, muted label, gap 16), Disc
drive status (fixed 330 px value column), DS metadata grid (uppercase labels), CS controller detail.
Three section-heading encodings (`Row.heading`, `Fact{label,""}`, `ChangelogLine.heading`).

**C10c DetailPanel.** GS list detail and Game options: art slot, caption block (duplicated),
status line under art, video slot, screenshot strip (GS, APP, DS: thumbs radius 5, stroke 2).

### Level 5: items

**C11 GridItem = Tile.** Variants: cover (art, border-only focus), icon (SS), typographic placeholder,
compact placeholder. Focus color is `lerp(accent, focus, .65)`, not `focus`; high contrast not handled;
caption offsets (4 + 24, +2 + 20) do not add up to the reserved 26 + 20; sublabel not elided; SS
reserves the caption inside the 4:3 frame while GS/HS reserve it outside. Non-cover growth/shadow
branch is currently unused.

**C12 ListItem.** One row look in eight copies:

| Where | Radius | Marker | Outline | Fill (focused / other) | Notes |
| --- | ---: | ---: | --- | --- | --- |
| ST category | 8 | 4 | yes + white HC | `surfaceHi` / `surface` α190·110 | |
| ST option | 8 | 4 | yes + white HC | `surfaceHi`·`surface` α190 / none | value, switch, heading |
| ST updater two-line | 8 | none | yes | `surfaceHi` / `surface` | checkbox, status, size |
| ArcadeSettings | 8 | 4 | yes | `surfaceHi` / `surface` α150 | copy of ST |
| GS list | 8 | 4 | yes | lerp | one/two-line, star |
| SVS | 8 | 4 | **no** | lerp | state pill, eases |
| AG | **6** | **3** | yes | lerp + zebra | eases |
| CS controller | 8 | 4 | yes | `surfaceHi` / `surface` α150 | no scroll, no elide |
| CS Bluetooth action | 8 | none | **no** | `surface` α110 | |
| Wi-Fi | 8 | 4 | no | `surface` / `surface` α110 | |
| Time zone | **7** | none | none | accent α48 / none | |
| Scan progress | **6** | none | yes | `surfaceHi` | |
| Game options ROM | 8 | none | no | `surfaceHi` | text "Selected" |

Row *types* (real differences): Nav, Value (cycle), Switch (round checkbox), Choice (checkbox + two
lines + status + size), Action, Info (inert but selectable), Heading, Status-only line. ST derives the
type from the value string (`"A"`, `"On"/"Off"`); that must become an explicit type. Trailing slots
seen: arrow/"A", value, checkbox+On/Off, star, state pill, status+size, flag cells.
→ reference ST option row (most complete, has HC border), taking the checkbox from ST.

**C13 ResultItem** (Done, Pending, Working, Warning, Error, Skipped). Today: colored word without icon
(updater, Disc drive, Scan), `warning` for errors, success text sometimes `textMuted` instead of the
accent (updater result).

**C14 Inputs.** TextField (ST account/Wi-Fi: h 54, `surface`, accent stroke, `_` cursor; GameSearch:
`surfaceHi`, `accent`/white stroke, hint text inside the field), OnScreenKeyboard (keys r6, alpha fills,
**no outline**), Picker (`< value >` with ASCII arrows, SS artwork offer), ActionButton (DS "Start
game": solid accent, inset border; contrast logic local to DS; label not elided), pad parts (CS: lit/dim,
a third button style).

### Level 6: primitives and tokens

| Primitive | Occurrences / variants | Resolve |
| --- | --- | --- |
| **FocusFrame** | Tile r10·3 px·lerp color; rows r8·2 px `focus`; AG r6; Scan r6; thumbs r5; search r10 `accent`; hint frame r6; white HC in ST/search only | one function; HC and White handled inside |
| **FocusMarker** | 4 px accent bar r2 (ST, GS, SVS, CS), 3 px r1 (AG), 4 px `focus` squared (DS) | one marker, one color |
| **Surface / Scrim / Shadow** | `surface`, `surfaceHi`, alphas 60-235; scrim α 150/175/205/210/215/220/230 + black α190 | named surface roles and 3 scrim levels |
| **ProgressBar** | LI 14 r7; Splash 10 r5; hold bars 6 r3 (`favorite`/`accent`); ST updater 12 r6 ×2; Scan 12 r6 on `surfaceHi`; CS 14, 10 (`warning`), 20, 28; fill min = height except Deadzone | one pill, `ProgressBar(fraction, role)`; heights from tokens |
| **Scrollbar + range** | Review (4 px, `focus`, 8 from edge, "↑ N more"), Scan (4 px, `accent` thumb, 4 from edge, "a–b of n"), Changelog/notes (↑/↓ glyphs); none elsewhere | one bar, `focus` thumb, 8 from edge, one range text |
| **Checkbox** | local `drawRoundCheckbox` (colors `0x17315F`, `0xD6D8DE` literals); SVS uses a pill instead | shared header, theme tokens |
| **Badge / StatePill** | SVS "Shown/Hidden", Home/Games star, ST status words | `Badge(label, tone, icon)` |
| **FavoriteMark** | Tile: 30 px circle + "*"; GS list: bare "*"; CS: "*" for shared mapping | one icon |
| **Label** | bold/regular, elide, right-align by `measure` (5 hand-written places), centered | `Label(role, align, elide)` |
| **TextBlock** | line step +3, +4, +6, +8, +9, +10; wrappers: `ErrorModal::wrap`, `GS wrapText` (+ inline), `DS wrapText`, `wrapHelp`, changelog lambda ×2, SS `drawParagraph` (hand-wrapped strings) | one wrapper, +9 |
| **RichText** | changelog and update notes: two markdown-subset parsers | one |
| **TabRow** | TopBar underline (fades 45 ms) and ST updater tab row; both 3 px r2 | one, instant vs fade decided |
| **Hints** | see section 6 | one |
| **Toast** | `Context::notify`, 15+ strings in DS, HS/GS/AG duplicates "Games starting with X" | keep, add tone |
| **Icons / glyphs** | AG check/cross (custom AA lines), `{DPAD}` cross, "*", ASCII arrows | `Icon` set |
| **Dot / Spinner** | TopBar separator, LI spinner, CS marker | primitives |
| **Placeholder text** | "Loading box art…", "No box art available" (body vs small) | `Placeholder` |
| **SectionLabel** | uppercase bold small muted ("AVAILABLE ROMS", "CATEGORIES", "SYSTEM", "OVERVIEW"); DESIGN says sentence case | decide |
| **SectionHeader(title, count)** | HS ×2, ST updater group heading, ST option heading (rule line), Settings column header | one |

Tokens to extract (all literals today):

- **Radius:** 4 (table header), 5 (thumbs), 6 (AG rows, Scan rows, OSK keys, hint frame), 7 (time zone),
  8 (rows; the majority), 9 (boxes in updater), 10 (`radius()`, panels, summary bar, tile body), 18
  (CrashScreen). Proposed: `chip` 5, `row` 8, `panel` 10.
- **Stroke:** 1, 2 (rows, modal), 3 (tile, emphasis).
- **Alpha:** 40, 55, 60, 70, 80, 90, 100, 110, 120, 140, 150, 155, 160, 170, 175, 180, 190, 200, 205, 210,
  215, 220, 230, 235, 238. Proposed named levels: scrim (3), muted tiers (3), disabled, overlay surface.
- **Spacing:** pad 24/26/28/30/34/36/44; title gap 4/12/16/18/20/48/52/54/58/72/78/96; row gap 4/5/6/8;
  text line extra 3/4/6/8/9/10.
- **Font:** sizes through `Theme`, except CrashScreen (`px()` literals bypass the text-size setting).
- **Colors outside Theme:** `0x111318`/`0xFFFFFF` (contrast text, Silver special case in APP and DS),
  `0x07090D` (light top bar), `0x000000` (screensavers), `0xA0A0A0` (disabled underline), `0xFFFFFF`
  (HC borders in ST, search, DS), `0x17315F`/`0xD6D8DE` (checkbox).

## 3. Behavior inventory

- **Focus easing:** instant (GS, SS, HS, Tile) vs eased `dt*9` (SVS, AG) vs fade (TopBar underline 45 ms).
  DESIGN: instant for tiles; decide for rows.
- **Left/Right in a list:** ±10 rows in GS list (not in the hints), value change in ST, filter in updater,
  nothing elsewhere.
- **Back on busy pages:** cancel (Scan Working, Checking, Bluetooth scanning), ignored (Installing,
  Controllers raw capture), close (most).
- **Start:** shortcut for submit (search, keyboard), for review (updater List), skip (CS wizard); never
  offered on Intro/Review pages as DESIGN allows.
- **Confirm vs Back to close:** Info modal closes on both while its hint says only "B Close".
- **Hold actions:** hold X favorite, hold B remove from recents, in APP; two near-identical code blocks.
- **Jump to letter:** three behaviors (scroll to top, cursor only, cursor only).
- **Y:** change view (GS), change view (HS search only), filter (AG), re-check (updater), space (keyboard).
- **Errors:** `Context::showError` (CS, scans), inline text (Scan, updater), toast (DS, 15 strings).
- **Disabled rows:** selectable but inert, Confirm gives no feedback (ST).

## 4. Duplicated logic worth extracting (non-visual)

Filter loop (updater, ×2), byte formatter (×3, "MB" vs "MiB"), status label/color mapping (×2),
dirty-rect union (SS, GS), image-request-before-paint (×4), favorite toggle and launch (HS, GS, AG),
jump-to-letter (×3), "draw focused tile last" two-pass (×4), painted-state tracking (×4), focus easing
loop (×5), Game options setup (`openGameActions`, `openGameVersionChooser`), version string (APP ×2),
value cycling in ST (≥6 hand-written `while`-find loops with duplicated tables).

## 5. Candidate component list (post-survey)

The first-read hypothesis in UI-FRAMEWORK.md holds, with these changes from the survey:

- **Add:** `PageHeader`, `SearchHeader`, `EmptyState`, `ProgressPanel`, `ScrollView` + `Scrollbar`,
  `KeyValueList`, `TextBlock`/`RichText`, `Badge`, `SectionHeader`, `TextField`, `ActionButton`, `Picker`,
  `Toast` (tone), `ScreenStack`, `DetailPanel`, `Table`.
- **Split:** `Modal` into frame (`Panel`) and variants; `ListItem` types made explicit (Nav, Value,
  Switch, Choice, Action, Info, Heading, Status).
- **Keep unchanged:** `Tile` fast paths, box art sizes, `ImageCache`, video slot.
- **Not a framework matter:** CS pad diagram and live pad, AG flag glyphs, DS metadata grid: keep as
  specific components built from primitives (they are inside the "exception with reason" rule or are
  real one-off visualizations).

## 6. Hints

Sources today: `Screen::hints()` (per screen), `APP::renderBottomBar` (Game options, hold hints, update
phases), `GameSearch::hints()` (prepends/replaces the owner's hints and detects `"Y Change view"` with
`find()`), footers drawn inside dialogs (ST: Changelog, Wi-Fi, time zone, Disc install, update confirm,
reboot, info, account; ErrorModal), and body text (CS wizard, Bluetooth "Press A").

Problems: footers inside dialogs repeat the bottom hint (against DESIGN) and diverge in wording
("A Set Linux time zone" vs "A Set this time zone", "A Reboot" vs "A Restart MiSTer", "A Install" vs
"A Install package"); some skip `formatButtonLabels` (Changelog, Info "B Close"); order violates the
DESIGN rule in HS, GS, SS ("A" before movement); the D-pad is `{DPAD}`, "D-pad", "↑/↓", "↑", "↓" in
different places; "Show/Hide" has no key; `hints()` and the in-panel text differ for the same state;
the Info modal and hints disagree about which button closes; Start is never offered as a shortcut on
Intro/Review.

Proposal: components declare hints as data (`{key, action}` in priority order); `Hints` orders, formats
and fits them; dialogs never draw a footer; the bottom bar shows the top modal's hints.

## 7. Bugs and risks found

- **Game options scroll mismatch:** key handling assumes `px(300)/px(49)` rows, render uses 44 + 5 and a
  different visible count (APP 1018 vs 816-818). The focused row can scroll out of view.
- **Disc screenshot viewer counter** "n / m" is placed at the center x without centering on its width
  (DS 416).
- **Unreachable controllers:** CS list `break`s when rows exceed the area; no scroll.
- **Reboot dialog** has a fixed 190 px height and can clip at Largest; Info modal silently truncates.
- **Tile caption** numbers do not add up (28 + 22 drawn vs 26 + 20 reserved); sublabel is not elided.
- **CrashScreen and fallback fonts** ignore the text-size setting.
- **Hard-coded alpha in SettingsScreen `line()` sentinel** (`Color{0,0,0,0}`) and render-written state
  (`coreUpdaterVisibleCount_`, `visibleProgressRows_`, `wifiMessage_`) that input handling reads:
  render feeds input, which breaks if a frame is skipped.
- **Dead state:** `coreUpdaterTypeFilter_` (set, unused), `focus_` in ArcadeSettingsScreen, unused
  `renderDetailBackground` in GS, unused non-cover tile growth branch.
- **High contrast:** white borders only in ST rows and search; tiles, GS/SVS/AG/CS rows and dialogs get
  none.
- **Success shown as neutral:** updater result success text is `textMuted`, not the accent.
- **Wrong hint text:** HS hint omits RemoveRecent; "←/→" paging in GS list is undocumented; ST help
  duplicates ("Manage systems" and "Manage Consoles").
- **Hard-coded content:** a Zelda quote in the Scan overview.

## 8. Decisions for the specification step

Each needs one answer before extraction; the proposal is the default.

| Decision | Proposal |
| --- | --- |
| Row radius | 8 (`row`); chips 5; panels 10; drop 4, 6, 7, 9, 18 |
| Row focus | outline 2 px `focus` + 4 px accent marker; no easing |
| Tile focus color | `focus` (same as rows), 3 px |
| Panel padding | 28; title gap 16; screen margin 36 |
| Scrim | one level for dialogs, one for viewers, one for screensavers |
| Dialog shadow/stroke | no stroke except Error (`error`); shadow on all or none, decided by cost on CPU |
| Dialog footer | none; hints only in the bottom bar |
| Sizes | presets `small`, `short`, `list`, `error`, `full` |
| Progress bar | one height (12), `surface` track, accent fill, min fill = height |
| Scrollbar | 4 px, `focus` thumb, 8 from edge, range text "a–b of n" |
| Status | label + icon + tone; add `error` color |
| Text | line step +9; sentence case section labels; small only for secondary detail |
| TabRow | instant underline, same component in top bar and dialogs |
| Wizard | optional page id; flow owns data; route as data |

## 9. Accessibility audit (current gaps)

Measured against [ACCESSIBILITY.md](../src/ui-framework/docs/ACCESSIBILITY.md) (the framework's guide). Application-specific, so
it lives here and not in the guide. Avoided by construction in the New GUI; fixed in Classic only if severe. Contrast figures
come from the second survey pass and are to be re-measured by the contrast registry in M1.


Known; avoided by construction in the New GUI, fixed in Classic only if severe:

- Muted text drawn with alpha 110 to 190 on `surface`/`surfaceHi` without a contrast check; text alphas
  are unnamed.
- `warning` (coral red) is used for both warnings and errors; the intended orange/red split does not
  exist yet.
- Decision-relevant text at `small` size (Home empty hints, Scan progress names and statuses, Arcade
  table, state pills).
- Status shown by color and one word only in places (updater "Update available" and "Not installed"
  share the accent; Disc and Scan status lines).
- Hold-only functions: hold X favorite, hold B remove from recents.
- Controller list rows that can exceed the screen with no scrolling (devices unreachable); Reboot
  dialog with a fixed height that clips at Largest; Info dialog silently truncating.
- Search focus in `accent`, tile focus in `lerp(accent, focus)`: different focus colors; high-contrast
  borders only in Settings rows and search, not on tiles, other lists or dialogs.
- Hints inside dialogs that differ from the real behavior (Info says only "B Close", A also closes).
- CrashScreen font sizes ignore the text-size setting.
- Tab underline fades (short, decorative; allowed, but listed).
- Idle and Bluetooth countdowns are not announced where the user reads them.
- **Checkbox contrast:** outline `#17315F` on `surfaceHi` `#222839` is about 1.15 : 1 (1.3 : 1 on
  `surface`), so an unchecked box is practically invisible in dark mode; in light mode the dot
  `#D6D8DE` on a near-white row is about 1.3 : 1. Only High contrast changes the outline, and the dot
  never follows the highlight. Fails 1.1 (3 : 1 for non-text).
- **Focus invisible in places:** Game options focused ROM row is `surfaceHi` on a `surfaceHi` panel
  (only the text color changes); the unfocused screenshot thumb stroke is `surfaceHi` on the same
  panel; Home/Games/Systems tiles use `lerp(accent, focus, .65)`, search uses `accent`, keys use only an
  alpha fill. Fails 1.4 (one strong, consistent focus cue).
- **Selection and focus conflated:** the Scan "active" progress row is drawn with the focus outline,
  although it is not focus; "Selected" is plain accent text on ROM rows; the controller list uses the
  same "*" for "shares mapping" as for favorites.
- **Largest text:** updater rows place a bold title at +9 and a small line at +35 (about 8 px overlap at
  130 %); the Games favorites row needs about 70 px in a 62 px frame; Settings text blocks and the Scan
  pages use fixed offsets.
- **Readability:** paragraphs hand-wrapped as separate strings per line (Scan), line steps from +3 to
  +10, uppercase section labels, text drawn over cover art gradients only via alpha.
- **Disabled rows:** the value of a disabled Settings row is still drawn in the accent; Confirm on it
  does nothing and says nothing.
- **Scrolling:** Settings options scroll pins the cursor to the bottom edge (moving up scrolls on every
  press); several lists have no position cue (see section 7).

## 10. Corrections and additions from the consolidation pass

A second pass (per component family, across all screens) re-checked this inventory against the code.
Where it differs from sections 1 to 9, this section wins. Classic is frozen (see UI-FRAMEWORK.md,
"Delivery"); these items are inputs for the New GUI and for the parity checklists.

**Corrections**
- **Quit** is handled at 11 sites, not 5; at `App.cpp:997` Quit closes Game options instead of stopping.
- **Settings modal state** is 7 bools, 2 enums and 2 service-phase prompts (not "11 bools");
  `wantsBack()` has 10 terms and omits both prompts.
- **Game options scroll "mismatch"** (section 7) is a latent coupling, not a live bug: at 1080p both
  sides compute 6 rows.
- **`ArcadeSettingsScreen` is dead code**: `openArcadeSettings` is never called; Settings opens the
  Arcade game list directly.
- **Value rows:** Left/Right switch panes (hint "←/→ Change column"); a Value row cycles on Confirm.
- **Progress bars:** 11 bars plus 2 gauges (deadzone, trigger), not 9. The updater's Installing bar is
  per-file bytes and saw-tooths while the text counts files. `Splash::show` runs its task
  synchronously, so no frame is drawn while it runs.
- **Row pitches** also include 49 (ROM rows) and 58 (controllers). The Scan "active" row is current work
  drawn with the focus look (a misuse). The controller list's `*` means "shares mapping", not favorite.
  The Settings options list scrolls statelessly (cursor pinned to the bottom edge). Games list focus
  keeps per-row float vectors for a 0/1 value.

**Additional findings**
- **Box art is enlarged** by arithmetic: the Games grid draws a 245 px art about 5 % larger (the grid
  stretches cells to the width), the list detail about 7 % larger, against the "never scaled" rule.
  Computed from the code, to be confirmed on the device. The list detail also draws a drop shadow behind
  the cover.
- **Home:** un-favoriting from the Favorites grid resets `activeRow_` (focus jumps to "Recently
  played"); letter jump also runs on the time-ordered "Recently played"; reserved-but-unused focus
  growth margin (9 px per side, 22 px per section); the non-cover growth branch of `Tile` is unused.
- **Games:** `visibleArtwork` is allocated every frame, focus and painted vectors are O(N) per repaint
  (up to about 17k entries).
- **Modals:** the Error modal contributes no hints (stale screen hints stay under the scrim) and hides
  Game options; Settings dialogs do not block LB/RB (the tab changes behind the dialog); B cancels a
  running update download from any tab; hold bars are not gated by modals; a second `showError`
  overwrites the first; update prompts are routed before the error check.
- **Flows:** the updater's Installing page always sets `installResult_` even when the install failed
  (no Try again); Start submits only on Wi-Fi password entry, not on SSID or country entry.
- **Toast:** about 75 `notify` call sites in 12 files; every message draws in the success blue,
  including failures; long toasts overlap the hints (the hint fit ignores the toast width).
- **Hints:** `formatButtonLabels` renders "↑ Start game" as "↑ (START) game"; `GameSearch` looks for
  "Y Change view" while Games says "Y Change game view" (the hint is silently lost); Changelog, time
  zone and Info footers skip `formatButtonLabels`; hold hints ignore the Hint-size setting; dialogs say
  "D-pad" because plain `font.draw` cannot render `{DPAD}`.
- **Text:** six byte formatters (KB/MB/KiB/MiB mixed), four duration styles, hand-coded plurals,
  "box art" / "boxart" / "cover art" for one thing; `Font::elide` appends ASCII "..." while strings use
  "…" or " ...". Muted text alpha tiers: 190 is about 4.6 : 1, 160 about 3.7 : 1, 120 about 2.6 : 1 on
  the dark background (estimates; light themes not measured).
- **`Font::elide` splits UTF-8 characters** (found while building the framework's copy): it removes
  continuation bytes but leaves the lead byte, so an accented character at the cut shows a broken glyph
  ("Pokémon" can end as "Pok\xC3..."). Invisible with English text, visible with accented titles and any
  translation. The framework's copy is fixed; the original in `src/legacy/Font.cpp` is a small, safe bug-fix
  candidate for Classic (owner's call; Classic is otherwise frozen).

