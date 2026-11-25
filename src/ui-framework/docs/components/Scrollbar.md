# Scrollbar

A thin bar beside a scrolling list or text, plus a short position text, so the user always knows where
in the content they are. Use it on every list or text that can be longer than its area. Do not draw
arrow glyphs or "N more" texts instead.

Layer: primitive. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext M0.

## Anatomy

```
 +---------------------------------------------------------+   12 / 340      <- position text,
 | item                                                    |                     small, Faint,
 | item                                                    | |^|                 aligned at the list
 | item (cursor)                                           | |#|  <- thumb          header
 | item                                                    | |#|     focus color, min 24 high
 | item                                                    | | |
 | item                                                    | | |  <- track, 4 wide, surfaceHi
 +---------------------------------------------------------+ |v|
                                                      <-8->|  bar sits 8 px inside the list's
                                                              right edge
```

The bar is drawn only when the content is longer than the visible area (total > visible). When
everything fits, nothing is drawn, no position text either.

## Variants

Two kinds of position text, chosen by what the content is.

```
cursor list      12 / 340         the cursor is on item 12 of 340   (the user moves a cursor)
read-only text   12-21 of 340     lines 12 to 21 of 340 are visible (the user only scrolls)
```

(ASCII shows a hyphen; the UI draws an en dash in "12-21".) The caller places the text (usually at
the right end of the header row); the component only formats and draws it.

## States

```
top            |#|         thumb at the top of the track
               |#|
               | |
               | |
middle         | |
               |#|
               |#|
               | |
bottom         | |
               | |
               |#|
               |#|
tiny thumb     | |         very long content: thumb stays at its minimum height 24
               |#|
               | |
all fits       (nothing drawn)
high contrast  thumb in the highlight color on a track with a white 1 px edge
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Track width | 4 | radius 2, `surfaceHi` |
| Thumb | `focus` color, radius 2, minimum height 24 | height = visible / total of the track |
| Offset from the right edge | 8 | inside the list area |
| Position text | small, Faint tier | one line, right-aligned, never elided into a number |
| Shown when | total > visible | otherwise nothing |

Thumb position: `first / (total - visible)` of the free track length.

## Behavior

- No input. It reflects `{first, visible, total}` that the list owner already tracks (the cursor and
  scroll integers).
- Repaints only when the thumb position or the position text changes.
- Replaces: arrow glyphs at the top and bottom of text, "N more" texts, and having no cue at all.
- The position text for a cursor list is `n / total` (cursor item); for read-only content it is
  `a-b of n` (visible range).

## Accessibility

- A thin bar is a visual aid only; the position text is the second cue and carries the same fact in
  words, so the bar is never the only cue.
- Thumb against track is at least 3 : 1 (`focus` on `surfaceHi`, contrast registry pair); the thumb
  uses the `focus` token, which follows high contrast and the chosen highlight.
- The position text is Faint tier (at least 4.5 : 1) and small because it is secondary detail.
- Width 4 is decoration, not a target; nothing needs to be aimed at.

## Largest text and high contrast

Largest text shrinks the number of visible rows, so the thumb grows shorter and the position text
widens; both stay on one line. In high contrast the thumb takes the highlight color and the track
gets a white 1 px edge (not for White).

```
Largest, 340 items, 6 visible rows:   14 / 340     bar thumb at its minimum height 24
```

## API

```cpp
struct ScrollInfo { size_t first, visible, total; };
void drawScrollbar(Painter&, Theme&, const Rect& list, const ScrollInfo&);   // no-op if it fits
void formatPosition(char (&out)[48], size_t cursor, size_t total);          // "12 / 340"
void formatRange(char (&out)[48], size_t first, size_t last, size_t total); // "12-21 of 340"
```

Strings are written into a caller's stack buffer; nothing allocates per frame.

## Derived from / Used by

Derived from: the core-updater Review bar (4 wide, `focus` thumb, 8 from the edge, "N more"), the
Scan progress bar (4 wide, `accent` thumb, flush right, "a-b of n"), the Changelog and release-notes
arrow glyphs, the Games header position text ("n / m"). Replaces the missing cue on the updater
list, Wi-Fi, time zone, system visibility, Games list, Arcade and Settings option lists.

Used by: *none yet.*
