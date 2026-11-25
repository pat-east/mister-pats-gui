# PageHeader

The title row at the top of a tab's content window: the title at the left, an optional search field in
the middle of the screen and optional information at the right. Every tab with a title uses it, so the
title, the field and the info are in the same place on every tab. Do not use it for a dialog title (that
is `Modal`).

Layer: container. Status: **implemented** (`src/ui-framework/kit/PageHeader.h`, shown on the Grids tab).

## Anatomy

```
 +----------------------------------------------------------------------------------------+
 | **Grid views**          +--------------------------------+            ~Grid . 3 of 42~  |  row 60
 |                         | Search items                   |                              |
 |                         +--------------------------------+                              |
 |                                                                                         |  gap 12
 | body ...                                                                                |
   ^ title: Heading, bold,    ^ field: 520 x 52, centered on        ^ info: body, Faint,
     Primary, left, elided      the SCREEN, never moves                right-aligned, elided
```

The layout is fixed. The field is centered on the screen (not on the content window), is always 520 x
52 design pixels and sits vertically in the 60 px row; the title takes the space to its left, the info
the space to its right. The body starts `pageHeaderHeight` (72) below the area top: the row plus a 12 px
gap, so a focus ring on the first body row never touches the field.

## Variants

```
title only                  title + info                  title + field + info
**Consoles**                **Systems**   ~6 consoles~    **Games**   [ Search games ]   ~Grid . 3 of 42~

empty title: the field is still centered
                            [ Search games ]                                ~12 results~
```

- Without a field the info is elided first (down to a third of the row), then the title.
- With a field the title and the info each get what is left of their side; both elide.
- The info is a count, a position or a short state (view name). It never carries instructions (those
  are hints).

## States

```
field idle, empty           focused (cursor on it)        with a value
| Search items |            +##############+             | sample |
                            # Search items #
                            +##############+
```

- Focus is the standard outline in `focus` (the same call as every target) on top of the fill.
- The field shows the placeholder (`Faint`) or the value (`Primary`). No key hints inside it: the
  keys are in the help bar. A tab opens the `TextEntryDialog` from the focused field.
- Typing happens in the dialog, never inline, so the keyboard flow is identical on every tab.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Row | 60 | title, field and info are centered in it |
| Header height | 72 | row plus a 12 gap to the body |
| Title | `Heading`, bold, `Primary`, at the area's left edge | elided |
| Info | `Body`, `Faint`, right edge of the area | elided |
| Search field | 520 x 52, centered on the screen | `TextField` look: `surfaceHi`, `Radius.panel`, 16 inset |
| Side gap | 24 | between the field and the title or the info |

Sizes are design pixels (scaled by the theme). The field's size does not follow the text size: at 130 %
the placeholder still fits, and a long value elides.

## Behavior

- **Up** from the first body row focuses the field; **Down** returns to the body; **A** opens the
  `TextEntryDialog`; **B** leaves the field (a tab may also use B to clear a non-empty query).
- The tab owns the query and the focus flag; the header draws them.

## Accessibility

- The title names where the user is; the info says how much there is and where they are in it.
- Info is `Faint` text from the theme (the contrast registry checks it) and is never the only carrier
  of essential information.
- The field's focus is the outline plus the surface, not hue alone.
- The placeholder says what the field searches ("Search games"), not just "Search".
- Largest text: the row does not grow; title and info elide. Pseudo-locale: strings wrap in markers and
  elide, the field stays put.
- High contrast: the field has the white 2 px border (from `TextField`) and the focus outline uses the
  highlight color.

## API

```cpp
struct SearchFieldSpec { Text placeholder; std::string value; bool focused = false; };
struct PageHeaderSpec {
    Text title;                 // may be empty
    Text info;                  // may be empty
    bool hasSearch = false;
    SearchFieldSpec search;
};
namespace PageHeaderMetrics { rowHeight = 60, height = 72, fieldWidth = 520, fieldHeight = 52, sideGap = 24 }

int  pageHeaderHeight(const Theme&);                                        // 72, scaled
Rect pageHeaderSearchRect(const Theme&, const Rect &area);                  // for focus rings, jump targets
Rect drawPageHeader(Painter&, const Theme&, const Rect &area, const PageHeaderSpec&);  // returns the body rect
```

`pageHeaderSearchRect` is valid even when the header shows no field. Strings are `Text`: catalog keys
for words, `Text::data` for counts and names.

## Derived from / Used by

Derived from: the title row and search layout copied in Home, Games, Consoles, System visibility, Arcade,
Settings and others (inventory C4, C4b). Fixes the owner's feedback that the field sat on the left on
one tab and in the middle on another.

Used by: the demo's Grids tab.
