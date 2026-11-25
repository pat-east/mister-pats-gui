# Surface

The filled rounded areas everything sits on: page, card, raised surface, faint row fill, overlay,
plus the two things built from them: the **Panel** (a dialog body) and the **Scrim** (the dim behind
it). Use Surface instead of calling `fillRoundedRect` in a screen. Do not pick colors or alphas by
hand.

Layer: primitive (Surface, Scrim) and frame (Panel). Status: **proposal.** Values marked (M0) wait
for the owner's decision in vNext milestone M0.

## Anatomy

A dialog over a screen: scrim, centered panel, title, body.

```
+--------------------------------------------------------------------------------------+
|  top bar and content, dimmed by the scrim over the whole canvas (alpha 210)          |
|                                                                                      |
|        +-----------------------------------------------------------+                |
|        |  <28>                                                     |  <- Radius.panel 10,    |
|        |  Restart MiSTer?                          heading, bold   |     fill surfaceHi,     |
|        |  <16>                                                     |     no drop shadow (M0) |
|        |  The GUI will close and start again.      body            |                |
|        |  Anything running is stopped.                             |                |
|        +-----------------------------------------------------------+                |
|                                                                                      |
|  bottom bar: redrawn above the scrim, shows the dialog's hints                       |
+--------------------------------------------------------------------------------------+
```

Panels are centered on the content area, clamped to the area minus 36 on each side.

## Variants

Surface roles:

```
Page      background gradient (background -> backgroundLo); top bar band in light mode
Card      surface            default area, boxes inside a panel, progress tracks
Raised    surfaceHi          panels, focused rows, fields on the page background
Faint     surface at alpha 110   resting fill of a Roomy row on the page background
```

Rule: a surface inside a Raised panel is a Card (darker), so its edge is visible. Anything the user
can focus carries an outline or marker; fill difference is never the only cue.

Panel sizes (presets; Small fits its content):

```
Small   760 wide, height fits content   confirm, error, update success, small notices
Medium  1120 x 760                      wizard pages, info, changelog, text entry, choose
Large   1812 x 860                      long lists and review, game options
```

All clamp to the area minus `Space.margin` 36. A wizard may change size per page.

Panel tones (optional 2 px stroke):

```
Neutral   no stroke
Done      2 px accent stroke
Warning   2 px warning stroke
Error     2 px error stroke
```

Scrim levels:

```
dialog   210   a dialog over a screen
nested   120   a dialog over another dialog (only the lower one dims further)
viewer   230   full-bleed viewer (screenshots)
saver    190   dimmed screensaver (black)
```

## States

```
dialog open          scrim over the whole canvas including top bar; tab switching is blocked
dialog over dialog   lower panel visible under the lighter scrim (alpha 120); only the top one acts
viewer               scrim 230, content area is the picture; no panel
takeover card        Panel on the plain background, no scrim (crash screen)
```

```
Nested (alpha 120 over the lower dialog):
  +--------------------------------------------+
  |  Game options                              |   lower panel, slightly dimmed
  |     +---------------------------+          |
  |     |  Screenshot 2 / 4         |          |   upper panel or viewer
  |     +---------------------------+          |
  +--------------------------------------------+
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Panel radius | `Radius.panel` 10 | |
| Panel padding | `Space.pad` 28 | one value everywhere (M0) |
| Title to content | `Space.titleGap` 16 | title `heading`, bold, `textPrimary` |
| Screen margin | `Space.margin` 36 | panel never closer than this to the area edge |
| Panel fill | `surfaceHi` | |
| Scrim color | `shadow` token (black) at the level above | one level per kind (M0) |
| Shadow | none (M0) | `dropShadow` on a 1120 x 760 panel is estimated at 15-35 ms per frame on the device; to be measured |
| Heights | Small fits content; others fixed but clamped; no fixed height that text can outgrow | |

The caller supplies: size preset, tone, title. The component returns the body rect; it draws no footer
and no buttons (dialogs have none, hints live in the bottom bar).

## Behavior

- Surface and Scrim are stateless draw functions; Panel returns the body `Rect` for the content.
- The scrim covers the whole canvas, including the top bar, and blocks the tab keys while a dialog is
  open. The bottom bar is redrawn above the scrim and shows the top dialog's hints.
- A dialog never draws a hint footer inside the panel.
- While a modal is open the frame may be repainted in full each frame; a later phase restores only the
  panel rect from a snapshot (see ModalStack).
- Panel text uses Label and TextBlock; nothing draws outside the panel; text elides.

## Accessibility

- Text on a surface meets the contrast registry (4.5 : 1); the registry covers every surface role.
- The panel is separated from the dimmed content by the scrim, not by a shadow; the stroke for a tone
  is a second cue, never the only one (the title and a word carry the tone).
- Panel height follows its text; no clipped or silently truncated content at Largest.
- Faint fill is decoration; focus on a faint row is carried by outline and marker (FocusFrame).
- No flashing; the scrim appears in the same frame the dialog opens.

## Largest text and high contrast

```
Largest: Small panel grows to fit, clamped to the area; Medium/Large stay and the body scrolls

  +----------------------------------------------+
  |  Restart MiSTer?                             |   heading 47 px
  |                                              |
  |  The GUI will close and start again.         |   body 27 px, line step + 9
  |  Anything running is stopped.                |
  +----------------------------------------------+

High contrast: panel fill black, 2 px highlight outline on the panel, text in the high-contrast
text color; scrim black at the same alpha.
```

Light mode: `surfaceHi` is lighter than `surface`; boxes inside a panel stay Cards for visible edges.

## API

```cpp
enum class SurfaceRole { Page, Card, Raised, Faint };
enum class PanelSize   { Small, Medium, Large };
enum class ScrimLevel  { Dialog, Nested, Viewer, Saver };

void fillSurface(Painter&, Theme&, const Rect&, SurfaceRole, int radius);
void drawScrim(Painter&, Theme&, const Rect& area, ScrimLevel);
Rect drawPanel(Painter&, Theme&, const Rect& contentArea, PanelSize, Tone, std::string_view title,
               int contentHeightHint = 0);     // returns the body rect; Small uses the hint
```

No state. Panel is also usable by takeover screens before a ModalStack exists.

## Derived from / Used by

Derived from: ~120 `fillRoundedRect` calls, the 20 dialog frames and their scrim alphas 150 to 230
(inventory C5 table), CrashScreen card, keyboard panel, row fills of every list (consolidation
reports).

Used by: *none yet.*
