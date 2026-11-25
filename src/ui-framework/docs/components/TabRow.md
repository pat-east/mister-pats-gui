# TabRow

A row of a few short labels that switch between views or filters of the same content, with an
underline under the active one. Use it for the main navigation and for filters inside a screen or
dialog. Do not use pills, filled chips, a background bar, a "Show" label or arrow glyphs beside it.

Layer: primitive. Status: **implemented for the in-screen variant** (`kit/TabRow.h`, `TabRow.cpp`: `drawTabRow(painter, theme, area, TabRowSpec{labels, active})`); the top bar's tabs are still drawn by `Shell`.

## Anatomy

```
  Needs update        All        Up to date        Changed
  ============
  ^ active: bold, Primary tier, underline under the label         ^ inactive: regular, Muted tier
  <--36--> gap between labels

  underline: 3 px high, radius 2, `focus` color, exactly as wide as the label, 4 px below the text
  line; 16 px of space between the underline and the content below
```

## Variants

The same component is used in two places; only the size and an icon option differ.

```
in a screen or dialog    Needs update    All    Up to date    Changed          label size body,
                         ============                                          starts 4 in

top bar, names           Home    Games    Systems    Arcade    Settings        label size tab,
                         ====                                                  spacing 44

top bar, icons + names   [h] Home   [g] Games   [s] Systems   [a] Arcade       icon 26, gap 10,
                         ========                                              spacing 54

top bar, disabled        Home    Games    Systems                              underline in the
                         ----                                                  Disabled tier
```

- Labels are short nouns or states ("Needs update", "All", "Up to date", "Changed").
- A tab row shows no hint of its own; the key that switches it is named in the bottom bar.

## States

```
active          bold, Primary, underline          the selected tab
inactive        regular, Muted, no underline
changed         the underline moves to the new tab in the same frame (instant) (M0)
disabled        row muted, underline in the Disabled tier; used while the main navigation is locked
                (for example while a full-screen task is open)
```

The underline is instant: the 45 ms fade in the top bar is at most one intermediate frame at the
repaint rate, so nothing is lost (M0). There is no focus frame: a tab row is switched by a key and is
not a cursor target, so selection is carried by weight, color and underline together.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Underline | 3 high, radius 2, `focus`, label width | 4 below the text line |
| Label gap | 36 (dialogs); 44 text only and 54 with icons (top bar) | design px |
| Label size | body in dialogs; tab (27) in the top bar | |
| Icon | 26, gap to label 10 | top bar, icons mode only |
| Space below | 16 | between underline and content |
| Left start | 4 in dialogs | |
| Weights | active bold, inactive regular | |

The caller supplies: labels (and icons), the active index, enabled or disabled. The component decides:
spacing, underline, tiers, positions.

## Behavior

- **Top bar:** LB / RB switch tabs (handled by the application's dispatcher, not by the row).
- **Dialogs and screens:** Left / Right switch the filter when nothing else uses that axis; wraps
  around; switching puts the cursor of the content below on its first item.
- The bottom bar names the key and what it changes: `Left/Right Change filter` (not a text next to the
  tabs).
- A tab row never takes focus; Up/Down belong to the content under it.

## Accessibility

- Three cues for the active tab: bold, brighter text and the underline; color is never the only cue.
- Inactive labels meet 4.5 : 1 (Muted on the surface); the underline meets 3 : 1 (`focus` on the
  background or surface).
- Labels are words, not icons alone; in icons-only mode the top bar uses glossary icons and keeps the
  active label visible (M0).
- At Largest the labels elide; row height grows with the text.
- No motion: the underline jumps to the new tab.

## Largest text and high contrast

```
Largest (130 %):   Needs upd...      All      Up to da...      Chan...
                   ============
```

High contrast: active label white, underline in the highlight color, inactive labels in the
high-contrast muted color.

## API

```cpp
struct TabSpec { std::string_view label; IconId icon = IconId::None; };
struct TabRow { std::span<const TabSpec> tabs; int active = 0; bool enabled = true;
                bool showIcons = false; bool showLabels = true; TextRole role = TextRole::Body; };
int  drawTabRow(Painter&, Theme&, const Rect& area, const TabRow&);   // returns height used
Rect tabLabelRect(Theme&, const Rect& area, const TabRow&, int index);   // for dirty regions
```

## Derived from / Used by

Derived from: the top-bar tab underline (`TopBar`, with a 45 ms fade and a hard-coded disabled color)
and the core-updater tab row (`SettingsScreen`, instant), both 3 px, radius 2 (inventory TabRow row).

Used by: *none yet.*
