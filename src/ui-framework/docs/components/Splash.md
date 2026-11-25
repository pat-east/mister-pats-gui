# Splash

The startup screen: logo, one progress bar and, on a long start, one calm line saying what is
happening. Use it for the time between launch and the first interactive screen. Do not use it for
progress inside the running application (see `ProgressPanel`).

Layer: takeover. Status: **proposal** (the status line is new; the logo and bar are today's).

## Anatomy

Normal start (about one second): logo and bar only, exactly as today.

```
+--------------------------------------------------------------------------------+
|                                                                                |
|                                                                                |
|                                  +------+                                      |
|                                  | LOGO |   128 x 128, image radius 16         |
|                                  +------+   centered, 30 above the middle      |
|                                                                                |
|                                       <--- 40 --->                             |
|                              ==========..............                           |
|                              ^ track 280 x 10, radius 5, surface;               |
|                                fill accent, minimum fill 10                     |
|                                                                                |
+--------------------------------------------------------------------------------+
```

Long start (box-art scan, first library build): one status line appears under the bar.

```
+--------------------------------------------------------------------------------+
|                                  +------+                                      |
|                                  | LOGO |                                      |
|                                  +------+                                      |
|                              ===================.......                         |
|                                                                                |
|                              ~Getting your games ready~                         |
|                              ^ one line, body size, textMuted, centered,       |
|                                16 below the bar, no animation                   |
+--------------------------------------------------------------------------------+
```

## Variants

One phrase, from a closed set. A feature cannot add free text.

```
(none)                      normal start: logo and bar only
Getting your games ready    a long start is already known: no saved box-art index, or no library file yet
```

- No file names, system names, counts, percentages or timers. The bar shows how far it is.
- A phrase never changes while the splash is up, so there is no fade and no flicker. "Almost there" was
  dropped: it would change at a moment that has nothing to do with the real state, and the bar already
  says how far along the start is (M0).
- The line is added only when a long start is known beforehand; a normal one-second start never shows it.
- The startup task runs synchronously and no frame is presented while it runs, so the line is drawn once,
  before the task starts, and stays. It cannot be updated during the task.
- If the startup task fails or times out, the splash ends in the `Error` modal ("Could not start",
  what was changed, what to try, `A Try again   B Quit`), never in a frozen bar.

## States

```
running, short            logo + bar
running, long             logo + bar + one status line
error                     Modal(Error) over the background; splash bar removed
finished                  100 % frame shown once, then the first screen
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Logo | 128 × 128, radius 16 | embedded image, decoded once |
| Logo position | centered, 30 above the middle | |
| Bar | 280 × 10, radius 5 | track `surface`, fill `accent`, minimum fill 10; the shared `ProgressBar` |
| Gap logo → bar | 40 | |
| Status line | body, `textMuted`, centered, 16 below the bar | elided to the bar width + 120 |
| Background | `background`, cleared | no top bar, no bottom bar, no hints |

## Behavior

- No input is accepted except Quit.
- Runs its own pacing loop before the application's render path exists (about 60 frames per second,
  one unit of work per frame).
- Progress: first quarter is the storage-settle wait, then the remaining three quarters are shared
  between preparing icons and the startup task. The phase text follows the phase, not the fraction.
- Shares the Systems grid geometry with the Systems screen through one function, so the preloaded
  icons match the cache entries (today the constants exist twice).

## Accessibility

- The phrase is `textMuted` on `background` and passes 4.5 : 1 in every theme and mode (checked by the
  contrast registry).
- The bar's fill against its track passes 3 : 1; high contrast uses the highlight color.
- Text size applies to the line; at Largest it still fits (one line, elided).
- Nothing flashes; the only change over time is the bar and, rarely, one phrase.
- The logo is decoration: no information depends on it.

## Largest text and high contrast

```
Largest: the line grows, the logo and bar stay the same size

                              =====================.....
                              ~Getting your games ready~     (130 %, still one line)

High contrast: black background, bar fill in the highlight color, text in the high-contrast
muted-text color.
```

## API

```cpp
enum class StartupPhase { None, GettingGamesReady };   // closed set; text chosen by the framework

struct SplashProgress {
    void phase(StartupPhase p);   // draws one frame immediately, then the task continues
};

void showSplash(Display&, Theme&, const SplashLogo&, bool longStartKnown,
                const std::function<void(SplashProgress&)> &startupTask);   // blocks, owns its loop
```

The application passes the startup task and whether a long start is already known; it calls
`phase()` before the task's long part begins.

## Derived from / Used by

Derived from: the current startup splash (logo, bar, icon preloading, drive warm-up), the
progress-panel survey (inventory C3a, C4e).

Used by: *none yet.*
