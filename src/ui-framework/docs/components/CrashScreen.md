# CrashScreen

The full-screen failure screen shown when the application cannot continue: a card with plain words
about what happened, what was changed and what to do, plus a small Details line. Use it only for a
failure the application cannot recover from inside itself. Do not use it for an error in a running
screen (that is the `Error` modal).

Layer: takeover. Status: **implemented in the wireframes.** Values marked (M0) wait for the owner's decision in vNext M0.

## Anatomy

No top bar and no hint bar exist at this point, so the one place a next step can be named is a
single line inside the card (the exception to "dialogs have no footer", M0).

```
+--------------------------------------------------------------------------------------+
|                                                                                      |
|        +--------------------------------------------------------------------+        |
|        |==================================================================== |  6 px, Error tone
|        |                                                                    |        |
|        |   +------+   **Something went wrong**                    heading   |        |
|        |   |  !   |   The interface stopped and could not continue.         |  body  |
|        |   +------+   Nothing on your device was changed.                   |  body  |
|        |                                                                    |        |
|        |   ~Details: failed while loading the game list (code 12)~          |  small |
|        |                                                                    |        |
|        |   A Try again   B Quit                           (one line)        |        |
|        +--------------------------------------------------------------------+        |
|        ^ card: surface, radius 10 (was 18), pad 28, no shadow                          |
+--------------------------------------------------------------------------------------+
```

## Variants

```
Failure        the default above; next step: Try again and Quit
Failure, loop  the same failure twice in a row: Try again is removed, only Quit is offered, and the
               Details line says why ("it stopped again right away")
Fallback       builtin font and fixed layout if the theme or fonts cannot load; same words
```

## States

```
shown          card on the plain background, input accepted for A and B only
waiting        no timer, no automatic restart: it waits for the user
retrying       A pressed: the application restarts the failed step once, a quiet "Trying again..."
closing        B pressed: the application quits to the system
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Card | Medium-width preset, height fits content, clamped to the area minus 36 | today `980 x 590` (M0) |
| Card | `surface`, radius 10, pad 28 | radius was 18; `Radius.panel` |
| Accent strip | 6 high, radius 3, Error tone | |
| Icon | 74 x 74 square, 3 px stroke, Error tone, "!" | glyph, always with the headline |
| Headline | heading, bold, `textPrimary` | plain words, never a raw error |
| Body | body, `textPrimary` | what happened, what changed |
| Details | small, `textMuted` | the technical reason, one line, elided |
| Next step | body, one line, in the card | `A Try again   B Quit`, formatted by the layout (M0) |
| Fonts | `TextRole` through the theme | text-size setting applies (today fixed px) |

## Behavior

- Input: A runs the single next step (Try again), B quits; nothing else is read.
- **No automatic retry, ever.** A crash and restart loop can burst the SD card with writes; every
  retry needs a user press, and a repeated failure removes Try again.
- The screen uses only the CPU renderer (never a GPU path), `Font`, `Theme` and the fallback font; it needs no application
  services, no modal stack and no allocation beyond its own strings, so it can run when the rest is
  broken. It writes nothing to storage except the application's existing crash log.
- Words come from the framework's closed structure (what happened, what changed, what to try), filled
  by the application; the raw reason goes into Details only.

## Accessibility

- Headline, icon and Error tone together; the strip is not the only cue. All text passes 4.5 : 1 on
  `surface` in every theme and mode.
- Text sizes follow the text-size setting and the card grows (the Details line elides); nothing clips
  at Largest.
- The next step is operable with the controller and named in the user's button layout.
- Nothing is on a timer and nothing flashes. Plain language, no error codes in the headline.

## Largest text and high contrast

```
Largest: card grows, body wraps, Details elides to one line
  +------------------------------------------------+
  |========================================        |
  | [!]  **Something went wrong**                  |
  |      The interface stopped and could not       |
  |      continue. Nothing on your device was      |
  |      changed.                                  |
  |      ~Details: failed while loading the ga...~ |
  |      A Try again   B Quit                      |
  +------------------------------------------------+
High contrast: black card, strip and icon in the highlight color, 2 px white card border unless the
highlight is white.
```

## API

```cpp
struct CrashInfo {
    std::string_view headline;      // plain words
    std::string_view whatChanged;   // e.g. "Nothing on your device was changed."
    std::string_view details;       // technical reason, one line
    bool canRetry = true;           // false after a repeated failure
};
enum class CrashChoice { Retry, Quit };

CrashChoice showCrashScreen(Display&, Theme&, Input&, const CrashInfo&);   // blocks, owns its loop
```

## Derived from / Used by

Derived from: the existing crash card (own radius 18, shadow, fixed pixel font sizes that ignore the
text-size setting, a status pill, no controller-operable next step; inventory C3, survey of the shell
and overlays). Open question for the owner: offer "A Restart MiSTer" as the next step.

Used by: *none yet.*

## Implementation

`drawCrashScreen` in src/ui-framework/kit/CrashScreen.cpp: card with the error strip, icon, what happened, what was changed, Details line and the one next-step line. Shown as a preview in the demo's Flows tab; the next-step line is a plain string today (not yet hint tokens).
