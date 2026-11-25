# HoldProgress

The bottom bar's answer to "I am holding a button and something will happen": the hold hint with a thin bar
under it that fills while the button is held. It replaces the help line for as long as the hold runs, so the
progress is visible whatever the help contained and no screen has to draw it.

Layer: primitive (part of Hints and the Shell). Status: **implemented.**

## Anatomy

```
Hold X Remove
======..............        <- thin bar, grows from empty to full while the button is held
```

The text is a hold hint: the word "Hold", the key token, the action (`Hint::hold = true`). The same hint
sits in the normal help line (without the bar) so the user learns the hold before using it.

## States

```
help line      (+) Move   Hold X Remove   A Choose   B Back              v1.4.2
holding 0 %    Hold X Remove                                             v1.4.2
               ......................
holding 60 %   Hold X Remove                                             v1.4.2
               ============..........
done           the help line returns; the action's own feedback (a dialog, a toast) follows
cancelled      the help line returns (released early, another button, the focus moved)
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Bar | 6 high, width of the text but at least 240 | track `surface`, fill `accent` |
| Gap text to bar | 5 | the pair is centered in the bottom bar |
| Text | the hint size, essential (never dropped) | |

## Behavior

- The host owns the hold (its length, its cancel rules) and reports a progress from 0 to 1. The shell shows it
  through `ShellSpec::hold` (`active`, `hint`, `progress`); `drawHoldProgress` is the function behind it.
- Progress at 0 is not shown: the help line stays.
- A hold is a shortcut, never the only way (ACCESSIBILITY.md 2.2): the same function must also be reachable by
  a normal press somewhere (a menu or options entry).

## Accessibility

- Progress is shape and length, not color alone; the key and the action are written out.
- Same contrast as the hints; the bar uses the accent on the surface tier (3 : 1 non-text).
- Nothing is lost by releasing: the action happens only when the bar is full.

## API

```cpp
struct Hint { HintKey key; Text action; bool essential = false; bool hold = false; };
struct HoldSpec { bool active = false; Hint hint; float progress = 0; };   // in ShellSpec::hold
int drawHoldProgress(Painter&, const Theme&, const Rect &area, const Hint&, float progress);
// strings: "hint.hold" = "Hold"
```

## Derived from / Used by

Derived from: the hold lines of the Classic bottom bar.
Used by: the application shell, through `ShellSpec::hold`.
