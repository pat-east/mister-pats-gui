# GridItem

One tile in a grid or tile row: prepared art (or an icon, or a placeholder), an optional caption and an
optional mark. Use it for every tile. Do not use it for list rows (see `ListItem`).

Layer: item. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext milestone M0.

## Anatomy

Default variant `Cover`, caption mode `TitleSub`, focused.

```
        cell width = art + 12 (fixed, never stretched)
        <------------- 257 ------------->            example: a grid whose prepared art is 245
        +-------------------------------+  ^
        |###############################|  |
        |#  +-----------------------+ #|  |  body = art + 12 (art inset 6 on every side)
        |#  |                  (*)  |  #|  |  focus outline 3 px, `focus`, radius 10, drawn
        |#  |     prepared art      |  #|  |  centered on the body edge (bleed 3 px)
        |#  |    native size 245    |  #|  |
        |#  +-----------------------+ #|  v
        |###############################|
        +-------------------------------+
         Game title that elides if long       <- label: 26 high, body size
         ~Second line~                        <- sublabel: 20 high, small, Faint
```

- `(*)` is the mark slot: top-right of the art, 8 from the edge, backdrop circle 30 so the mark is
  readable on any picture (see `FavoriteMark`).
- The art is drawn at its prepared size, centered, **never scaled** (DESIGN.md "Box art is
  fixed-size"). Cell and gap come from the art size, not the other way round.
- Focus never grows the tile, adds no shadow, and never changes the art size.

## Variants

```
Cover          Icon                    Placeholder (name inside)     Placeholder (compact)
+---------+    +---------+             +---------+                   +-------+
|  art    |    | [icon]  |  black      |  Game   |  gradient         |  [ ]  |   no caption
|  native |    | scaled  |  backdrop   |  title  |  + neutral mark   |       |
+---------+    +---------+             +---------+                   +-------+
 Title          Title                   (name is the caption)
 ~Sub~          ~Sub~                   
```

- **Cover:** game box art in a fixed prepared size. Focus is the 3 px outline only.
- **Icon:** a system icon on a black backdrop, kept opaque. The icon is the one thing allowed to be
  scaled to the tile (it is not box art). Same cell rule, same outline, same caption table.
- **Placeholder:** shown while art is missing or loading. `name inside` draws the title in bold
  inside the tile (size steps down until it fits, elided); `compact` is for the small-art grid.
- The old "non-cover tile grows 10 %" branch is removed: no caller uses it (DESIGN.md row to be
  deleted with the extraction).

## Caption modes

Chosen per grid, not per tile, so rows stay aligned.

| Mode | Reserved below the body | Label rect | Sublabel rect |
| --- | --- | --- | --- |
| `None` | 0 | - | - |
| `Title` | 26 | {y, 26} | - |
| `TitleSub` | 26 + 20 = 46 | {y, 26} | {y + 26, 20} |

Drawn offsets equal reserved heights (today the label box is 4 + 24 and the sublabel starts 2 below
it, so the text overshoots the 46 by about 4 px). Both lines elide with an ellipsis; the sublabel is
never allowed to overflow.

## States

```
normal                       focused                         focused + mark
+------------+               +############+                  +############+
|  art       |               #  art       #                  #  art  (*) #
+------------+               +############+                  +############+
 Title (Muted)                Title (Primary)                  Title (Primary)

loading (placeholder)        art arrives (reveal)            unavailable ("No core")
+------------+               +------------+                  +------------+
| ~~~~~~~~~~ |               | art fades  |                  | [No core]  |  badge, top-left
|  [ ]       |               | in, 1 time |                  |   icon     |  Neutral tone
+------------+               +------------+                  +------------+
```

- Focus is instant (same frame): the outline leaves the old tile and appears on the new one.
- Label is `Primary` when focused and `Muted` otherwise; sublabel `Faint`.
- Art arrival uses the existing reveal path (`drawImageReveal`); nothing else animates.
- Unfocused art alpha today is 215 (dimming): proposal to draw art opaque always (M0), calmer and
  cheaper.
- "No core" is a `Badge` on the art, not a sublabel, so the tile shape and caption stay equal.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Cell width | prepared art + 12 | Home 190 (art 178), grid 257 (245), small 140 (128), arcade 160 (148) |
| Art inset | 6 | art centered, native size |
| Body radius | `Radius.panel` 10 | art corner radius `Radius.chip` 5 (M0; 4 today) |
| Focus | 3 px, `focus`, bleed 3 | one value for every grid (M0); replaces `lerp(accent, focus, .65)` |
| Caption | 26 + 20 | `None` / `Title` / `TitleSub` |
| Mark | 30 circle, 8 from top-right | `FavoriteMark` with backdrop alpha 190 |
| Badge | top-left, 8 from the edge | `Badge`, chip radius |
| Footprint | frame inset by -3 | focus bleed only; no shadow, no growth |

The caller supplies: variant, art (an image reference), label, sublabel, mark, badge, caption mode
(via the grid). The component decides geometry, colors, high-contrast treatment and elision.

## Behavior

- A tile has no input of its own: the grid owns the cursor.
- Hints come from the screen: the tile contributes nothing.
- The focused tile is drawn last so its outline is never covered by a neighbour's pixels.

## Accessibility

- The outline (3 px, `focus`) is at least 3 : 1 against the tile and its surroundings and does not
  depend on hue. In high contrast it takes the chosen highlight color.
- Selection of a tile (for example a favorite) is the mark shape, not color.
- Caption text: `Primary` / `Muted` on `background` pass 4.5 : 1; the `Faint` sublabel stays at
  alpha 190 or more.
- Minimum cell is 140 wide: well above the 44 px target size.
- A tile is `{role: tile, label, sublabel, state: focused/marked}` through `Describable`.

## Largest text and high contrast

```
Largest (130 %): art and cell do not change; the caption block grows (box art never resizes)

 normal caption (46)              Largest caption (about 60: 34 + 28 line boxes, elided)
 Title                            A long game title that is cut off with an ell...
 ~Sub~                            ~System name~
```

Grid rows get taller by the same amount (caption is a metric, not a fixed literal, at larger sizes
(M0)). In high contrast the outline uses the highlight color; tiles get no resting border (a white box
on every tile adds noise; the outline marks the cursor).

## API

```cpp
enum class GridItemKind { Cover, Icon, Placeholder, PlaceholderCompact };
enum class Caption { None, Title, TitleSub };

struct GridItemSpec {
    GridItemKind kind;
    ImageRef art;                    // framework interface, not a cache type
    std::string_view label, sublabel;
    bool mark = false;               // drawn as FavoriteMark(onArt)
    std::string_view badge;          // e.g. "No core"
    bool revealing = false;
};
void drawGridItem(Painter&, Theme&, const Rect& cell, Caption, const GridItemSpec&, bool focused);
Rect gridItemFootprint(const Rect& cell);                // frame inset(-3), for dirty rects
int  captionHeight(Theme&, Caption);                     // 0 / 26 / 46, metric at larger text
```

## Derived from / Used by

Derived from: `Tile` (cover, icon, placeholder, compact), Systems icon tiles, Home and Games tiles
(inventory C11; tile consolidation report: caption arithmetic, unused growth branch, focus color).

Used by: *none yet.*
