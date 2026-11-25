# Component overview

All framework components, by layer, from the smallest building block up to the whole-screen flows.
Each row links to its page (ASCII mockups, states, measurements, behavior, accessibility, API). The
conventions and the page template are in [README.md](README.md); the demo application that shows all of
them is [UI-DEMO.md](../UI-DEMO.md); the design is in
[UI-FRAMEWORK.md](../UI-FRAMEWORK.md); the survey the list comes from is
[UI-INVENTORY.md](../../../../docs/UI-INVENTORY.md); the accessibility rules every page is checked against are in
[ACCESSIBILITY.md](../ACCESSIBILITY.md); how modes, color themes and high contrast work is in [THEMING.md](../THEMING.md); the language requirements are in [I18N.md](../I18N.md).

Status: **proposal** (derived from the survey, waiting for the M0 decisions), **agreed** (owner signed
off), **implemented**. Implemented in the wireframes so far: Tokens, the primitives (FocusFrame, Checkbox, Surface, Label, TextBlock, Icon, ProgressBar, Scrollbar, Hints, HoldProgress), ListItem, GridItem, NavItem, ActionButton, TextField, Keyboard, PageHeader, TextEntryDialog, ResultItem, Shell, List, Grid, Table, KeyValueList, EmptyState, DetailPanel, ProgressPanel, Modal, ModalStack, Wizard, Splash, CrashScreen. The others are proposals.

How the layers depend on each other: a component uses only components from its own layer or below.

```
 Takeovers    Splash, CrashScreen
 Flows        Wizard (+ WizardPage)
 Overlays     Modal (+ Viewer, Choose), ModalStack, TextEntryDialog
 Containers   Shell, Screen, PageHeader, List, Grid (+ TileRow), Table, Result, KeyValueList,
              EmptyState, DetailPanel, ProgressPanel
 Items        ListItem, GridItem, ResultItem, NavItem, ActionButton, TextField, Keyboard
 Primitives   FocusFrame (+ FocusMarker, SelectionMark), Checkbox, FavoriteMark, Surface (+ Panel,
              Scrim), Icon, ProgressBar, Scrollbar, Badge, Label, TextBlock, TabRow, Hints, HoldProgress, Toast
 Tokens       Radius, Stroke, Space, Alpha, TextRole, StatusColor
```

## Tokens

| Component | Page | What it is | Replaces |
| --- | --- | --- | --- |
| Tokens | [Tokens.md](Tokens.md) | named radii, strokes, spacing, alpha levels, text roles, status colors | literals scattered over every screen |

## Primitives

| Component | Page | What it is | Replaces |
| --- | --- | --- | --- |
| FocusFrame | [FocusFrame.md](FocusFrame.md) | the one focus outline, the focus marker bar, the selection mark | 8 focus-frame looks |
| Checkbox | [Checkbox.md](Checkbox.md) | round on/off control with its On/Off word | `drawRoundCheckbox`, pills |
| FavoriteMark | [FavoriteMark.md](FavoriteMark.md) | the favorite mark on art and in rows | tile badge, list `*`, hint `*` |
| Surface | [Surface.md](Surface.md) | filled rounded surface roles, the panel, the scrim | ~120 `fillRoundedRect` calls |
| Icon | [Icon.md](Icon.md) | `Icon` and `BootstrapIcon(name, width, height)`: SVG icons in any color; the glossary of symbols that need no text | `{DPAD}`, check/cross, tab icon bitmaps |
| ProgressBar | [ProgressBar.md](ProgressBar.md) | one calm progress pill | 9 progress bars |
| Scrollbar | [Scrollbar.md](Scrollbar.md) | scroll bar and position text | 3 scroll cues, none elsewhere |
| Badge | [Badge.md](Badge.md) | short status word with tone | state pills, status words |
| Label | [Label.md](Label.md) | one line of text in a role: elide, align, fit | hand-written right alignment, fit strategies |
| TextBlock | [TextBlock.md](TextBlock.md) | wrapped paragraph, line step, rich text | 7 wrappers, 2 markdown parsers |
| TabRow | [TabRow.md](TabRow.md) | underlined tabs | top bar underline, updater tabs |
| Hints | [Hints.md](Hints.md) | the bottom-bar legend: ordering, vocabulary, fitting | 4 hint sources, in-dialog footers |
| HoldProgress | [HoldProgress.md](HoldProgress.md) | the hold hint with a filling bar, shown in the bottom bar while a button is held | 2 hold lines in the bottom bar |
| Toast | [Toast.md](Toast.md) | transient confirmation line (implemented) | `Context::notify` call sites |

## Items

| Component | Page | What it is | Replaces |
| --- | --- | --- | --- |
| ListItem | [ListItem.md](ListItem.md) | one list row, seven kinds | 13 row looks |
| GridItem | [GridItem.md](GridItem.md) | a tile: cover, icon, placeholder | `Tile` variants |
| ResultItem | [ResultItem.md](ResultItem.md) | one line of an outcome or status list | updater/scan/Disc status lines |
| Selector | [Selector.md](Selector.md) | one value out of a short list, stepped with Left/Right: chevrons and the value | Classic's `<  All systems  >` picker |
| ActionButton | [ActionButton.md](ActionButton.md) | the large focusable action ("Start game") | Disc and Game options buttons |
| TextField | [TextField.md](TextField.md) | single-line text field | search, account and Wi-Fi fields |
| Keyboard | [Keyboard.md](Keyboard.md) | on-screen keyboard | `OnScreenKeyboard` |
| NavItem | [NavItem.md](NavItem.md) | a top-bar item (clock, disk, network) and the static footer text | top bar blocks, version label |

## Containers

| Component | Page | What it is | Replaces |
| --- | --- | --- | --- |
| Shell | [Shell.md](Shell.md) | the main-screen layout: tabs top center, nav items top left and right, help bottom left, static footer bottom right, content window | top bar, bottom bar |
| Screen | [Screen.md](Screen.md) | the page frame: bars, content area, states, focus owner | app frame, 7 takeover flags |
| PageHeader | [PageHeader.md](PageHeader.md) | title, right-hand info, optional centered search field; one fixed layout | 10 header copies |
| List | [List.md](List.md) | vertical list with cursor, scroll, position | 9 list implementations |
| Grid | [Grid.md](Grid.md) | tile grid and the one-row tile strip | `GridView` + Home's own grids |
| Table | [Table.md](Table.md) | weighted columns, header, flag cells | Arcade table |
| Result | [Result.md](Result.md) | list of `ResultItem` with summary and next step | live lists, review boxes |
| KeyValueList | [KeyValueList.md](KeyValueList.md) | label/value rows | info modal, facts, Disc metadata |
| EmptyState | [EmptyState.md](EmptyState.md) | why it is empty and what to do next | 7 empty-state looks |
| DetailPanel | [DetailPanel.md](DetailPanel.md) | art, caption, status, video, screenshots | Games detail, Game options art side |
| ProgressPanel | [ProgressPanel.md](ProgressPanel.md) | calm progress with one status line | `LoadingIndicator`, Scan working |

## Overlays

| Component | Page | What it is | Replaces |
| --- | --- | --- | --- |
| Modal | [Modal.md](Modal.md) | centered dialog: Message (Info/Confirm/Error/Result/Progress), Choose, Input, Viewer | 20 dialog frames |
| TextEntryDialog | [TextEntryDialog.md](TextEntryDialog.md) | the one on-screen-keyboard flow: title, field, keyboard, hints, Changed/Submitted/Cancelled | search keyboards in Home, Games, Settings |
| ModalStack | [ModalStack.md](ModalStack.md) | owner of "what is on top", input routing, hints | modal flags, `wantsBack`, hint chains |

## Flows

| Component | Page | What it is | Replaces |
| --- | --- | --- | --- |
| Wizard | [Wizard.md](Wizard.md) | multi-step flow with pages | 7 hand-built flows |

## Takeovers

| Component | Page | What it is | Replaces |
| --- | --- | --- | --- |
| Splash | [Splash.md](Splash.md) | startup screen | `Splash` |
| CrashScreen | [CrashScreen.md](CrashScreen.md) | failure screen | `CrashScreen` |

## Application-level compositions (not framework)

These stay in the application, built from the components above, and get their mockups with the
application's own screens: the controller pad diagram and live pad, the Arcade flag glyphs, the Disc
metadata grid, game-specific tiles and the Game options content.
