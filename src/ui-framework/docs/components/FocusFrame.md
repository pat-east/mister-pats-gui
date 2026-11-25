# FocusFrame

The one focus outline, the focus marker bar and the selection mark. Together they answer the two
questions a user asks at a glance: *where am I* (focus) and *what is chosen* (selection). Use them
for every focusable thing. Never use the focus look for "current work", "selected" or "available".

Layer: primitive (FocusFrame, FocusMarker, SelectionMark, RestBorder). Status: **proposal.** Values
marked (M0) wait for the owner's decision in vNext milestone M0.

## Anatomy

A focused Roomy row in the active pane: marker, outline, fill.

```
     +---------------------------------------------------------------------+   ^
     |#|                                                                   |   | row frame
     |#|   Video settings                                                  |   |
     |#|                                                                   |   v
     +---------------------------------------------------------------------+
     ####################################################################### outline: 2 px, focus,
      ^                                                                      radius 8, drawn inside
      marker: 4 px wide, accent, radius 2, full row height, under the outline  the frame (no bleed)
     fill: solid surfaceHi
```

The caller supplies a rect and "has the cursor". The component decides color, stroke, radius, the
marker and the high-contrast treatment.

## Variants

Shapes, one outline style, different radius and stroke by target.

```
Row     radius 8   stroke 2   list rows, table rows, buttons in a list      |#|  [ Row ]
Thumb   radius 5   stroke 2   screenshot thumbs, keys                       +--+ [thumb]
Tile    radius 10  stroke 3   cover tiles; border only, the tile never grows, bleeds 3 px outside
Field   radius 10  stroke 2   text fields, search field (outline in focus, not accent)
```

```
Tile focus (bleed 3 px outside the art body so the art is not covered):
      +===========================+     === = 3 px outline in focus
      |  +---------------------+  |
      |  |        art          |  |     art is drawn at its prepared size, never scaled,
      |  +---------------------+  |     never grows; footprint for repaint = frame.inset(-3)
      +===========================+
```

Companions:

```
FocusMarker   |#|  4 px accent bar at a row's left edge, only on the cursor row of the active pane
SelectionMark  (o) or  [check] Selected   the cue for "chosen / on / current" when a row has no
                                          checkbox: check icon + word, accent, never color alone
RestBorder     2 px white border on every Roomy row in high contrast (not for White highlight)
```

## States

Shown for a Roomy row.

```
normal (no cursor here, pane inactive)         faint fill, no marker, no outline
  |   Video settings

cursor in an inactive pane (remembered)        faint fill, textPrimary, no marker, no outline
  |   Video settings                           <- the position is remembered, not focused

focused (cursor, active pane)                  solid fill, marker, outline
  |#| Video settings
  #####################################################################

selected, not focused                          selection cue visible without focus
  |   Dark mode                                                    (o) On

focused and selected                           both cues, neither replaces the other
  |#| Dark mode                                                    (o) On
  #####################################################################

disabled, focused                              muted text, outline and marker still shown
  |#| ~Dark mode~                                                 ~(o) On~
  #####################################################################
```

`(o)` is the filled round checkbox in the real UI; the words On / Off carry the state in ASCII.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Outline stroke | `Stroke.focus` 2 (Tile: `Stroke.emphasis` 3) | clamped to at least 1 px |
| Outline color | `focus` | follows high contrast and light mode; never `accent`, never `lerp(accent, focus)` (M0) |
| Outline placement | inside the frame (Row, Thumb, Field); Tile bleeds `focusBleed` 3 | footprint = frame.inset(-3) for tiles |
| Radius | Row 8, Thumb 5, Tile 10, Field 10 | tokens |
| Marker | 4 wide, radius 2, `accent`, row height | under the outline; cursor row of the active pane only |
| Row fill | focused `surfaceHi` | rest fill is the list's choice (see ListItem) |
| Target size | at least about 44 px high | small targets are grouped into larger rows |
| RestBorder | 2, white | Roomy rows only, omitted when the highlight is White (M0) |

## Behavior

- Exactly one focus target is visible per screen or dialog at any time; the container owns the
  cursor, the component only draws it.
- Focus moves in the same frame as the input: the old target repaints without it, the new one with it.
  No easing, no fade, no growth. A move repaints two frames only.
- Focus is never hidden by an overlay, a scroll or a dimmed layer. When a dialog closes, focus returns
  to where it was.
- Selection is shown independently of focus and stays visible when the item is unfocused.
- The Scan "active" progress row is not focus; it is an emphasized row (surfaceHi fill, no outline).

## Accessibility

- Focus cue = outline plus marker, 3 : 1 or better against the row and its surroundings, independent
  of hue; high contrast uses the chosen highlight color and adds rest borders.
- The same treatment on every kind of target (tile, row, button, field, key, tab) so it is learned once.
- Selection is the checkbox dot with its word, or the check icon with a word; never a fill or a color
  alone. A focused and selected item shows both.
- In light mode the fill difference is small; the outline and marker carry focus.
- Pairs in the contrast registry: `focus` on background, surface and surfaceHi (3); marker `accent`
  on surfaceHi (3).

## Largest text and high contrast

```
Largest: row frame grows with the text, the outline and the marker follow the frame

  |#| File name that is long and elides with an ell...                    Update available
  |#|     ~second line, small, muted~                                              12.4 MB
  #####################################################################################

High contrast, highlight yellow: Roomy rows have a white rest border, the cursor row yellow
  +-------------------------------------------------------------------------------+   white
  |   Text size                                                         Larger    |
  +-------------------------------------------------------------------------------+
  |#| Dark mode                                                         (o) On    |   yellow
  #################################################################################
```

## API

```cpp
enum class FrameShape { Row, Thumb, Tile, Field };

void drawFocusFrame(Painter&, Theme&, const Rect&, FrameShape);     // outline in `focus`
void drawFocusMarker(Painter&, Theme&, const Rect&);                // 4 px accent bar
void drawSelectionMark(Painter&, Theme&, const Rect&, std::string_view word); // check + word
void drawRestBorder(Painter&, Theme&, const Rect&);                 // high contrast, Roomy rows
Rect focusFootprint(const Rect& frame, FrameShape);                // repaint bounds incl. bleed
```

The caller owns cursor and scroll; nothing here stores state.

## Derived from / Used by

Derived from: Settings category and option rows, updater rows, Wi-Fi, Arcade settings, Controllers,
system-visibility, Games list, Arcade table, Scan rows, thumbs, tile focus, search field, hint frame,
on-screen keys (inventory C12 and the focus table; 8 focus looks).

Used by: *none yet.*
