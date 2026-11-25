#pragma once

#include "ui-framework/kit/Palette.h"

namespace ui {

// One color theme: the colors of its dark mode and the page colors of its light mode. An application passes
// its own themes to makePalette(); the framework ships a small generic set (builtinColorThemes).
struct ColorTheme {
    const char *name;
    uint32_t accent, focus;
    uint32_t background, backgroundLo, surface, surfaceHi;
    uint32_t brightBackground, brightBackgroundLo;   // light mode
};

// A highlight color for high contrast: light, saturated, different in hue, so a viewer can pick the one that
// is easiest to see.
struct HighlightColor {
    const char *name;
    uint32_t rgb;
};

const ColorTheme *builtinColorThemes(int &count);
const HighlightColor *highlightColors(int &count);
constexpr int kDefaultHighlight = 2;   // yellow

Palette makePalette(const ColorTheme &theme, bool dark);
Palette makeHighContrastPalette(const HighlightColor &highlight);

}  // namespace ui
