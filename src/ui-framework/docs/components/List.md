# List

A vertical list of [ListItem](ListItem.md) rows with one cursor, persistent scrolling, a position cue
and a scroll bar. Use it for every list the user moves through with Up/Down. Do not use it for
tiles (see `Grid`), for columns of values (see `Table`) or for read-only lines (see `Result`).

Layer: container. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext
milestone M0.

## Anatomy

Default: Roomy density, 10 of 340 items visible, cursor on the third row.

```
  Section title (optional)                                                          3 / 340
 +-----------------------------------------------------------------------------------+  ^
 |   Item one                                                                        |  |
 +-----------------------------------------------------------------------------------+  |
 +-----------------------------------------------------------------------------------+  |
 |   Item two                                                                        |  |  "
 +-----------------------------------------------------------------------------------+  |  scroll
 +-----------------------------------------------------------------------------------+  |  bar
 |#| Item three                                                                      |  |  (4 px)
 +-----------------------------------------------------------------------------------+  |
 ...                                                                                     v
  ^ position text, small, Faint, right-aligned in the header line
                                                          scroll bar: 4 px, 8 from edge ^
```

- The List owns the area, the rows' vertical placement, the scroll bar and the position text. The
  rows are drawn by `ListItem`.
- The header line (title left, position right) is optional; without it the position text is placed
  by the caller (see Variants).

## Variants

By **horizontal behavior** (what Left/Right do), chosen by the screen:

```
HNone    Left/Right do nothing in the list                      short lists, filters elsewhere
HPage    Left/Right page by (visible rows - 1)                  long single-column lists
HOwner   Left/Right belong to the screen (pane, filter, value)  settings panes, tabbed lists
```

By **position text** (what the cue says):

```
cursor list      3 / 340            the cursor is the position; n of total
read-only scroll 12-21 of 340       no cursor; a range of what is visible (logs, long text)
```

By **content**: plain, with `Heading` rows (kept with their first item when scrolling), or with
jump-to-letter enabled (sorted lists longer than two pages).

## States

```
top            first row visible, cursor 1 / n        scroll bar thumb at the top
middle         cursor anywhere in the window          cursor moves, window stays
bottom         last row visible, cursor n / n         Down does nothing (no wrap)
short list     all rows fit                           no scroll bar, position text optional
empty          zero items                             EmptyState instead of the list
pane inactive  remembered cursor, pane has no focus   faint fill, no marker (see ListItem)
```

Cursor at the top edge and Up pressed: the List reports "left the top edge" so a screen can move
focus to a header or a search field. It never wraps.

Short list (everything fits, scroll bar and position text hidden):

```
 +-----------------------------------------------------------------------------------+
 |#| Only item                                                                       |
 +-----------------------------------------------------------------------------------+
 +-----------------------------------------------------------------------------------+
 |   Second item                                                                     |
 +-----------------------------------------------------------------------------------+
```

## Measurements

Design pixels at 1080p; tokens once `Tokens.h` exists.

| Item | Value | Notes |
| --- | --- | --- |
| Row pitch | Roomy 64, Dense 52, Table 38 (M0) | frame + gap; from `ListItem` / `Table` |
| Two-line row | frame `max(62, text + 16)`, gap 6 | grows with Largest text |
| Heading row | 36 | not focusable |
| Visible rows | `max(1, area.h / pitch)` | with content-based pitch at Largest |
| Scroll bar | 4 wide, thumb `focus`, track `surfaceHi`, 8 from the right edge, min thumb 24 | only if `count > visible` |
| Position text | small, `Faint`, header line right | `n / total` or `a-b of n` |
| Scroll model | persistent minimal scroll | below |

The screen owns two integers, `cursor` and `scroll`. The List derives everything else. Minimal
scroll: if `cursor < scroll` then `scroll = cursor`; if `cursor >= scroll + visible` then
`scroll = cursor - visible + 1`. Moving up after scrolling down does not scroll until the cursor
reaches the top edge of the window.

## Behavior

- **Up/Down** move one item, skip `Heading` rows, clamp at both ends (no wrap).
- **Left/Right** follow the variant: HNone ignores, HPage pages, HOwner passes the action to the
  screen. A screen that already uses Left/Right for panes or filters must pick HOwner.
- **Confirm/Back** are returned to the screen as events (`Confirm`, `Back`); the List does not
  interpret them. Confirm on an item is the item's business (see `ListItem`).
- **Jump to letter** (shoulder buttons) only on sorted lists longer than two pages. The cursor
  moves to the first item of the next or previous initial letter and that item becomes the top row.
  One toast through `Toast`, "Items starting with X", from one shared helper.
- **Switching** to a different list or category puts the cursor on its first item; **returning**
  with Left or Back keeps the remembered position.
- **Repaint:** with instant focus, a cursor move repaints exactly two rows (old and new cursor row,
  plus 3 px bleed). A change of scroll, count, density or area repaints the page. No per-row
  state, no per-frame allocation.
- **Hints** the list contributes: `{DPAD} Move` or `UP/DOWN Move`, `A <action of the focused kind>`,
  and, when enabled, `L2/R2 Jump to letter` or `LEFT/RIGHT Page`.

## Accessibility

- A list longer than one page always shows where the user is: the scroll bar and the position
  text (rule 2.6). The thumb (`focus` on `surfaceHi`) meets 3 : 1.
- Every item is reachable: a container test walks Down from the first to the last item and checks
  that each becomes visible (rule 2.3). No row stays beyond the area without scrolling.
- Rows are at least about 44 px high (rule 1.4); the pitch grows at Largest text and the visible
  count falls, nothing overlaps.
- Focus and selection stay separate (see `ListItem`, `FocusFrame`).
- The cursor is never hidden by the scroll; the minimal scroll keeps it inside the window.

## Largest text and high contrast

```
Largest (130 %): rows are taller, fewer fit, position text and bar stay readable

  Section title                                                                    3 / 340
 +--------------------------------------------------------------------------------------+
 |   Item one that is long and gets cut with an ell...                                  |
 +--------------------------------------------------------------------------------------+
 +--------------------------------------------------------------------------------------+
 |#| Item two                                                                           |
 +--------------------------------------------------------------------------------------+
```

High contrast: the scroll thumb takes the highlight color through `focus`; Roomy rows get the
white border (see `ListItem`); the position text stays `Faint` or brighter.

## API

```cpp
struct ListView   { int cursor = 0, scroll = 0; };                 // owned by the screen
struct ListConfig {
    int count, pitch; Rect area;
    enum { HNone, HPage, HOwner } horizontal = HNone;
    bool jumpToLetter = false;
    const std::string &(*nameAt)(int index, void *ctx) = nullptr; void *ctx = nullptr;
};
enum class ListEvent { None, Moved, LeftTop, Confirm, Back };

ListEvent listHandle(ListView &, const ListConfig &, Action);      // Up/Down/page/jump
void      listClamp(ListView &, const ListConfig &);               // persistent minimal scroll
int       listVisible(const ListConfig &);
void      drawScrollbar(Painter &, Theme &, const Rect &track, int count, int visible, int scroll);
void      formatPosition(char (&out)[48], int cursor, int count);  // "3 / 340"
void      formatRange(char (&out)[48], int first, int last, int count);  // "12-21 of 340"
bool      listRowsDirty(const ListPaint &, const ListView &, int count, int &a, int &b);
```

State (cursor, scroll) is owned by the screen; strings are views and buffers sit on the stack.

## Derived from / Used by

Derived from: the persistent scroll clamps of the system-visibility, Games list, Arcade, Wi-Fi and
time-zone lists; Settings options (stateless, pins the cursor to the bottom edge, replaced);
updater list (line-based with headings); Controllers list (no scroll, replaced); Scan progress
list (range text and bar). Survey: inventory C9; row/list consolidation report.

Used by: *none yet.*

## Naming (owner, 2026-11-22)

**List** is the real list: a column of equal rows with a cursor and a scroll position, used for controls
(Settings, the MRA files, dialogs, wizards). The list shown in the main views next to the grid modes is the
third grid mode, **Grid-List** (see `Grid.md`); it is built from this component with content rows (`Entry`
kind of `ListItem`, dense) and a `DetailPanel`. Implemented: `ListLayout` (visible rows, persistent minimal
scroll, page step, no wrap) and `drawScrollbar`.
