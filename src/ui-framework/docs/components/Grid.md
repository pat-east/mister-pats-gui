# Grid

Rows and columns of `GridItem`, with one navigation, layout and scroll rule, plus the one-row `TileRow`
(horizontal, scrolling). Use it for every tile view. Do not use it for lists of rows (see `List`).

Layer: container. Status: **proposal.** Values marked (M0) wait for the owner's decision.

## Anatomy

A grid of 6 columns, 2 visible rows, content 1776 wide.

```
 content area (margins 72)
 <------------------------------------ 1776 ------------------------------------>
 +----+  26  +----+  26  +----+  26  +----+  26  +----+  26  +----+  .. slack ..       ^
 |cell|      |cell|      |cell|      |cell|      |cell|      |cell|                    |
 |    |      |    |      |    |      |    |      |    |      |    |                    | row
 +----+      +----+      +----+      +----+      +----+      +----+                    | step =
  caption     caption     caption     caption     caption     caption                  | cell + caption
                                                                                       | + gap 26
 +----+      +----+      +----+      +----+      +----+      +----+                    v
 |    |      |    |      |    |      |    |      |    |      |    |
 +----+      +----+      +----+      +----+      +----+      +----+
  caption ...
 (peek) the next row shows partly at the bottom edge when more rows exist
```

- **Cell rule:** `cellW = art + 12`, fixed; gap `Space.gap` 26. Columns = `(width + gap) / (cellW + gap)`.
  Left-aligned; the slack (a few px) stays at the right edge. Cells are never stretched to fill (stretching
  is what scales box art by 5 to 7 % today).
- **Rows visible:** `(height + gap) / (rowStep)`; one more row is allowed to show partly (peek).

## Variants

```
Grid (row snapping)                 TileRow (one row, horizontal)
rows scroll by whole rows           +----+ +----+ +----+ +----+ +--
                                    |    | |    | |    | |    | | 1 peek tile cut by the clip
Grid with pixel origin (Home)       +----+ +----+ +----+ +----+ +--
sections stack; the page scrolls    perPage = (width + gap) / (cell + gap)
by pixels as one unit
```

- **Grid:** vertical, snaps by row. Used by Systems, Games, Favorites and Home (also the small-art view). The application layer wraps it in `src/app/GridPage` (one page for every tab: clip and peek row, focused tile last, small grid footer, the one-row strip), see `src/app/README.md`.
- **Grid, pixel origin:** the same layout and navigation, but the owner supplies a vertical pixel offset,
  so several sections can scroll as one page (Home favorites under a TileRow).
- **TileRow:** one row, scrolls sideways, one extra tile drawn for the peek, clipped to the strip.
  Shares the scroll-follow code with Grid (it exists twice in Home today).

## States

```
normal                       focus moves instantly         short last row
+----+ +----+ +----+        +----+ +####+ +----+           +----+ +----+ +----+
|    | |    | |    |        |    | #    # |    |           |    | |    | |    |
+----+ +----+ +----+        +----+ +####+ +----+           +----+ +----+
                                                            Down from the row above lands on the
empty (no items)             loading                        LAST item when the column has no tile
 +------------------+         placeholders in the cells,
 |   EmptyState     |         art reveals one by one
 +------------------+
```

- An empty grid draws nothing; the screen supplies an `EmptyState` (why, and what to do next).
- Loading shows `Placeholder` tiles; art arrives asynchronously and reveals once.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Gap | `Space.gap` 26 | between cells, both directions |
| Row step | cell height + caption + gap | caption 0 / 26 / 46, metric at larger text (M0) |
| Content width | 1776 | screen margins 72 |
| Focus bleed | 3 | drawn inside the 26 gap; one policy for every grid (M0) |
| Left inset | 3 | only the bleed; today 9, 15, 10 and 18 on different screens |
| Peek | one partly visible row or tile | never a full extra one |

Spec the caller supplies: art size, caption mode, area, count. The grid decides columns, rows, scroll
and cell rects.

## Behavior

- **Left/Right** move one tile; they never wrap and never page. At column 0 Left does nothing (the
  container may use it to leave to a sibling pane).
- **Up/Down** move one row. Down from a row whose column is empty jumps to the last item. Up from the
  top row reports `leftTop` so the owner can move focus to a search field above.
- **Scroll:** `ensureVisible` keeps the focused row fully visible; one partly visible next row is the peek.
  With a pixel origin the owner scrolls by the same rule in pixels.
- **Jump to letter** (shoulder buttons, sorted collections only): the cursor moves to the first item of the
  letter and that row is put at the top. Not offered on time-ordered strips.
- Hints from the screen: `{DPAD} Move`, plus actions the screen adds. Cursor and scroll are the owner's
  two integers.

## Repaint

The framework wraps the existing repaint machinery; it does not add per-frame cost.

```
state kept: cursor, scroll, count, columns, cell size, image generation,
            the visible range's art pointers (reused storage, no allocation per frame)
cursor moved          -> dirty = footprint(old tile) U footprint(new tile)         (exactly two)
art arrived           -> dirty = footprint(that tile)
scroll/count/columns  -> repaint the whole grid area
fullRedraw            -> repaint everything
```

- Focus is instant, so a focus change never needs more than two tiles.
- Image requests happen before the dirty decision, every frame (idle screens still need them).
- Tiles are drawn in two passes: all except the cursor, then the cursor (outline on top).
- A test for the tile footprint (focus bleed 3) sits next to `canvas_damage_test`.

## Accessibility

- Exactly one focused tile is always visible; scrolling keeps it on screen.
- Movement is predictable (no wrap, no hidden paging); the position is always readable from the
  screen: a position cue ("n of total") is shown by the owner in the page header for long collections.
- Cells are at least 140 wide; text in captions elides, never overlaps.
- Layout never depends on color; the focus rule is `GridItem`'s.

## Largest text and high contrast

```
Largest: cell and gap are unchanged, the caption grows, so rows get taller and fewer rows fit

 normal: 2 rows + peek                  Largest: 2 rows, the peek row shrinks or disappears
 +----+ +----+                          +----+ +----+
 |    | |    |                          |    | |    |
 +----+ +----+                          +----+ +----+
  Title  Title                           A long title  Another long...
  ~Sub~  ~Sub~                           ~Sub~         ~Sub~
```

High contrast changes only the focus color (GridItem).

## API

```cpp
struct GridSpec { int artW, artH; Caption caption; };       // design px

class Grid {
public:
    void configure(const Rect& area, Theme&, const GridSpec&);
    Rect cell(int index) const;                              // includes scroll and origin
    struct Move { int index; bool leftTop; };
    Move move(int cursor, int count, int dx, int dy) const;
    void ensureVisible(int cursor, int count);               // row snapping
    void setOriginY(int px);                                 // pixel origin (page scroll)
    int firstVisible() const, lastVisible() const;           // peek included
};
class TileRow {                                              // one row, horizontal
public:
    void configure(const Rect& strip, Theme&, const GridSpec&);
    Rect cell(int index) const;
    void ensureVisible(int cursor, int count);
};
class RepaintTracker {                                       // value type, reused storage
public:
    bool begin(bool fullRedraw, int cursor, int scroll, size_t count, uint64_t imageGen);
    void touch(int index, const Rect& footprint);
    Rect dirty() const; bool full() const;
    template <class F> void passes(int first, int last, int cursor, F&& draw);
    void commit();
};
```

## Derived from / Used by

Derived from: `GridView` (Systems, Games), the Home rows and favorites grid (a third, independent
implementation), the painted-state tracking copied in three screens (inventory C8, C8b; tile report).

Used by: *none yet.*

## Naming: the grid views (owner, 2026-11-22)

The browse views of the main screens form one family, **Grid**, in three modes:

| Mode | What it is | Component |
| --- | --- | --- |
| **Grid** | tiles with a two-line caption (art 245) | `Grid` + `GridItem` |
| **Grid-Small** | small tiles without a caption (art 128), many per screen | `Grid` + `GridItem`, caption `None` |
| **Grid-List** | a list of entries on the left, the detail of the focused entry on the right | `GridList`: `List` with `Entry` rows + `DetailPanel` |

"List" alone always means the real list of controls (Settings, dialogs, wizards); the list inside the
Grid-List is a list of content entries (`Entry` rows). A key (Y in the examples) cycles through the three
modes; the cursor is kept. In the demo the three modes are on the "Grids" tab.
