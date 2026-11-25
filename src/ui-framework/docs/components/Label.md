# Label

One line of text in a role (size and weight) and a tier (how strong it is). Use it for every single
line of text: titles, row labels, values, captions. Do not use it for paragraphs (see `TextBlock`), and
do not shrink text to make it fit, except in the bottom hint bar.

Layer: primitive. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext M0.

## Anatomy

```
 +---------------------------------------------------------+
 |  Select a network                         <- role: body, tier: Primary, align: left     |
 +---------------------------------------------------------+
   ^ the caller gives a rect; the label draws one line inside it, elided with an ellipsis at the
     right edge, vertically centered on the font's line height
```

## Variants

Three choices, all made by the caller.

```
role     Title(48)  Heading(36)  Clock(33)  Tab(27)  Body(21)  Small(17)  Hint (own setting)
weight   regular | bold
align    left | right | center
```

```
Heading, bold, Primary         Settings
Body, regular, Primary         Dark mode
Body, bold, Primary            Update available
Small, regular, Muted          Secondary detail such as a path
Body, regular, align right                                                       Larger
Body, regular, align center                      Centered text
```

## States: text tiers

Four tiers replace about twelve alpha literals (80, 110, 120, 150, 155, 160, 170, 180, 200, 220, 230).
They are theme colors with a fixed value per mode and color theme, not runtime alpha.

```
Primary    Everything the user reads to decide             textPrimary
Muted      Secondary text, inactive labels                 textMuted
Faint      Detail and counters; the floor for any text     muted at 190 alpha equivalent (M0)
           the user must read
Disabled   Not allowed right now; only with a second       about 120 alpha equivalent (M0)
           cue (a word or reason), never as the only cue
```

Estimated contrast on the dark background (to be measured by the contrast registry, not final): Muted
about 7 : 1, Faint about 4.6 : 1, Disabled about 2.6 : 1 (below the 4.5 : 1 text rule, so Disabled is
allowed only on inert text that also has a second cue and a reason; see Accessibility).

## Elision

```
fits           Select a network
too long       Select a network that has a very long na...        <- ellipsis, never mid-glyph
Largest        Select a network that has a ve...                  <- same rule, narrower
```

The UI draws one ellipsis glyph; the ASCII shows three dots. The label never shrinks its font to fit
(no `drawFittedLine`). The one exception is the hint bar, which steps down to `Small` before it drops
hints (see `Hints`).

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Sizes | Title 48, Heading 36, Clock 33, Tab 27, Body 21, Small 17 | design px, scaled by the text-size setting |
| Hint size | own setting: 100 / 115 / 130 % on top of Small | hint bar only |
| Line height | font line height; boxes grow with it | never fixed |
| Weights | regular and bold only | no light, no italic |
| Ellipsis | one glyph "..."; built-in font draws three dots | |

The caller supplies: the string, the rect, role, tier, weight, align. The component decides: font,
color per mode, vertical centering, elision.

## Behavior

- Draws exactly one line; never wraps (use `TextBlock` for more).
- Right-aligned labels measure first and elide the left part of the available width, never overlap a
  neighbour: the caller gives non-overlapping rects (value slot, then label slot).
- Pure function of its inputs: no state, no allocation per frame (string views in, canvas out).
- Case: sentence case; the label does not change case itself. All caps only for short fixed labels
  (M0 decides whether section labels stay uppercase or become sentence case).

## Accessibility

- Primary and Muted meet 4.5 : 1 on every surface they are drawn on; Faint meets 4.5 : 1 by definition
  (it is the floor); Disabled is allowed below that only with a second cue and never for text the user
  must read to decide.
- Body is the minimum size for anything read to decide or confirm; Small only for secondary detail.
- Nothing clips, overlaps or leaves its box at Largest: it elides.
- No text sits directly on busy artwork: the caller provides a flat surface.
- Contrast registry pairs: Primary, Muted, Faint, Disabled on `background`, `surface`, `surfaceHi`.

## Largest text and high contrast

```
Largest (130 %):    Dark mode                                      Lar...
```

High contrast: Primary and Muted take the high-contrast text colors; Faint and Disabled are mapped to
values that still pass 4.5 : 1 and 3 : 1 respectively on black.

## API

```cpp
enum class TextRole { Title, Heading, Clock, Tab, Body, Small, Hint };
enum class TextTier { Primary, Muted, Faint, Disabled };
enum class Align { Left, Right, Center };
void drawLabel(Painter&, Theme&, const Rect&, std::string_view text, TextRole,
               TextTier = TextTier::Primary, bool bold = false, Align = Align::Left);
int  measureLabel(Theme&, std::string_view text, TextRole, bool bold);   // for right-aligned slots
```

## Derived from / Used by

Derived from: `Font::elide` and `drawCentered` uses everywhere, hand-written right alignment with
`measure` (5 places), `LoadingIndicator::drawFittedLine`, the text alphas on `textMuted` and
`textPrimary`.

Used by: *none yet.*
