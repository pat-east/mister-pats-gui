#include "bridge/LookBridge.h"

int frameworkColorTheme(int classicIndex) {
    // The framework carries all of Classic's 22 color themes in Classic's order (src/ui-framework/kit/Palettes.cpp kThemes, the names say
    // the color instead of a brand), so the Preferences index is the framework index. An index the framework does not
    // have (a future Classic theme) shows the framework default.
    int count = 0;
    ui::builtinColorThemes(count);
    return classicIndex >= 0 && classicIndex < count ? classicIndex : -1;
}

patsgui::LookSettings classicLook(const Context &context) {
    patsgui::LookSettings look;
    const Preferences &prefs = context.preferences;
    look.textScale = prefs.fontSizeScale();
    look.hintScale = prefs.hintSizeScale();
    look.highContrast = prefs.highContrast();
    look.highlight = prefs.highContrastColor();
    look.colorTheme = frameworkColorTheme(prefs.colorTheme());
    look.dark = prefs.darkMode();
    look.buttonLayout = ui::buttonLayoutFromName(prefs.faceButtonStyle());   // "xbox" | "playstation" | "nintendo"
    look.regularFontPath = context.theme.regular().path();
    look.boldFontPath = context.theme.bold().path();
    return look;
}
