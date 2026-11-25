# Theming

How the framework looks in dark and light mode, in a color theme and in high contrast. Components never
choose a color themselves: they read the **palette** through `Theme`. One change of mode, theme or contrast
therefore reaches every screen without a line in the screen.

Status: **implemented** (`src/ui-framework/kit/Palette.h`, `Palettes.h/.cpp`, `Theme.h`, `Contrast.h/.cpp`).

## The axes

```
mode         dark | light
color theme  22 built in (an application may supply its own with Theme::setColorThemes)
high contrast  on | off, with a highlight color (8 built in, default white)
```

`Theme::setDarkMode`, `setColorTheme` and `setHighContrast(on, highlight)` rebuild the palette; components
read it on the next frame. High contrast ignores mode and color theme: black ground, white text, the
highlight color for focus, plus plain borders around rows and fields when the highlight is not white.

## The palette

`Palette` holds the colors a look is made of: background (and its darker end), surface, raised surface,
accent, primary and muted text, warning, error, focus, and the top bar's own colors. `finishPalette` derives
the rest so that contrast holds in every combination:

| Derived color | Rule |
| --- | --- |
| `textMuted` | strengthened toward primary text until it reaches 4.5 : 1 on every surface |
| `textFaint` | secondary detail text: 4.5 : 1 on every surface |
| `textDisabled` | only with a second cue: 3 : 1 |
| `accentText` | the accent as words and numbers: 4.5 : 1 (the plain accent may be too dark or too light as text) |

Text tiers (`TextTier::Primary / Muted / Faint / Disabled`) and tones (`Tone::Done / Action / Warning /
Error / Neutral`) are resolved through the palette; no component carries its own alpha values.

## The top bar

The bar stays dark in light mode (light text on a dark bar that fades into the page), so its colors are
separate (`topBarFill`, `topBarText`, `topBarMuted`, `topBarFocus`). Tones on the bar use their own light
variants (`barTone` in Shell.cpp): the surface tones are made for light panels and would not be readable on
the bar.

## Contrast check

`ui-demo --contrast` checks the registered pairs (text tiers on background, surfaces and the top bar,
accent text, tones, focus) for every theme in dark and light mode and for every high-contrast highlight: 52
configurations, thresholds 4.5 : 1 for text and 3 : 1 for non-text. The current result is that no pair falls
short. A new theme must pass it before it is listed; `finishPalette` repairs muted and faint text
automatically, but a theme whose accent or warning colors cannot be repaired fails the report.

## What the wireframes show

The Appearance tab changes dark or light mode, color theme, high contrast and highlight live. `make -C src/ui-framework run`
also renders a light-mode screen, a light-mode dialog and two high-contrast screens to `src/ui-framework/build/out`.

## Rules for components

1. Read colors from `Theme` (`t.palette.*`, `t.text(tier)`, `t.tone(tone)`); never write a color literal.
2. Use `accentText` for accent-colored words, never `accent`.
3. In high contrast, draw `t.borders()` outlines on rows and fields; the focus color carries the focus.
4. On the top bar use the bar's colors.

## Button labels

The focused, ready `ActionButton` uses `readyButtonColors(palette)`: a white label on the accent darkened as far
as needed for 4.5 : 1 (at most 45 % toward black). Where white cannot get there (a light accent such as Gold or a
bright high-contrast highlight) the label is dark on the accent itself, and in high contrast with a colored highlight
the label is dark on the plain highlight (7 : 1 or better). The contrast report checks this pair for every
configuration. `ui-demo` renders `themes.png`, `themes-2.png`, ... (18 configurations per sheet: every theme in every mode and the high-contrast colors) (buttons, text tiers,
accent text, cursor row) to look at before a theme is accepted.
