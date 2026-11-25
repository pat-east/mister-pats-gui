# Shell

The main-screen layout of every application built on the framework: six fixed regions around the content
window. Use it as the one frame of an application. Do not use it for the splash, the crash screen or a
dialog (those are takeovers and `Modal`).

Layer: container. Status: **proposal.** Values marked (M0) wait for the owner's decision.

## The premise (owner, 2026-11-22)

| Region | Place | Holds |
| --- | --- | --- |
| **Tabs** | top center | the tab row |
| **Nav left** | top left | nav items (for example clock and date) |
| **Nav right** | top right | nav items (for example disk usage, network) |
| **Help** | bottom left | **always** context-sensitive help: the input options that work right now |
| **Footer** | bottom right | **static** text outputs (for example the version) |
| **Content window** | middle | whatever the active tab shows (`Screen`) |

Two rules follow, and the framework enforces them:

- The Help region **always** shows help, never anything else, and is never empty while the shell is
  visible. What is not help goes to the footer or a toast.
- The Footer region shows **static** text only. Anything that changes with the user's actions goes to the
  help region, a toast or the content window.

## Anatomy

The whole 1920 x 1080 design canvas; the numbers are from the current layout (M0 where marked).

```
+--------------------------------------------------------------------------------------------+
| 10:42              Overview  Text  Lists  Grids  Dialogs  Flows  Input  Appearance    Disk 62%|  top bar 96
| Tue 22 Nov                         ^^^^^^^^ active: bold + underline                 Ethernet|
|  ^ NAV LEFT                        ^ TABS (centered)                          NAV RIGHT ^    |
+--------------------------------------------------------------------------------------------+
|                                                                                            |
|    +--------------------------------------------------------------------------------+      |
|    |                                                                                |      |
|    |                        CONTENT WINDOW                                          |      |  margins
|    |                        (the active tab's Screen)                               |      |  72 / 32
|    |                                                                                |      |
|    +--------------------------------------------------------------------------------+      |
|                                                                                            |
+--------------------------------------------------------------------------------------------+
| {DPAD} Move   A Open   B Back                                              Version 1.4.2    |  bottom bar 72
|  ^ HELP (context-sensitive, always)                                  FOOTER (static) ^      |
+--------------------------------------------------------------------------------------------+
```

- Top bar 96 high, bottom bar 72 high, side margin 72, vertical margin 32 (from the current theme).
- Nav left starts at the side margin; nav right ends at the side margin; tabs are centered on the canvas
  between them.
- A toast is not part of the bottom bar: it has its own slot just above it (see `Toast`), so help and
  footer never fight for room.

## Variants

```
tab display: Names           Overview   Text   Lists   Grids
tab display: Icons           [i]  [i]  [i]  [i]                 (icons only; glyphs from the glossary)
tab display: Both            [i] Overview   [i] Text   [i] Lists
tabs locked (a takeover      Overview   Text   Lists               dimmed; LB/RB do nothing; the
is open)                                                            helper area shows the takeover's help
```

- Tab display is a user preference (names, icons, both), as today.
- **Light mode:** the top bar stays black with white text and fades into the page background below its
  icons, as today (a token, not a literal).
- **Without tabs:** an application with one view omits the tab region; the layout does not shift.

## States

```
normal                help shows the screen's help; footer shows its static texts
modal open            the scrim dims the whole canvas including the top bar; help shows the TOP MODAL's
                      help; tabs locked; the footer stays
nav item in warning   the item shows a warning tone with its word and icon (never color alone)
too narrow            nav right drops its lowest-priority items first; then tabs switch to icons
                      (M0); the help drops non-essential hints from the right; the footer never clips
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Top bar | 96 | `Theme.topBarHeight` |
| Bottom bar | 72 | `Theme.bottomBarHeight` |
| Side margin / vertical margin | 72 / 32 | content rect x 72, y 96 + 32 |
| Nav left start | x = side margin | first item's top at y + 16 |
| Nav right end | x = canvas width - side margin | first item's top at y + 18 |
| Tabs | centered; label body-large `Tab` role; spacing 54 (icons) / 44 (text) | underline 3 high, radius 2, 4 below text, `focus` |
| Help | left at side margin, vertically centered in the bar | `Hints` fitting and ordering |
| Footer | right at side margin, `small` role, `Muted` tier | right-aligned list, 26 gap between items |
| Toast slot | above the bottom bar | `Toast` |

The caller supplies: tab list and active tab, nav items for both sides, the help (`Hints`), the footer
texts. The component decides: positions, fitting, dropping, tone colors, high contrast, locale
formatting through its items.

## Behavior

- **LB/RB** switch tabs (wrapping); a locked shell ignores them.
- The top regions are display-only in the first version (a `NavItem` may become actionable later, D45).
- The content window gets a `Rect` and redraws only its dirty area; the shell redraws only the nav
  items, the help or the footer when their text changed (the clock once per minute).
- Back and Confirm are not shell actions: they belong to the content window or the top modal.
- The help region reads the active `Screen` or the top `Modal`: one source (`Hints`).

## Accessibility

- Nav text and footer text meet 4.5 : 1 on the top and bottom bar in every theme and mode (registry).
- Status in nav items is a word plus an icon; the tone only colors it. Time and date follow the locale.
- The help region uses the hint-size setting and never clips: fit, then drop non-essential hints.
- The tab row is the only focus-like element in the top bar; it carries the underline plus bold weight, not
  color alone.

## Strings

| Key | English source | Note |
| --- | --- | --- |
| `shell.tab.*` | tab labels | supplied by the application; short; no fixed width |
| `shell.footer.*` | footer texts | static, supplied by the application |
| `hint.*` | the hint vocabulary | owned by `Hints` |

## Largest text, other languages and high contrast

```
Largest (130 %): nav items and tabs grow; tabs may switch to icons to keep the bar from overlapping

   10:42                       [i]  [i]  [i]  [i]  [i]  [i]  [i]  [i]                  Disk 62 %
   Tue 22 Nov                                                                          Ethernet

German-length help: the least important hints drop from the right
   {DPAD} Bewegen   A Öffnen   B Zurück   ...
```

High contrast: black bars, white text, underline and warning words in the highlight color.

## API

```cpp
struct ShellSpec {
    std::span<const TabSpec> tabs;  int activeTab;  bool tabsLocked;
    std::span<const NavItemSpec> left, right;
    const Hints &help;                       // the top modal's, if one is open
    std::span<const FooterTextSpec> footer;
};

Rect drawShell(Painter&, Theme&, const Rect &canvas, const ShellSpec&);   // returns the content rect
class Shell { public: void update(float dt); bool dirty(Rect &out) const; /* regions that changed */ };
```

## Derived from / Used by

Derived from: `TopBar.cpp` (clock and date, tab row, network and disk-space blocks), `App.cpp`
(`renderBottomBar`: hint line, version, toast), `Theme` metrics.

Used by: *none yet.*
