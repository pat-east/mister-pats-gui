# Icon

A small picture of fixed size that is drawn in one color: the same icon takes the focus color, the muted
color or a warning tone. `Icon` is the base class; `BootstrapIcon` draws an icon of the Bootstrap Icons
set by name. Use an icon where a symbol everyone knows says more than a word (see the glossary below);
never as the only carrier of a function or a status.

Layer: primitive. Status: **implemented** (the class model, the SVG renderer, the full Bootstrap Icons set).

## Anatomy

```
Icon                         base class: width, height, draw(painter, x, y, color), drawIn(painter, box, color)
 +-- BootstrapIcon           BootstrapIcon("chevron-right", 128, 128): one icon of the set, by name

   BootstrapIcon("chevron-right", 128, 128)            BootstrapIcon("chevron-right", 24, 24)

        ##                                                  #
          ##                                                 #
            ##                                              #
          ##                                               #
        ##                                                #
   (rendered from the SVG at exactly this size, anti-aliased, in the color the caller passes)
```

```cpp
BootstrapIcon icon("chevron-right", 128, 128);          // rasterized once per name and size, then cached
icon.draw(painter, x, y, theme.palette.focus);           // top-left at x, y
icon.drawIn(painter, box, theme.palette.textMuted);      // centered in a box
auto shared = makeBootstrapIcon("house", 26, 26);        // a shared handle (IconPtr), for specs and lists
```

An unknown name draws nothing and `valid()` is false; it is never an error at draw time.

## How it is drawn: the SVG renderer

`src/ui-framework/core/Svg.h` renders a small SVG subset, independent of any renderer:

| Supported | Notes |
| --- | --- |
| `<svg viewBox>` | the view box is fitted into the requested width x height, aspect kept, centered |
| `<path d>` | every command, relative and absolute, including arcs; compact numbers and arc flags |
| `fill-rule` | `nonzero` (default) and `evenodd` |
| `<circle>`, `<rect>` | plain, without rounded corners |
| several shapes | laid over each other |

Not supported: strokes, gradients, transforms, text, clip paths. Bootstrap Icons use only filled paths, so
none of this is needed for them.

The renderer fills with supersampling (8 sub-scanlines per pixel, exact horizontal coverage) and produces a
**coverage mask**: an image whose alpha is the shape. The painter draws the mask in one color
(`Painter::drawMask`); a GPU renderer would upload it as a texture or draw the contours as vectors.

## The icon set

All **2078** Bootstrap Icons are available by name ("house", "chevron-right", "controller", ...). They live
as SVG files in `src/ui-framework/assets/bootstrap-icons/` (MIT, with the licence text), are embedded into the library at
build time by `src/ui-framework/tools/embed-icons.py` (about 1.3 MB, sorted table, binary search), and the licence text
is embedded with them. Adding another SVG of the subset is dropping the file into the folder.

Availability is not permission: **which symbols may appear without a text** is the glossary's decision,
not the set's.

## Glossary of self-explanatory symbols

A closed list, kept short on purpose (docs: accessibility rule 3.6). A symbol outside it needs a text next
to it.

```
D-pad cross, face and shoulder buttons (drawn in the user's layout)    not Bootstrap icons: the hint bar draws them
check (check-lg)    cross (x-lg)    arrow up (arrow-up)    alert (exclamation-circle)
clock (clock)       network (ethernet, wifi)     search (search)     scroll arrows
```

Status symbols (`IconId::Check`, `Cross`, `ArrowUp`, `Alert`, `Clock`, `Network`, `Wifi`) and the two page symbols `Search` ("search") and `Drive` ("hdd") are drawn through
`BootstrapIcon` with those names; they always come with a word (the page symbols with the headline under them: they
illustrate it and carry no meaning of their own).

## Variants and states

An icon has no states of its own; the caller chooses the color:

```
default            muted text color            e.g. a tab icon, inactive
active             primary text color          the active tab's icon
focus              the focus color             an icon in a focused element
tone               Done / Action / Warning / Error     status symbols, always with their word
disabled           Disabled tier               only with a second cue
```

Sizes come from the caller in pixels (design pixels through `Theme::px`): 20 in status lines, 24 to 26
in tabs and rows, 160 on an icon tile.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Status icon | 20, gap 10 to the word | `Status` |
| Tab icon | 26, gap 10 to the label | `Shell` |
| Nav item icon | 20, gap 10 to the text | `NavItem` |
| Icon tile | 160 in a 257 body | `GridItem`, muted color |

## Behavior

- Rasterized at construction through a cache keyed by name, width and height; drawing is a mask blend, no
  parsing and no allocation per frame beyond the construction of the icon object.
- A `BootstrapIcon` is cheap to keep: specs hold an `IconPtr`.
- No input, no focus.

## Accessibility

- A color change never carries meaning alone: status icons come with a word; the glossary limits symbols
  without text; the mask is crisp at 20 px and up.
- Contrast of the icon color against its surface follows the non-text rule (3 : 1), checked through the
  color the caller passes.
- The icon-only tab display is a user preference; it needs the names available elsewhere (the active
  tab's page title) so that no tab is identified by a picture alone.

## Strings

None. The name of an icon is an identifier, not text; the word next to a status icon is a catalog entry.

## Largest text, other languages and high contrast

Icons scale with the size the caller passes (the text-size setting scales the row, and the icon follows
the row's metric where the design says so); they do not change with the language. In high contrast the icon
takes the highlight or text color like any text.

## API

```cpp
class Icon {
public:
    int width() const; int height() const;
    virtual void draw(Painter &p, int x, int y, Color color) const = 0;
    void drawIn(Painter &p, const Rect &box, Color color) const;        // centered
};
using IconPtr = std::shared_ptr<const Icon>;

class BootstrapIcon : public Icon {
public:
    BootstrapIcon(const std::string &name, int width, int height);
    const std::string &name() const; bool valid() const;
    void draw(Painter &p, int x, int y, Color color) const override;
};
IconPtr makeBootstrapIcon(const std::string &name, int width, int height);

class SvgIcon {                                                          // src/ui-framework/core/Svg.h
public:
    bool parse(const std::string &svg);
    ImagePtr rasterize(int width, int height) const;                     // white, alpha = coverage
};
```

Other kinds derive from `Icon` later (an icon from an image, an icon from an SVG file on the device).

## Derived from / Used by

Derived from: the check and cross glyphs of the Arcade table, the `{DPAD}` cross, the tab icon bitmaps
and the status marks scattered over the screens (inventory, primitives), now one class and one set.

Used by: `Status` (status icons), `Shell` (tab icons, nav item icons), `GridItem` (icon tiles), the demo's
icon sheet.

Third-party: Bootstrap Icons, MIT, Copyright (c) 2019-2024 The Bootstrap Authors (see `NOTICE` and
`src/ui-framework/assets/bootstrap-icons/LICENSE`).
