# Toast

A short, quiet confirmation that disappears by itself. Use it only when a change happened that nothing
on screen shows, or when a blocked action needs one short explanation. Do not use it for progress
(show it inline), for failures (use an `Error` modal) or for anything the user must read, decide or
act on.

Layer: primitive. Status: **implemented** (`src/ui-framework/kit/Toast.h`, 2026-11-23); the values below are the decided ones.

## Anatomy

A pill in its own slot just above the bottom bar, so it never collides with the hints.

```
                                                  +----------------------------------+
                                                  | [v] Time zone set to Berlin      |   <- pill
                                                  +----------------------------------+
+--------------------------------------------------------------------------------------------+
| (+) Move   A Choose   B Back                                                 v1.4.2        |
+--------------------------------------------------------------------------------------------+
   bottom bar: hints stay readable and are never overlapped
```

Right-aligned to the content margin. One line, body size, elided to at most 60 % of the screen
width, about 48 characters at most. Icon plus text, never color alone. In ASCII `[v]` is the check
icon.

## Variants

```
Done      [v] Saved                          accent, check icon   a change was made
Notice    . Another task is running. Try again when it ends.     Neutral tone, dot icon:
                                              a blocked action explained in one line
```

`Notice` is the Neutral tone of the status set. There is no failure variant: failures use an `Error`
modal.

## When to toast

```
toast            a change nothing else shows ("Time zone set to Berlin"), or one line explaining a
                 blocked action ("Another task is running. Try again when it ends.")
inline status    work in progress, disabled states: a row value, a button label, the ProgressPanel
                 line
Error modal      every failure: what happened, what changed, a Details line, a next step
Info modal       longer results or anything with a path or a list
```

## States

```
showing        pill visible for 3.5 s, plus 1 s for every further 40 characters (one policy, no shorter
               "position feedback" variant: the line must stay long enough to read, ACCESSIBILITY 2.4)
replaced       a newer toast replaces the current one in the same frame (no queue)
gone           removed instantly; no fade (a fade repaints every frame)
suppressed     not drawn, and its time does not run, while a modal is open or a hold hint is showing,
               so it is never lost unseen
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Slot | directly above the bottom bar, 12 gap | right edge at the content margin |
| Width | content-fitted, at most 60 % of the screen | text elided inside |
| Height | body line height + 16 | grows with Largest text |
| Radius | half the height | pill |
| Text | body, Primary on `surfaceHi` | |
| Icon | 16 + gap 8 | Done: check; Notice: dot |
| Duration | 3.5 s + 1 s per further 40 characters | `Toast::duration()` |
| Length | one line, at most about 48 characters | no paths, counters or progress |

The caller supplies: text, tone (Done or Notice), optional short duration. The component decides:
slot, size, elision, lifetime.

## Behavior

- Raised through one call (`Toast::show(text, tone, minSeconds)`; `minSeconds` (default 0) keeps a line up at least that
  long when the owner knows better than the policy, never shorter than it; an application wraps it in its own sink, e.g. the New GUI's
  `patsgui::Notice`); there is one toast at a time, a newer one replaces it and restarts the time.
- No input and no hints of its own; it never takes focus.
- Repaints once when it appears and once when it disappears, nothing in between.
- Never carries essential information: anything the user must read or act on stays until dismissed in
  a modal or stays inline. A toast of 3.5 s is long enough for one short sentence; longer lines
  stay 1 s more per 40 characters (the accessibility guide's open question 4, decided).

## Triage (keep, drop, move)

Rules applied when writing a call site. Notes for the New GUI follow.

- **Drop** a toast if the screen already shows the effect (the star appears, the view changes, the app
  starts), if it narrates progress, or if it repeats what a label already says.
- **Move to an `Error` modal** every failure toast; failures never toast (today they show in the
  success color).
- **Move inline** progress and disabled-state explanations (button label, row value, `ProgressPanel`).
- **Keep** confirmations of saved settings, removals where the item disappears, one-line blocked-action
  explanations, and navigation feedback shared through one helper.

Notes for the New GUI: drop "Starting X", "Game view: X", "Showing: ...", "added to favorites",
"Button mapping cancelled", the progress toasts ("Creating settings backup...", "Downloading...",
"Checking ... API key...", "Installation is finishing safely"), font enabled or disabled; move
failures (time zone, Wi-Fi scan, TheGamesDB, disc installer, icon and font download errors) to the
`Error` modal; move most Disc toasts inline (button label plus one status line); "Backup saved to
<folder>" becomes an Info modal; keep "Games starting with X" as one shared helper, "Stick deadzone
saved", "Button mapping saved", the saved-credentials and time-zone confirmations, "removed from
favorites", "removed from Recently played", and one constant "Another online task is running. Try
again when it finishes."

## Accessibility

- Text meets 4.5 : 1 on the pill; the icon 3 : 1; icon and text together, never color alone.
- Nothing essential appears only as a toast (guide rule 2.4); a toast never contains the only copy of
  an instruction.
- 3.5 s for at most one short line leaves time to read it; it does not flash, slide or fade.
- Does not overlap hints, so the way out stays visible while a toast shows.
- Body size, Primary tier; at Largest it elides at 60 % width and the pill grows taller.

## Largest text and high contrast

```
Largest (130 %):
                                          +--------------------------------+
                                          | [v] Time zone set to Ber...    |   taller pill, elided
                                          +--------------------------------+
```

High contrast: pill outlined in white, icon and text in the high-contrast colors.

## API

```cpp
ui::Toast toast_;                                              // one per shell
toast_.show(Text::data("Time zone set to Berlin"));            // Tone::Done (check); Tone::Neutral = notice (dot)
toast_.show(text, Tone::Done, 8.0f);                           // at least 8 s (the policy is the floor)
redraw |= toast_.update(dt, dialogOpenOrHoldShowing);          // true once, when it goes away
toast_.draw(p, theme, p.bounds());                             // after the page, not while suppressed
toast_.rect(p, theme, p.bounds());                             // where the pill is (a partial redraw must not cross it)
Toast::duration(text);                                         // 3.5 s + 1 s per further 40 characters
```

The caller supplies text (a catalog key or data) and the tone; the component decides slot, size, elision and
lifetime. Other tones than Done and Neutral are drawn as Neutral: there is no failure variant. High contrast
draws the plain outline. The demo's Text tab shows it (A: confirmation, Y: notice).

## Derived from / Used by

Derived from: `Context::notify` (about 75 call sites in 12 files), drawn today as small text in the
`success` color for every message, including failures, overlapping the hint line when long
(consolidation report, section 5).

Used by: the New GUI shell (`src/app/PatsGui`, through `src/app/Notice.h`) and the demo's Text tab.
