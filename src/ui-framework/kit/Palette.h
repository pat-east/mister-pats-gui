#pragma once

#include <algorithm>
#include <cmath>

#include "ui-framework/core/Color.h"

namespace ui {

// WCAG contrast ratio of two opaque colors (1 to 21).
inline float contrastRatio(const Color &a, const Color &b) {
    auto channel = [](uint8_t v) {
        const float s = v / 255.0f;
        return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
    };
    auto luminance = [&](const Color &c) { return 0.2126f * channel(c.r) + 0.7152f * channel(c.g) + 0.0722f * channel(c.b); };
    const float la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
}

// The colors a look is made of, for one theme in one mode. Components read colors only from here (through
// Theme), never their own: that is what makes dark and light mode, color themes and high contrast work
// everywhere without a line in a screen.
struct Palette {
    bool dark = true;           // dark mode (false: light mode)
    bool borders = false;       // high contrast: rows and fields get a plain white border

    Color background   = Color::rgb(0x0B0D14);
    Color backgroundLo = Color::rgb(0x05070C);
    Color surface      = Color::rgb(0x161A26);
    Color surfaceHi    = Color::rgb(0x222839);
    Color accent       = Color::rgb(0x4C8DFF);
    Color textPrimary  = Color::rgb(0xFFFFFF);
    Color textMuted    = Color::rgb(0x9AA4C0);
    Color warning      = Color::rgb(0xF0A33A);   // orange: needs attention
    Color error        = Color::rgb(0xE06A5A);   // red: failed
    Color focus        = Color::rgb(0xB9D5FF);

    // Derived by finishPalette(): the text colors that reach their contrast on every surface, and the accent
    // as text (the accent itself may be too dark to read on a dark surface, or too light on a light one).
    Color textFaint    = Color::rgb(0x8890A8);   // secondary detail: at least 4.5 : 1 on every surface
    Color textDisabled = Color::rgb(0x6A7288);   // only with a second cue: at least 3 : 1
    Color accentText   = Color::rgb(0x4C8DFF);   // accent used for words and numbers: at least 4.5 : 1

    // The top bar keeps its own colors: in light mode it stays dark with light text and fades into the page.
    Color topBarFill   = Color::rgb(0x05070C);
    bool topBarSolid   = false;                  // light mode: solid up to the bar, then a short fade
    Color topBarText   = Color::rgb(0xFFFFFF);
    Color topBarMuted  = Color::rgb(0x9AA4C0);
    Color topBarFocus  = Color::rgb(0xB9D5FF);
};

}  // namespace ui
