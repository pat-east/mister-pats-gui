# UI Wireframes: the demo application

A small application that shows **every framework component** in a real main-screen layout, in the look
of the current GUI. It is the executable form of the component pages: where a page draws a component in
ASCII, the demo draws it on the screen. It runs on the MiSTer (started from Settings), and on the host
(a window), and it is the test bed for performance, accessibility and i18n.

The demo is part of the framework (`src/ui-framework/demo/`). It contains no application content:
the examples are generic and exist only to show components. The application contributes only the entry
that starts it and, on a device, the platform services (display, input, system values).

## Starting it

On the MiSTer: **Settings, Development, UI wireframes** (a category of the existing Settings screen
on the development branch). On the host: the demo's own binary. Back leaves it.

## Flow

```
Splash  ->  Shell (main-screen layout)  ->  tabs, each with a content window of exemplary screens
```

1. **Splash** (the `Splash` component), including the calm status line when a start is simulated as
   long.
2. **Shell**: tabs top center, nav items top left and right, help bottom left (always, context
   sensitive), static text bottom right. See [Shell](components/Shell.md).
3. **Content window** per tab: a few screens built from the components, with generic content.

## The shell in the demo

```
+--------------------------------------------------------------------------------------------+
| 10:42              Overview  Text  Lists  Grids  Dialogs  Flows  Input  Appearance    Disk 62%|
| Tue 22 Nov                                                                          Ethernet|
+--------------------------------------------------------------------------------------------+
|                                                                                            |
|                          (the active tab's content window)                                 |
|                                                                                            |
+--------------------------------------------------------------------------------------------+
| {DPAD} Move   A Open   LB/RB Change tab   B Back                         Version 0.9.0-dev  |
+--------------------------------------------------------------------------------------------+
```

- **Nav left:** time and date. **Nav right:** disk usage and network. They are generic system
  properties, not application content; on a device the platform provider supplies real values, on the
  host a fake provider supplies fixed ones, and a switch in the Appearance tab forces the warning and
  offline states.
- **Help:** changes with the focused component and with an open dialog.
- **Footer:** the demo's version and a build note.

## Tabs and what they show

All content is generic (placeholder titles, sample values, generated images, no game data).

| Tab | Content window | Components shown |
| --- | --- | --- |
| **Overview** | the shell's regions labeled in the live layout; a short text on how to use the demo | `Shell`, `NavItem`, `Hints`, `TabRow` |
| **Text** | text roles and tiers; a long paragraph and rich text; status words in every tone; toasts; progress bars; a progress panel; an empty state | `Label`, `TextBlock`, `Badge`, `Toast`, `ProgressBar`, `ProgressPanel`, `EmptyState` |
| **Lists** | a settings-style list with every item kind; a long scrolling list with position text and scroll bar; a table; key/value rows; a result list | `ListItem`, `List`, `Scrollbar`, `Table`, `KeyValueList`, `ResultItem`, `Result`, `Checkbox` |
| **Grids** | the `PageHeader` (title, centered search field, view and position); a tile grid with cover, icon and placeholder tiles (generated images); a one-row tile strip; a detail panel. Up from the first row focuses the field, A opens the search dialog | `PageHeader`, `TextEntryDialog`, `GridItem`, `Grid`, `DetailPanel`, `FavoriteMark` |
| **Dialogs** | buttons that open each modal variant: Info, Confirm, Error (with a Details line), Result, Progress, Choose, Input, Viewer; a dialog over a dialog | `Modal`, `ModalStack`, `Surface` |
| **Flows** | two generic wizards, one Medium and one Large frame (Step, Choose single and multiple, Capture, keyboard, scrolling review, Busy with and without cancel, Result Done / Error / Warning with Try again); a replay of the splash; a preview of the crash screen | `Wizard`, `Splash`, `CrashScreen` |
| **Input** | text fields (plain, masked), the text entry dialog (on-screen keyboard), action buttons in every state, switches | `TextField`, `TextEntryDialog`, `Keyboard`, `ActionButton`, `Checkbox`, `FocusFrame` |
| **Data** | a table with flag cells, a key-value list, a result list, a progress panel and an empty state | `Table`, `KeyValueList`, `ResultItem`, `ProgressPanel`, `EmptyState` |
| **Appearance** | live controls: text size (100 / 115 / 130 %), hint size, dark or light, high contrast and its highlight color, color theme, language (including the pseudo-locale), forced nav states; a contrast report and a fit report | `Tokens`, theme engine, contrast registry, `Hints`, i18n |

## Controls

LB/RB change the tab, the D-pad moves focus, A activates, B goes back or closes a dialog, Start is the
shortcut it is in the real components. The help area always says what works now.

## Rules for the demo

- **Not application-related.** Every title, label, value and image is generic. Real system values only
  through the provider interface (nav items).
- **One component, one place.** A component is shown on the tab named in the table, in all its variants and
  states, with a one-line caption saying what to look at; no component is shown twice.
- **Every state is reachable** without code changes: focused, selected, disabled, empty, error, long text,
  Largest text, high contrast, other language.
- **Calm.** The demo follows the premises it demonstrates: no decoration, no motion, plain language.
- **Useful for review.** The owner can walk through it on the television with the controller and compare
  with the component pages; defects found there are fixed in the component, not in the demo.
- **Measuring.** A hidden counter shows frame time and redrawn area per frame, so performance on the
  MiSTer can be read off the screen (measure only; no long jobs, nothing written outside
  `/media/fat/mister-pat`).
- **Host and device alike.** The same demo runs on the host through a window presenter and a scripted
  input source, so tests can walk through it and record each state.

## Relation to the other documents

- The component pages (`components/`) are the specification; the demo is checked against them.
- The framework's golden-image and recording-painter tests render the demo's states.
- It is the early on-device proof of the framework (frame times, hand-over of input and display, fonts at
  1080p) before any application screen exists.

## Open questions

- **D44** The Development category: always present on the development branch; for 0.9.0, keep it as a
  hidden developer tool (a flag file) or remove it. Proposal: hidden, because the demo is also the best
  review tool for later changes.
- **D45** May a nav item become actionable? Proposal: not in the first version.
- **D46** Real or fixed system values on a device? Proposal: real values through the provider (it proves the
  platform seam), with the Appearance tab forcing the other states.
- Generated sample images for the grid tab: a small built-in set (patterns with the standard art sizes), so
  the art-size rules can be seen without any game data.
