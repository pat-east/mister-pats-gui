# DetailPanel

The right-hand panel that describes the focused item: art at its prepared size, a caption, an optional
status line, an optional video, and an optional strip of screenshots. Use it next to a list or inside a
dialog body. Do not use it for the whole screen.

Layer: container. Status: **proposal.** Values marked (M0) wait for the owner's decision.

## Anatomy

Panel beside a list (`ListDetail` split: left list, right panel, gap 26, half and half).

```
 +--------------------------+  26  +--------------------------------------------+
 | list rows ...            |      |  pad 24                                    |
 |                          |      |   +------------------------------------+   |
 |                          |      |   |                                    |   |
 |                          |      |   |   art slot                         |   |
 |                          |      |   |   prepared size, centered,         |   |
 |                          |      |   |   never scaled, no shadow          |   |
 |                          |      |   |                                    |   |
 |                          |      |   +------------------------------------+   |
 |                          |      |     ~status line~  (only while engaged)    |
 |                          |      |   Item title, wrapped, centered            |
 |                          |      |   ~second line~                            |
 +--------------------------+      +--------------------------------------------+
                                     surface panel, radius 10 (panel), fill backdrop
```

- The panel is a `Surface` (panel role). The art is centered in the slot at native size; the slot is
  at least as large as the largest prepared image for that view (DESIGN.md "Box art is fixed-size").
- The caption sits below the art: title (body or heading, bold), then one muted line.

## Variants

```
Art + caption                         Art + status + video                 Art + screenshots
+------------------+                  +------------------+                 +------------------+
|      [art]       |                  |     [video]      |  unscaled,      |      [art]       |
+------------------+                  +------------------+  centered       +------------------+
 Title                                 ~Looking for a video...~             +----+----+----+----+
 ~second line~                         Title                                |    |    |    |    |  16:9
                                       ~second line~                       +----+----+----+----+
```

- **Screenshot strip:** four thumbnails side by side, 16:9, gap 12, radius 5 (chip), 2 px stroke:
  `focus` when focused, `surfaceHi` otherwise. Slots without a screenshot are not drawn.
- **Video slot:** the existing preview display; video is blitted unscaled, centered in the art area. The
  slot may be wider than the art (for example 744 for a 689 image) so an unscaled video fits.
- **Status line:** one line under the art (see `Badge` / status vocabulary), shown only while a preview
  is engaged; the reserve is `lineHeight + 16` and exists only then.

## States

```
art loaded            loading                          no art
+--------------+      +--------------+                 +--------------+
|    [art]     |      |              |                 |  Item title  |  title drawn inside,
+--------------+      |  Loading     |  Placeholder    |  (bold,      |  wrapped, centered,
 Title                |  box art...  |  body, Muted    |   wrapped)   |  same size as the art
 ~line~               +--------------+                 +--------------+  slot

status shown          screenshots focused              empty selection
 [video / art]        +####+----+----+----+            the panel shows nothing; the list owner
 ~Looking for a       #    #                            supplies an EmptyState if needed
  video...~           +####+  (outline on the focused thumbnail)
```

- Placeholder text is one body-size muted line, elided: "Loading box art..." / "No box art". The word
  is **box art** everywhere (never "cover art" or "boxart").
- No drop shadow behind the art (DESIGN.md: no shadow behind covers; the current list detail draws
  one and is the exception to fix).
- The title-wrap-and-center block exists once (today it is duplicated).

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Panel padding | 24 (M0: 28 to match `Space.panelPad`) | |
| Panel radius | `Radius.panel` 10 | fill: backdrop surface, not a hard-coded alpha |
| Art slot | max prepared size, centered | not scaled; 689 x 624 image in a 744 x 624 slot |
| Caption title | bold, body or heading | wrapped, centered when it replaces the art |
| Second line | small, Faint | secondary detail only |
| Status line reserve | lineHeight + 16 | only while engaged |
| Thumbnails | 4 across, 16:9, gap 12, radius 5 | stroke 2 px |
| Footer (optional) | 112 reserved by the owner | caption block for a collection mode |

The caller supplies: art, title, second line, status, screenshots, video presence. The component decides
geometry, placeholders, colors.

## Behavior

- Read-only except for the screenshot strip, which can take focus: Left/Right move between thumbnails,
  Confirm opens a `Modal` viewer (see `Modal`, Viewer), Back returns.
- It shows what the owner's cursor selects; it holds no cursor of its own.
- Hints come from the screen (for example `{DPAD} Move`) and, while the strip has focus,
  `<-/-> Browse screenshots   A Open   B Back`.
- It repaints only when the selection, the art, the status or the strip focus changes.

## Accessibility

- The status line states what is happening in words (Working / Neutral / Error tone), never an icon alone.
- Placeholders are real text, not blank boxes. "Loading" and "No box art" differ in words.
- Focused thumbnail: 2 px `focus` outline (3 : 1 or better); unfocused thumbnails keep a visible
  `surfaceHi` edge that is not confused with focus.
- Text on the panel meets 4.5 : 1 on its surface in every theme (contrast registry).

## Largest text and high contrast

```
Largest: art and slot do not change; the caption wraps to more lines, the strip and status keep their size

   [art]
   ~status line grows to its metric height~
   A long item title that wraps
   onto a second line
   ~second line, elided with an ell...~
```

High contrast: the panel gets the Roomy border treatment of its surface (white 2 px unless the highlight
is White); the focused thumbnail takes the highlight color.

## API

```cpp
struct DetailContent {
    ImageRef art;                    // may be empty: loading or none
    bool artLoading = false;
    std::string_view title, second;
    Status status;                   // tone + text, empty when not engaged
    bool videoSlot = false;
    int screenshotCount = 0;         // 0..4
    int screenshotFocus = -1;        // -1: strip not focused
};
Rect detailArtSlot(const Rect& area, Theme&, bool hasStatus);
void drawDetailPanel(Painter&, Theme&, const Rect& area, const DetailContent&);
```

## Derived from / Used by

Derived from: the list view's detail panel, the options panel's art and screenshot side, the Disc
screen's art and thumbnail strip, `PreviewDisplay` status line (inventory C10c; tile report). Game-specific
content (ROM rows, "Start game") is application composition around this panel.

Used by: *none yet.*
