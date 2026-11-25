#pragma once

// The look of the new GUI as plain values, and the one place that applies it to the framework theme and fonts.
// Classic keeps these settings in Preferences (text size, hint size, high contrast and its color, color theme, dark
// mode) and in the Classic Theme's resolved font files (Space Grotesk, enabled and installed, else none); the hosts
// (src/legacy/bridge/PatsGuiScreen, src/legacy/bridge/WireframesScreen, the host runner src/app/host) read them into a LookSettings and call these
// functions, so the new GUI follows the settings at open and at every change, from any screen that changed them.
// Application-agnostic apart from the struct's name: it takes plain numbers and file paths.

#include <string>
#include <vector>

#include "ui-framework/kit/Theme.h"
#include "ui-framework/render/soft/Font.h"

namespace patsgui {

struct LookSettings {
    int textScale = 100;               // 100, 115, 130 (percent)
    int hintScale = 100;               // 100, 115, 130
    bool highContrast = false;
    int highlight = ui::kDefaultHighlight;   // the high contrast color, 0..7; always passed together with highContrast
    // The color theme as an index into the framework theme's list (Classic's Preferences index, same order for the 8
    // built-in themes) and dark mode. An index the theme does not have shows theme 0 (the framework default).
    int colorTheme = 0;
    bool dark = true;
    // The face-button layout of the controller (Classic's Preferences::faceButtonStyle): what hints and the action glyph
    // print. Changes no behavior.
    ui::ButtonLayout buttonLayout = ui::ButtonLayout::Xbox;
    // The font files the GUI shows: the Classic Theme's resolved paths. Empty = the built-in face.
    std::string regularFontPath, boldFontPath;

    bool sameFonts(const LookSettings &o) const {
        return regularFontPath == o.regularFontPath && boldFontPath == o.boldFontPath;
    }
    bool sameTheme(const LookSettings &o) const {
        return textScale == o.textScale && hintScale == o.hintScale && highContrast == o.highContrast &&
               highlight == o.highlight && colorTheme == o.colorTheme && dark == o.dark &&
               buttonLayout == o.buttonLayout;
    }
    bool operator==(const LookSettings &o) const { return sameTheme(o) && sameFonts(o); }
    bool operator!=(const LookSettings &o) const { return !(*this == o); }
};

// Sets button layout, text scale, hint scale, dark mode, color theme and high contrast (with its highlight, also when it is off:
// the color is part of the same call). Pure: no state besides `theme`.
void applyLook(ui::Theme &theme, const LookSettings &look);

// Loads the look's font files into `regular` and `bold` in place (the glyph caches are cleared, so painters that
// hold the references stay valid) and sets the glyph scale: 90 % for Space Grotesk files (the same rule as the
// Classic Theme), 100 % otherwise. An empty path leaves the built-in face, as Classic does.
void applyLookFonts(ui::Font &regular, ui::Font &bold, const LookSettings &look);

}  // namespace patsgui
