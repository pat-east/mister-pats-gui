# Table

A list whose rows have several columns of short values: a header row, weighted columns, text and
flag cells. Use it when the user compares the same attributes across many rows. Do not use it for
a list of choices (see [ListItem](ListItem.md)) or for label/value pairs of one object (see
`KeyValueList`).

Layer: container. Status: **implemented in the wireframes.** Values marked (M0) wait for the owner's decision.

## Anatomy

```
 +-----------------------------------------------------------------------------------------+
 | Name                    Year   Maker             Works      Core       Files            |  header
 +-----------------------------------------------------------------------------------------+
 4 below                                                                                       ^ 36
 +-----------------------------------------------------------------------------------------+
 |#| Alpha Quest            1991   Acme Games         (v)        (x)       alpha.zip        |  focus
 +-----------------------------------------------------------------------------------------+
 |   Beta Run               1992   Foo Works          (v)        (v)       beta.zip         |  zebra
 +-----------------------------------------------------------------------------------------+
 |   Gamma Ray              1993   Bar Studio         (x)        (x)       gamma.zip        |
 +-----------------------------------------------------------------------------------------+
   ^ weight 30     ^8     ^22              ^10        ^10        ^20  (weights add up to 100)
```

- Header: bold, small, `Muted`, sentence case, 36 high, radius 8, faint fill. Header text sits 10 px
  from the column's left edge.
- Rows use the shared row chrome in **Table density** (see `ListItem`): marker and outline on the
  cursor row, zebra as the only resting fill.
- `(v)` and `(x)` stand for the check and the cross glyph; the real glyphs are drawn lines.

## Variants

Two **cell kinds**:

```
Text   left-aligned, elided with an ellipsis at the column's right edge (16 px reserved)
Flag   a glyph centered in the column: check (Done tone) or cross (Warning tone)
```

The two flag glyphs differ in **shape** (a check mark and an X), not only in color; a column header
names what the flag means ("Works"). Columns declare their kind and weight; weights are relative and
add up to 100.

## States

```
normal row      resting fill only on odd rows (zebra, Faint surface)
cursor row      surfaceHi fill, marker 4, outline 2
zebra           odd rows: surface at low alpha (M0), even rows: none
header          never focusable
loading         header shown, body replaced by ProgressPanel (calm: one line)
empty           header hidden, EmptyState with why and next action
filtered empty  header shown, body replaced by EmptyState ("No rows match this filter.")
```

```
 filtered empty:
 | Name                    Year   Maker             Works      Core       Files            |
                    No games match this filter.
                    Press Y to change the filter.
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Header row | 36, radius 8 (was 4) (M0) | text bold small `Muted` |
| Row pitch | 38 = frame 34 + gap 4 (M0) | text at body size if the columns fit (M0; was small) |
| Radius | `Radius.row` 8 (was 6) | marker 4 (was 3), radius 2 |
| Cell padding | 10 left, 16 right reserve | per column |
| Zebra | odd rows, `surface` low alpha | only resting fill in a table |
| Flag glyph | about 60 % of the row frame height | centered, stroke at least 2 px |
| Scroll | as `List` | scroll bar + position text |

The caller supplies: columns (title, weight, kind), rows of cell values, the flag meaning per
column. The component decides geometry, fills, elision, glyph drawing.

## Behavior

- Up/Down, jump-to-letter, paging, reset and position text are exactly those of `List`; a Table is
  a List whose rows draw columns.
- Confirm acts on the row (the screen decides). Left/Right: the screen's; the Table has no
  column focus.
- Elision is per column; a cell never overlaps its neighbor.
- Hints contributed: as `List`, plus the screen's filter hint (for example `Y Change filter`).
- Repaint: old and new cursor row; header only when its text changes.

## Accessibility

- Flags carry shape plus the column header; color alone never says "works" (rule 1.2). The check
  uses the Done tone and the cross the Warning tone, both against the row surface at 3 : 1.
- Data cells are body size where the columns fit; small only if the owner decides a diagnostic
  table may use it (M0), never for the column that decides the action.
- Rows are 34 px frames, below the usual 44 px target; this is allowed because a Table row is a
  dense read-and-pick row with a single action (M0: confirm or use Dense density).
- At Largest text columns elide; the Name column must stay readable (check in the gallery).

## Largest text and high contrast

```
Largest: pitch grows, fewer rows, cells elide

 | Name                 Year   Maker           Works    Core                          |
 |#| Alpha Qu...        1991   Acme Ga...        (v)      (x)                         |
```

High contrast: marker and outline take the highlight color; zebra stays; flag glyphs keep their
shape and add a text-free second cue by shape only (check versus cross).

## API

```cpp
enum class CellKind { Text, Flag };
struct Column { std::string_view title; int weight; CellKind kind; };
struct TableConfig { const Column *columns; int columnCount; int rowCount; Rect area; };
struct TableCell { std::string_view text; bool flag = false; };     // flag: true = check

void drawTableHeader(Painter &, Theme &, const Rect &, const TableConfig &);
void drawTableRow(Painter &, Theme &, const Rect &, const TableConfig &, int row,
                  const TableCell *cells, bool cursor, bool paneActive);
```

State (cursor, scroll) is a `ListView`, owned by the screen.

## Derived from / Used by

Derived from: the Arcade game-status table (weighted columns, flag cells, zebra, header row),
and, for the row look, the Scan progress rows (38 / 2). Survey: inventory C9b.

Used by: *none yet.*

## Implementation

`drawTable` in src/ui-framework/kit/Data.cpp: weighted columns, header, text and flag cells, zebra, cursor row. Scrolling (`first`) is a parameter; filtered-empty and loading bodies are not built yet. Shown in the Data tab.
