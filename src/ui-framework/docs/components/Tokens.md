# Tokens

The named values every component is built from: radii, strokes, spacing, alpha levels, text roles and
status colors. A number like `px(8)` or an alpha of 150 never appears in a screen or component; it is
a token. Use them for every measurement. Do not invent a value for one screen: add a token with a
reason that applies to the component, not to one place.

Layer: tokens. Status: **proposal.** Values come from the survey and the consolidation reports; every
value marked (M0) waits for the owner's decision in vNext milestone M0. All sizes are design pixels
on the 1920 x 1080 canvas and pass through `Theme::px()`.

## Anatomy

How the tokens meet on one row (Roomy, focused). Names in angle brackets are tokens.

```
        <Space.rowGap 8>
   +---------------------------------------------------------------------+  <- <Radius.row 8>
   |#|  <Space.inset 18>  Dark mode                            (o) On |  <- outline <Stroke.focus 2>
   +---------------------------------------------------------------------+
    ^                     ^                                         ^
    <Space.marker 4>      <TextRole.body>, <TextTier.Primary>       <TextRole.body>, accent
                          row frame height <Space.rowFrame.roomy 56>
```

```
   Panel (dialog)
   +--------------------------------------------------------+  <- <Radius.panel 10>
   |  <Space.pad 28>                                        |
   |  Title                       <TextRole.heading>        |
   |  <Space.titleGap 16>                                   |
   |  Body text                   <TextRole.body>           |
   +--------------------------------------------------------+
```

## Variants

### Radius

```
chip   5    thumbs, small chips, keys
row    8    list rows, table rows, buttons inside rows
panel  10   panels, dialogs, fields, tile body, summary bars
pill   h/2  progress bars, badges (derived: half the height)
```

Dropped as outliers (M0): 4, 6, 7, 9, 18.

### Stroke

```
hairline  1   rules, separators
focus     2   focus outline, rest border (high contrast), field border, thumb outline, checkbox
emphasis  3   tile focus (border only; the tile never grows)
```

### Space

```
pad          28   panel padding
titleGap     16   title to content in a panel
margin       36   panel clamp: area minus this on each side
gap          26   between tiles, between columns (existing Theme gap)
rowGap        8   Roomy rows          6   Dense rows          4   Table rows
inset        18   text inset in a row (left); right inset 16
marker        4   accent marker bar width
lineExtra     9   extra per line in text blocks (line height + 9)
```

Existing shell metrics stay in `Theme` unchanged: side margin 72, vertical margin 32, top bar 96,
bottom bar 72.

### Alpha (named levels)

Alpha is chosen by meaning, never typed as a number.

```
scrim.dialog    210   dim behind a dialog
scrim.nested    120   dim behind a modal that opens over another modal
scrim.viewer    230   dim behind a full-bleed viewer
scrim.saver     190   dimmed screensaver
surface.faint   110   resting fill of a Roomy row on the page background
surface.overlay 235   panel that floats over content (on-screen keyboard)
art.backdrop    190   backdrop circle under a mark on cover art
text.faint      muted at >= 190 on dark   the floor for any text people must read
text.disabled   about 120                 only together with a second, non-color cue
```

Retired literals: 40, 55, 60, 70, 80, 90, 100, 140, 150, 155, 160, 170, 175, 180, 200, 205, 215, 220,
238 and the eased per-frame alphas. The text tiers become theme colors (`textFaint`, `textDisabled`)
with a value per mode instead of a runtime alpha, so the contrast registry can measure them.

### TextRole

Sizes before the text-size setting, and after (Standard / Larger / Largest = 100 / 115 / 130 %).

```
role      100 %   115 %   130 %   use
title      48      55      62     detail title
heading    36      41      47     screen and dialog titles
clock      33      38      43     top bar clock
tab        27      31      35     tab labels
body       21      24      27     everything read to decide or confirm
small      17      20      22     secondary detail only
hint       body    -       -      bottom bar legend, own setting 100 / 115 / 130 % on top
```

Rounding to the nearest pixel (M0). Spacing and radii do not scale with the text-size setting.

### TextTier

```
Primary    textPrimary
Muted      textMuted
Faint      textFaint      replaces muted at alpha 110..180
Disabled   textDisabled   not allowed without a second cue
```

### StatusColor and Tone

```
Neutral   textMuted           no icon needed
Done      accent              check icon, success and completion, never green
Action    accent              up-arrow icon, "do something" (update available)
Warning   orange              "!" icon (new token, replaces the coral warning for warnings)
Error     red                 "x" icon (new token, split from warning)
```

A tone always travels with a word; the icon separates Done from Action (see Badge).

## States

Tokens resolve per mode. The same token name gives a different value:

```
              dark (default)     light              high contrast
background    #0B0D14            light gradient     black
surface       #161A26            near white         black
accent        theme accent       darker shade       chosen highlight color
focus         #B9D5FF            accent             chosen highlight color
textMuted     #9AA4C0            darker gray        light gray
```

Each color theme (default, Nintendo, PlayStation, Xbox, others) supplies its own palette through the
palette provider; the token names do not change. Values for the new orange `Warning` and red `Error`
are set in M0.

## Measurements

The contrast registry is the measurement that makes the color tokens safe. Every pair a component
draws is listed with its minimum; a test checks each theme x dark/light x high-contrast color.

```
pair (foreground on background)                         minimum
text on background / surface / surfaceHi                4.5
textMuted, textFaint on surface / surfaceHi             4.5
textDisabled on surface / surfaceHi                     3
label on accent (large action button)                   4.5
warning, error as text on surfaceHi                     4.5
focus outline on background / surface / surfaceHi       3
accent marker, progress fill on surface / surfaceHi     3
checkbox outline (textMuted) on surface / surfaceHi     3
progress fill vs track                                  3
```

Survey estimates, to be re-measured: muted at alpha 190 is about 4.6 : 1, at 160 about 3.7 : 1, at
120 about 2.6 : 1 on `#0B0D14`; the checkbox outline `#17315F` on `surfaceHi` is about 1.15 : 1.

## Behavior

- Tokens are constants and functions, not state. `Theme` resolves them for the current mode, text
  size, hint size and high contrast.
- No literal radius, stroke, spacing or alpha outside tokens; `tools/check-ui.sh` lists exceptions.
- Adding a token: say which components use it and why an existing one does not fit; update this page
  and DESIGN.md in the same change.
- A token is never overridden per screen. A screen that needs a different value needs a different
  component or variant.

## Accessibility

- Contrast is a property of the palette: every token pair is in the registry with a minimum, and no
  theme ships without passing it.
- `Faint` is the floor for text people must read; `Disabled` is allowed only with a second cue.
- Status colors never carry meaning alone: every `Tone` has a word and an icon.
- Text roles scale in three steps; spacing does not, so components must fit at Largest (see Label,
  TextBlock).

## Largest text and high contrast

```
Standard:  body 21     Largest:  body 27     rows grow (frame = max(preset, text + 16))
           pad 28                pad 28      spacing stays, text grows, text elides if needed

High contrast: background black; accent, focus, favorite, success follow the chosen highlight
color; rest borders appear on Roomy rows (see FocusFrame); the registry runs for each highlight.
```

## API

```cpp
namespace ui::Radius  { constexpr int chip = 5, row = 8, panel = 10; }          // pill = h / 2
namespace ui::Stroke  { constexpr int hairline = 1, focus = 2, emphasis = 3; }
namespace ui::Space   { constexpr int pad = 28, titleGap = 16, margin = 36, gap = 26, inset = 18,
                        insetRight = 16, marker = 4, lineExtra = 9, rowGapRoomy = 8,
                        rowGapDense = 6, rowGapTable = 4; }
namespace ui::Alpha   { constexpr int scrimDialog = 210, scrimNested = 120, scrimViewer = 230,
                        scrimSaver = 190, surfaceFaint = 110,
                        artBackdrop = 190; }
enum class TextRole { Title, Heading, Clock, Tab, Body, Small, Hint };
enum class TextTier { Primary, Muted, Faint, Disabled };
enum class Tone     { Neutral, Done, Action, Warning, Error };

int   Theme::px(int design) const;                 // design px -> framebuffer px
Font &Theme::font(TextRole, bool bold = false);    // size includes the text-size setting
Color Theme::text(TextTier) const;
Color Theme::tone(Tone) const;
struct ContrastPair { const char *name; Color fg, bg; float minimum; };
std::vector<ContrastPair> Theme::contrastRegistry() const;   // used by the contrast test
```

## Derived from / Used by

Derived from: the radius, stroke, spacing and alpha literals found in every screen (inventory
section 2, "Tokens to extract"), `Theme` size functions, DESIGN.md "Sizes and spacing reference".

Used by: *none yet.*
