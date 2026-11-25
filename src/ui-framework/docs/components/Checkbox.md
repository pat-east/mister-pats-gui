# Checkbox

The one round on/off control, optionally with its word (On / Off) in a fixed-width slot. Use it for
every switch and every selectable list entry. Do not draw square boxes, tick marks, pills or a
differently colored variant.

Layer: primitive. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext
milestone M0. The outline color and the dot color change from today's code; see Accessibility.

## Anatomy

```
     ___
    /   \          outline: round, 2 px, textMuted (M0)
   |  o  |         dot: filled circle inside, textPrimary (follows the highlight in high contrast)
    \___/          dot is inset by a quarter of the diameter
   <-20->          diameter 20 in a one-line row, 24 in a two-line row; follows body text size (M0)
```

Switch row with its word, right-aligned in a fixed-width slot so all checkboxes line up:

```
  |   Dark mode                                                          (o) On
  |   Video preview                                                      ( ) Off
                                                                          ^   ^
                                                                          |   word slot, width of
                                                                          |   the wider of On / Off
                                                                         gap 10 between box and word
```

## Variants

```
Switch    checkbox + word On/Off, right-aligned, fixed-width slot         (o) On      ( ) Off
Choice    checkbox alone at the left edge of a two-line row, before text  (o)  Item name
```

- **Switch** is for a setting with two states. The word carries the state next to the dot.
- **Choice** is for picking entries in a list. It appears only when the entry can be selected; a row
  that cannot be selected has no checkbox and no empty placeholder (the text moves left).

## States

```
off              ( )   outline only, nothing inside
on               (o)   outline plus the dot            <- (o) is the filled dot in the real UI
disabled off     ( )   outline and word in the Disabled tier
disabled on      (o)   outline and dot in the Disabled tier, word muted
focused row      the row shows focus; the checkbox does not change with focus or selection
```

```
Switch row, all four combinations (the row shows focus, the checkbox shows state):
  |#| Dark mode                                                         (o) On     focused, on
  |#| Video preview                                                     ( ) Off    focused, off
  |   Dark mode                                                         (o) On     unfocused, on
  |   Video preview                                                     ( ) Off    unfocused, off
```

State is never color alone: the dot is the state, and Switch also shows the word.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Diameter, one-line row | 20 | derived from body text size: about 0.95 x body (M0) |
| Diameter, two-line row | 24 | about 1.15 x body (M0) |
| Stroke | `Stroke.focus` 2 | outline |
| Dot | inset a quarter of the diameter | filled circle |
| Outline color | `textMuted` (M0) | high contrast: `textMuted` too |
| Dot color | `textPrimary`; high contrast: highlight color (M0) | |
| Word slot | width of the wider of "On" / "Off" | text `body`, accent for On is not used: word in `textPrimary` |
| Gap box to word | 10 | |
| Right inset | the row's right inset 16 | |
| Left position (Choice) | row inset 18; text starts 18 after the box | |

The caller supplies: on / off, size class (one-line or two-line), disabled. The component decides
colors, geometry and high-contrast treatment.

## Behavior

- A checkbox draws state; the row owns the action. Confirm on the row toggles (Switch) or selects
  (Choice).
- It does not change with focus. The row shows focus.
- A row that cannot be selected (already current, unavailable) draws no checkbox at all.
- The word in a Switch changes with the state in the same frame.

## Accessibility

- The outline must be visible on both row surfaces: 3 : 1 against `surface` and `surfaceHi` (registry
  pair). Today's `#17315F` outline on `surfaceHi` is about 1.15 : 1 (1.3 : 1 on `surface`), so an
  unchecked box is practically invisible in dark mode, and the light-mode dot `#D6D8DE` on a near-white
  row is about 1.3 : 1. The proposal fixes both (outline `textMuted`, dot `textPrimary`).
- State is the dot and, for Switch, the word; never color alone.
- A checkbox is in the glossary of self-explanatory symbols; it needs no text label of its own, but a
  Switch always carries On / Off.
- Size follows body text at Largest; target size is the whole row (about 44 px or more).
- Disabled stays readable at 3 : 1 and the row can explain why.

## Largest text and high contrast

```
Largest (130 %): box 26 / 31 px, word grows, slot widens, row grows

  |#| Dark mode                                                       (o) On
  #######################################################################

High contrast, highlight yellow: outline light gray, dot yellow, word in the contrast text color

  +-----------------------------------------------------------------------------------+
  |#| Dark mode                                                       (o) On          |
  +-----------------------------------------------------------------------------------+
```

Light mode: outline and dot use the light-mode values of `textMuted` and `textPrimary` (dark gray on
white); the registry measures them.

## API

```cpp
struct CheckboxSpec {
    bool on = false;
    bool twoLine = false;       // 24 instead of 20
    bool disabled = false;
};

int  checkboxSize(Theme&, bool twoLine);                     // follows body text
void drawCheckbox(Painter&, Theme&, const Rect& box, bool on, bool disabled);
int  switchSlotWidth(Theme&);                                // wider of "On" / "Off" + gap + box
void drawSwitch(Painter&, Theme&, const Rect& slot, const CheckboxSpec&); // box + word
```

Colors come from theme tokens, never literals. No state is stored.

## Derived from / Used by

Derived from: `drawRoundCheckbox` in the Settings screen (switch rows, updater list), the state pill
of the system-visibility screen, the "Selected" word on Game options ROM rows (inventory C12, FocusFrame
page for the selection cue). Replaces the local helper with its two literal colors.

Used by: *none yet.*
