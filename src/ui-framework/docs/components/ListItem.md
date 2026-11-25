# ListItem

One row in a vertical list: a label with optional detail, and a slot on the right (value, switch,
status, mark). Use it for every list of choices, options and entries. Do not use it for tiles (see
`GridItem`) or for read-only result lines (see `ResultItem`).

Layer: item. Status: **proposal.** Values below come from the survey and the consolidation pass; every
number marked (M0) is waiting for the owner's decision in vNext milestone M0.

## Anatomy

Default variant `Nav`, Roomy density, focused, in the active pane.

```
          <--------------------------- list width ---------------------------->
          +--------------------------------------------------------------------+  ^
          |#|                                                                  |  | frame
          |#|  <18>  Video settings                                  A         |  | 56 (M0)
          |#|                                                                  |  v
          +--------------------------------------------------------------------+
          ##########################  outline 2 px, focus, radius 8  ###########
           ^                                                         ^
           marker 4 px, accent, radius 2                             action glyph, right inset 16
                                                                     (glyph, no text: self-explanatory)
                                                                                    gap 8 (M0) to the next row
```

Left inset 18 to the label (body, `textPrimary` when focused, `textMuted` otherwise). Right inset 16
(M0). The marker is drawn only on the cursor row of the pane that has focus.

## Variants

Seven kinds. The kind is an explicit field, never guessed from the value text.

```
Nav       |#| Video settings                                              A
Value     |#| Text size                                            Larger      (Confirm cycles)
Switch    |#| Dark mode                                              (o) On
Choice    |#| (o)  File name of the item                      Update available      12.4 MB
          |#|      ~path/or/second line~
Action    |#| Check for updates                                            A
Info      |#| Version                                                  1.4.2     (focusable, inert)
Heading     **Display**
            ------------------------------------------------------------------   (not focusable)
```

- **Nav** and **Action** show the A-glyph in the user's layout (`Theme::buttonLayout()`: `A` Xbox, cross
  PlayStation, `B` Nintendo; see Hints.md; a glyph from the self-explanatory set, no text). Nav opens something; Action runs something.
- **Nav with a value** (a Nav item whose `value` is set) shows the value in `accent` just left of the A-glyph: the row opens
  something and says what is set now ("Time zone    System default  (A)"). Use it where a Value would be misread as a
  cycling row; Value (no glyph) is for rows that cycle.
- **Value** shows its current value in `accent`; Confirm steps to the next value.
- **Switch** shows a round checkbox and the word On or Off in a fixed-width slot, so all switches
  line up. State is carried by the checkbox dot and the word, never by color alone.
- **Choice** is the two-line row: checkbox at the left (only when it can be selected), bold title,
  `small` muted second line, a status word, and a right-aligned size or count. A row that cannot be
  selected has no checkbox and no empty space is reserved (the title moves left).
- **Info** is focusable so its explanation can be shown, but inert: no glyph, Confirm does nothing.
- **Heading** is not focusable and is skipped by Up/Down.

## States

Shown for `Switch`. The same states apply to the other kinds.

```
normal, pane inactive (remembered position)     faint fill, no marker, no outline
  |   Dark mode                                                          (o) On

focused (pane active, cursor)                   solid fill surfaceHi, marker, outline
  |#| Dark mode                                                          (o) On
  ####################################################################################

selected / on                                   the checkbox dot and the word carry it
  |   Dark mode                                                          (o) On      <- dot filled
  |   Dark mode                                                          ( ) Off     <- dot empty

focused and selected                            both cues; they do not replace each other
  |#| Dark mode                                                          (o) On
  ####################################################################################

disabled (temporarily not allowed)              title and value muted, stays focusable
  |   ~Dark mode~                                                       ~(o) On~
  Confirm shows one toast line from `reason`, for example "Not available while a game is running."

unavailable (cannot be chosen at all)           status word instead of a checkbox, Confirm silent
  |   File name of the item                                             Up to date
```

`(o)` is shown filled in the real UI; ASCII cannot tell the two apart, the words On / Off do.

## Measurements

Design pixels at 1080p; tokens once `Tokens.h` exists.

| Item | Roomy | Dense | Two-line | Notes |
| --- | --- | --- | --- | --- |
| Pitch (frame + gap) | 64 (M0) | 52 (M0) | frame 62 + 6 | Heading row 36 |
| Frame height | 56 | 46 | `max(62, text + 16)` | grows with Largest text, never overlaps |
| Gap | 8 | 6 | 6 | |
| Radius | `Radius.row` 8 | 8 | 8 | |
| Marker | 4 wide, radius 2, `accent` | same | same | cursor row of the active pane only |
| Outline | 2, `focus` | same | same | white 2 px border in high contrast, Roomy only (M0) |
| Fill | focused `surfaceHi`; rest: none / faint (`surface` α110) / solid (`surface`) | | | rest fill chosen by context, not per screen (M0) |
| Text inset | left 18, right 16 | same | checkbox at 18, text 18 after it | |
| Checkbox | 20 | 20 | 24 | size follows body text |
| Title / detail | body / small, muted | body | bold body / small muted | small only for secondary detail |

The caller supplies: kind, title, detail, value, status word and tone, checked, reason when disabled.
The component decides: all geometry, colors, high-contrast treatment, elision.

## Behavior

- **Up/Down** move the cursor one item, clamp at the ends (no wrap), skip Heading rows.
- **Confirm** runs the item: Nav opens, Action runs, Value cycles, Switch toggles, Choice toggles when
  selectable. On a disabled item it shows `reason` (or does nothing). On Info it does nothing.
- **Left/Right** are owned by the container: leave or enter a pane, page, or change a filter. A row
  never uses them itself.
- The hint bar shows the action of the focused item's kind (for example `A Change` for Value,
  `A Turn on` / `A Turn off` for Switch), not a fixed string for the whole list.
- The cursor and scroll position belong to the list's owner (two integers), not to the row.
- Switching to another list puts the cursor on its first item; returning keeps the position.

## Accessibility

- Focus is the outline plus the marker (3 : 1 or better against the row and its surroundings), so it
  never depends on hue. Selection is the checkbox dot and the word, visible when unfocused.
- The checkbox outline uses `textMuted`, not a dark blue, so an unchecked box is visible on both
  surfaces (the current `#17315F` on `surfaceHi` is about 1.15 : 1 and fails; see inventory).
- A disabled value is muted, not accent, and the reason is available.
- Row height follows the text at Largest; text elides with an ellipsis and never overlaps the value.
- Symbols used: A-glyph, round checkbox, scroll bar; all in the self-explanatory glossary.

## Largest text and high contrast

```
Largest (130 %), Choice row grows instead of overlapping:
  |#| (o)  File name of the item that is long and gets cut off with an ell...     Update available
  |#|      ~path/or/second line that also elides~                                      12.4 MB
  (frame is taller than 62; title and detail keep their line spacing)

High contrast, highlight color yellow (Roomy rows get a border, the cursor row also the marker):
  +--------------------------------------------------------------------------------------+
  |   Text size                                                                 Larger    |   white 2 px
  +--------------------------------------------------------------------------------------+
  +--------------------------------------------------------------------------------------+
  |#| Dark mode                                                               (o) On    |   yellow outline
  +--------------------------------------------------------------------------------------+
```

## API

```cpp
enum class ListItemKind { Nav, Value, Switch, Choice, Action, Info, Heading };
enum class Density { Roomy, Dense };

struct ListItemSpec {
    ListItemKind kind;
    std::string_view title, detail, value, status;
    Tone statusTone = Tone::Neutral;
    bool checked = false;
    std::string_view disabledReason;   // non-empty: disabled, Confirm shows it
    bool unavailable = false;          // cannot be chosen at all
};
struct ListItemState { bool cursor = false, paneActive = true; };

int  listItemHeight(Theme&, Density, const ListItemSpec&);           // frame, grows with text
void drawListItem(Painter&, Theme&, const Rect&, Density, const ListItemSpec&, ListItemState);
```

State (cursor, scroll) is owned by the list's owner; the item is drawn, never stored.

## Derived from / Used by

Derived from: Settings category and option rows, updater two-line row, Wi-Fi rows, time-zone rows,
Arcade settings row, Controllers rows, system-visibility rows, Games list rows (survey: inventory C12,
consolidation report). Replaces 13 row looks and the "kind guessed from the value string" logic.

Used by: *none yet.*

## Entry (implemented, 2026-11-22)

An eighth kind, **Entry**, is the content row of a Grid-List: a title, an optional second line, a favorite
mark on the right; Confirm opens or plays. It is Dense (frame 46, pitch 52; with a second line 62 / 68), the
title is regular and takes the cursor tier. The other kinds are controls (Roomy: frame 56, pitch 64).
