# ProgressBar

One calm progress pill: a track and a fill that shows how far a piece of work is. Use it wherever work
has a known or unknown end. Do not use it for a value the user sets (a deadzone, an analog trigger):
those are gauges and stay application components.

Layer: primitive. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext M0.

## Anatomy

```
 <----------------------------- width set by the container ----------------------------->
 (===========================...................................................)         ^
  ^ fill: accent, radius 6                 ^ track: surface, radius 6                     | 12
                                                                                          v
```

The bar is a pill: radius is half its height. The fill never gets narrower than its height, so even
at 0 % the user sees a bar and not an empty line. The bar carries no text; the line next to it (see
`ProgressPanel`) says what is happening.

## Variants

```
default     (===============......................)     h 12, radius 6, track surface
onSurface   (===============......................)     same, track surfaceHi (on a surface box)
thin        (=========...........)                      h 6, radius 3, only for hold bars in the
                                                        bottom bar (always with a text label)
```

- **default** is the only bar for progress of work: panels, dialogs, startup, updates.
- **onSurface** swaps the track color so the bar stays visible on a `surface` box.
- **thin** shows how long a button has been held. It has the same fill color as every other bar (the
  hold for favorite and the hold for remove are the same job). A warning tone is allowed only for a
  destructive hold, and then always together with its text label.

## States

```
0 %             (===...................................................)    minimum fill = height
42 %            (===========================..........................)
100 %           (======================================================)
indeterminate   (===...................................................)    static minimum fill,
                                                                           no motion (M0, see below)
high contrast   +--------------------------------------------------------+
                |(============================.......................)|   2 px white track outline
                +--------------------------------------------------------+   fill: highlight color
```

- **Monotonic.** The fraction passed in is clamped to 0..1 and never drawn smaller than the last frame
  of the same phase. A new phase starts a new bar.
- **Indeterminate** (fraction unknown, passed as a negative value) is a static bar at minimum fill
  plus the status line. No sweep, no pulse (calm premise). If "alive" feedback is wanted, one slow
  sweep after 2 s without a known fraction is the only allowed motion (M0).
- **Failed or stopped** work is not shown by a bar; the panel becomes a `Result` or an `Error`
  `Modal`.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Height | 12 (`Space.bar`) (M0) | design px; one height for every progress bar |
| Radius | height / 2 | |
| Minimum fill | height | |
| Track | `surface`; `surfaceHi` when on a `surface` box | |
| Fill | `accent` | follows high contrast and the color theme |
| Thin variant | height 6, radius 3, track `surfaceHi` | hold bars only |
| High-contrast outline | 2 px white around the track | unless the highlight is White |
| Width | set by the caller | e.g. 280 for a startup bar |

The caller supplies: fraction (or "unknown"), whether it sits on a surface box, thin or not. The
component decides: geometry, colors, outline, clamping.

## Behavior

- No input; it only reflects a number the owner of the work reports.
- Reports an overall fraction. A fraction that restarts for each sub-item (per-file bytes) makes the
  bar jump back and is not allowed; report the whole job.
- Repaints only when the visible fill width changes by at least one pixel.
- Contributes no hints. A bar for cancellable work is accompanied by `B Cancel` in the bottom bar.

## Accessibility

- Fill against track is at least 3 : 1 in every theme and mode (contrast registry pair
  `accent` on `surface` and on `surfaceHi`); in high contrast the outline adds a second boundary.
- Never the only cue: a bar always sits next to a word (the phase line) or, for a hold, a label.
- No flashing, no motion by default.
- The bar does not scale with the text-size setting; its text neighbours do.

## Largest text and high contrast

```
Largest (130 %): the bar keeps its height; the status line under it grows and elides.

  (===========================...........................)
  Reading your game folders and sorting th...
```

High contrast: black background, fill in the chosen highlight color, track outlined in white (not
for White, where the outline would equal the fill).

## API

```cpp
struct ProgressBar {
    float fraction = -1.0f;     // 0..1; negative = unknown (static minimum fill)
    bool onSurface = false;     // track surfaceHi instead of surface
    bool thin = false;          // hold bars only
    Tone tone = Tone::Done;     // Done = accent; Warning only for destructive holds
};
int drawProgressBar(Painter&, Theme&, const Rect& area, const ProgressBar&);   // returns height used
```

The owner of the work holds the fraction; the bar keeps no state besides the last drawn width for
the monotonic rule (passed in by the caller as `lastFill` where it matters).

## Derived from / Used by

Derived from (inventory C4e and the progress survey, 11 bars): `LoadingIndicator` (h 14, r 7), Splash
(280 x 10, r 5), the core-updater Checking and Installing bars (h 12, r 6), the Scan jump-start bar
(h 12, `surfaceHi`), the favorite/remove hold bars in the bottom bar (h 6, r 3), the Controllers
hold, countdown and Start-hold bars (h 10 to 14). Not derived: the deadzone and analog-trigger gauges
(h 20 and h 28), which are not progress.

Notes for the New GUI: the updater's Installing bar is per-file bytes and saw-tooths today
(`CoreMraUpdater` resets `currentBytes` per file); it must report the overall fraction. Splash becomes
h 12 (+2 px), `LoadingIndicator` h 12 (-2 px).

Used by: *none yet.*
