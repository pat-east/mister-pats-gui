# Modal

A centered dialog over the current screen, with one frame and a body. Use it to tell, ask, show
progress, offer a choice, take text or show an image, without leaving the screen underneath. Do not
use it for a whole-screen page (see `Screen`) or for a step sequence (see `Wizard`, which hosts its
pages in a Modal).

Layer: overlay. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext M0.
Tone icons in the drawings: `(v)` Done, `(^)` Action, `(!)` Warning, `(x)` Error.

## Anatomy

The scrim covers the whole canvas, top bar included; the hint bar is drawn again above it and shows
the top modal's hints. Dialogs have no buttons and no footer.

```
+--------------------------------------------------------------------------------------+
|  top bar ....................................................... (dimmed by the scrim) |
|                                                                                      |
|            +--------------------------------------------------------+                |
|            |<28>                                                    |                |
|            |  **Title**                                             |  heading, bold |
|            |  <16>                                                  |  textPrimary   |
|            |  Body: text, a List, a Result, a keyboard or an image  |                |
|            |                                                        |                |
|            +--------------------------------------------------------+                |
|            ^ panel: surfaceHi, radius 10, pad 28, no shadow                           |
|                                                                                      |
|  A Close   B Back ........................ (hint bar, redrawn above the scrim, in full)|
+--------------------------------------------------------------------------------------+
```

The panel is centered on the content area. Stroke: none for a neutral tone; 2 px in the tone color
(accent, orange, red) for Done, Action, Warning, Error (M0).

## Variants

Four families. Message covers five tones of one layout.

```
Info      +--------------------------+   Confirm  +--------------------------+
          | **System information**   |            | **Restart now?**         |
          | Label A        value     |            | The device restarts and  |
          | Label B        value     |            | returns to this screen.  |
          +--------------------------+            +--------------------------+
          hints: B Close                          hints: A Restart   B Cancel

Error     +==========================+   Result    +==========================+
          | (x) **Could not save**   |            | (v) **Settings saved**   |
          | The change was not made. |            | 3 changed, 0 failed.     |
          | ~Details: disk is full~  |            +--------------------------+
          +==========================+            hints: A Done
          hints: A Try again   B Close   (== red 2 px stroke)

Progress  +--------------------------+   Choose   +--------------------------+
          | **Installing**           |            | **Choose a region**      |
          | ========..........       |            | |# Europe                |
          | Keep the device on.      |            |    Asia                  |
          +--------------------------+            |    Africa                |
          hints: B Cancel (if allowed)            +--------------------------+
                                                  hints: UpDown Move  A Choose  B Cancel
Input     +--------------------------+   Viewer   +--------------------------------------+
          | **Network name**         |            |  (image, centered, native size)      |
          | [ my-network_          ] |            |            2 / 4                     |
          | (on-screen keyboard)     |            +--------------------------------------+
          +--------------------------+            hints: LeftRight Browse  A Close
          hints: Dpad Choose key  A Enter  X Delete  Y Space  Start Done  B Cancel
```

- **Message** = Info, Confirm, Error, Result, Progress. They differ only in tone, actions and an
  optional bar. A success is a Result with the Done tone, not a separate variant. Error is a Message with the Error tone on the top layer.
- **Choose** hosts any list body (a `List`, a `Result`, a review list), single or multi select.
- **Input** hosts a `TextField` and the `Keyboard`; the keyboard is part of the body, not a modal.
- **Viewer** is a full-bleed image with a counter; its own scrim, no panel.
- A custom `drawBody` hook covers application panels (an options panel with art and rows).

## States

```
normal              panel, body, hints in the bar
busy (Progress)     bar + one calm line; Back only if cancellable, else the hint is absent
over another modal  second scrim alpha 120; only the top modal gets input
error body          plain headline, what changed, small Details; never a raw error string
long body           scrolls with the shared Scrollbar and range text, never silently cut
```

## Measurements

Design pixels at 1080p. All sizes clamp to the content area minus 36.

| Item | Value | Notes |
| --- | --- | --- |
| Small | 760 wide, height fits content | confirm, error, result |
| Medium | 1120 x 760 | info, choose, input, wizard pages (M0) |
| Large | 1812 x 860 | review lists, panels with art (M0) |
| Pad | 28 | `Space.panelPad` |
| Title gap | 16 | title to body |
| Title | heading, bold, `textPrimary` | tone color only on the icon and stroke |
| Panel | `surfaceHi`, radius 10 | `Radius.panel` |
| Scrim | alpha 210 dialog, 120 over a modal, 230 viewer | one token each (M0) |
| Stroke | 2, tone color, non-neutral only | none for Neutral |
| Shadow | none | measured on the device before M4 closes (M0) |

The caller supplies title, tone, size, body, hints. The modal decides geometry, scrim, stroke, input
routing for Confirm/Back/Start and the hint ordering.

## Behavior

Input by variant (logical actions):

| Variant | A | B | Start | Other |
| --- | --- | --- | --- | --- |
| Info | close | close | = A | none |
| Confirm | primary action | cancel | = A, unlisted | none |
| Progress | ignored | cancel if cancellable, else ignored, hint absent | ignored | none |
| Result | primary (Done or Try again) | close | = A | X for declared extra choices |
| Error | primary (Close or Try again) | close | = A | none |
| Choose | select | previous level, else cancel | declared secondary, else = A | L/R only if the body declares filter tabs |
| Input | key | back, discards text | Done, on every entry page | X Delete, Y Space |
| Viewer | close | close | = A | L/R browse |

- Hints are declared by the modal; it never draws a footer or repeats them in the body.
- Status text ("Keep the device on") lives in the body; hints carry keys only.
- Error content: headline in plain words (what happened), one line saying what was or was not
  changed, a small muted `Details:` line for the technical reason, then the hints
  `A Close` or `A Try again   B Close`. Failures are Errors, never toasts.
- Success and Done use the accent, never green. A second error queues (see `ModalStack`).

## Accessibility

- Title, tone icon and word carry the tone; color is only the stroke. Stroke and icon pass 3 : 1.
- Panel text passes 4.5 : 1 on `surfaceHi` in every theme and mode (contrast registry).
- Focus inside a body (a List, a keyboard) uses the standard focus frame; nothing is hidden by the
  scrim. Hints in the bar name every action available now, in the user's button layout.
- Nothing important is on a timer; a Progress modal says what is happening in one line.
- Text elides or wraps; the panel grows within its preset instead of clipping.
- Symbols used: tone glyphs (check, up arrow, exclamation, cross) are in the glossary and always sit
  next to a word.

## Largest text and high contrast

```
Largest (130 %): panel grows to the preset's maximum, body scrolls, title may wrap to two lines
  +----------------------------------+      (Small is 760 wide and gets taller;
  | (x) **Could not save the         |       Medium/Large keep their size and scroll)
  |     settings**                   |
  | The change was not made.         |
  | ~Details: the disk is full~      |
  +----------------------------------+
High contrast: panel black, stroke and tone icon in the highlight color, text in the high-contrast
text color, 2 px white border on the panel unless the highlight is white.
```

## API

```cpp
enum class Tone { Neutral, Done, Action, Warning, Error };
enum class ModalSize { Small, Medium, Large };
enum class ModalResult { Stay, Close };

namespace ModalFrame {   // stateless; usable before the stack exists
    Rect draw(Painter&, Theme&, const Rect &area, ModalSize, Tone, std::string_view title);
    void scrim(Painter&, Theme&, const Rect &area, bool overModal);
}
class Modal {
public:
    virtual std::string_view title() const = 0;
    virtual ModalSize size() const { return ModalSize::Medium; }
    virtual Tone tone() const { return Tone::Neutral; }
    virtual void drawBody(Painter&, Theme&, const Rect &body) = 0;
    virtual void hints(Hints&) const = 0;
    virtual ModalResult handle(Action, ModalStack&) = 0;
    virtual void update(float dt) {}
    virtual bool animated() const { return false; }
};
```

State belongs to the flow or screen that owns the modal; the stack holds non-owning pointers.

## Derived from / Used by

Derived from: 20 dialog frames with different padding, scrim alpha, size, shadow and stroke, plus the
footers drawn inside panels (inventory C5, C6; modal consolidation report). The core-updater dialog
is the structural reference; the error dialog is the reference for the Error variant.

Used by: *none yet.*

## Status (implemented, 2026-11-22)

Built in `src/ui-framework/kit/Modal.{h,cpp}`: the seven variants (Info, Confirm, Error, Result, Progress, Choose, Viewer; Input
waits for `TextField` and `Keyboard`), the three sizes, the one frame, the tone stroke, the hints per variant, and
`ModalStack` with the lighter scrim for a dialog over a dialog. The demo's Dialogs tab opens each one; the help
bar is redrawn above the scrim with the top dialog's help, and the tabs are locked while one is open.
