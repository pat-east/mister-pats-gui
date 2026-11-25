# ActionButton

The one large focusable action inside a screen ("Start game", "Continue"). Use it when a screen has one
main thing to do. Do not use it inside dialogs: dialogs have no buttons, their actions live in the
hint bar (DESIGN.md "Buttons").

Layer: item. Status: **implemented in the wireframes.** Values marked (M0) wait for the owner's decision.

## Anatomy

```
 <------------------------------ width of its column ------------------------------>
 +----------------------------------------------------------------------------------+  ^
 |                                                                                  |  | 64 (M0)
 |                              Play game                                           |  | radius 10
 |                                                                                  |  v
 +----------------------------------------------------------------------------------+
                  ^ label: bold, body size, centered, elided if long
```

- One action per button; its **label says what it will do or why it cannot yet** ("Play game",
  "Waiting for a supported disc"). The label is the explanation, so it is always text.
- One contrast helper picks the label color on the accent fill from the palette (a palette property,
  not a string compare on the theme name).

## Variants

None: one component with three states.

## States

```
Ready, focused                 accent fill, label in contrast color, one thin inset border
 +==================================================+
 | +----------------------------------------------+ |   inset border 1 px, contrasting color,
 | |                 Play game                    | |   inside the button bounds; no shadow, no growth
 | +----------------------------------------------+ |
 +==================================================+

Ready, not focused (Idle)      raised flat surface, label Primary
 +--------------------------------------------------+
 |                    Play game                     |
 +--------------------------------------------------+

Unavailable, not focused       raised flat surface, label Muted
 +--------------------------------------------------+
 |           ~Waiting for a supported disc~         |
 +--------------------------------------------------+

Unavailable, focused           same surface, focus outline 2 px + 4 px marker at the left edge
 +####################################################+
 #|#          ~Waiting for a supported disc~          #
 +####################################################+
```

- The focused-and-unavailable button never looks ready: no accent fill. Outline plus marker (the
  row focus rule) makes focus visible without suggesting it can run (M0: today only a marker).
- The button does not grow, animate or cast a shadow.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Height | 64 | grows to `lineHeight + 24` at larger text; Game options and Disc agree on 64 (M0) |
| Radius | `Radius.panel` 10 | |
| Label | bold, body, centered, elided | vertical centering by line height |
| Ready focused | accent fill, label on accent, inset border 1 px | border inset = `max(1, 2)` px from the edge |
| Idle / Unavailable | flat `surface` (raised), no border | gradient dropped (M0) |
| Marker | 4 px, `accent`, radius 2 | focused and unavailable only; same marker as rows |

The caller supplies: label, state (Ready / Idle / Unavailable), focused. The component decides colors,
contrast and border.

## Behavior

- **Confirm** runs the action when Ready. When Unavailable it does nothing; the label already says why
  (an optional `reason` toast may add one line, see `Toast`).
- Up/Down move focus to and from the button as part of the owner's cursor; the button holds none.
- Hint (from the screen): `A <Action>`; when Unavailable the A hint is omitted (DESIGN.md: leave the
  action out when it cannot continue).
- A busy state (for example "Rechecking disc...") is an Unavailable label, not a spinner.

## Accessibility

- Label on the accent fill is at least 4.5 : 1 in every color theme, in light and dark mode and in
  high contrast (contrast registry checks the pair `labelOnAccent` on `accent`).
- Focus is carried by fill plus border (Ready) or outline plus marker (Unavailable), never by hue alone.
- The state is a word in the label; nothing is communicated only by color.
- The button is at least 64 high (target size well above 44).
- Described as `{role: button, label, state: ready/unavailable, focused}`.

## Largest text and high contrast

```
Largest: the button grows to fit the text; long labels elide

 +--------------------------------------------------+
 |                                                  |   height = max(64, lineHeight + 24)
 |        Waiting for a supported disc               |
 |                                                  |
 +--------------------------------------------------+

High contrast: accent = the chosen highlight color; label color from the contrast helper (black on
yellow, black on white, and so on); the inset border uses the contrasting color; Idle/Unavailable
surfaces get the high-contrast border like other Roomy surfaces.
```

## API

```cpp
enum class ActionState { Ready, Idle, Unavailable };

void drawActionButton(Painter&, Theme&, const Rect&, std::string_view label,
                      ActionState, bool focused);
int  actionButtonHeight(Theme&);                         // 64, metric at larger text
Color labelOnAccent(const Theme&);                       // the one contrast helper (a palette property)
```

## Derived from / Used by

Derived from: the Disc screen's "Start game" button and the Game options "Start" button (two
implementations with different heights, fills and labels), plus the duplicated `contrastingText` and
Silver special case (inventory C14; tile report finding 9, 10).

Used by: *none yet.*

## Implementation

`drawActionButton` in src/ui-framework/kit/Input.cpp: states Ready and Unavailable, focused or not. Shown in the Input tab.
