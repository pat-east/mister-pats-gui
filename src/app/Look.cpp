#include "app/Look.h"

namespace patsgui {

void applyLook(ui::Theme &theme, const LookSettings &look) {
    theme.setButtonLayout(look.buttonLayout);
    theme.setTextScale(look.textScale);
    theme.setHintScale(look.hintScale);
    theme.setDarkMode(look.dark);
    // setColorTheme ignores an index the theme does not have; show the framework default then, not the last one.
    theme.setColorTheme(look.colorTheme >= 0 && look.colorTheme < theme.colorThemeCount() ? look.colorTheme : 0);
    theme.setHighContrast(look.highContrast, look.highlight);
}

namespace {

void loadFont(ui::Font &font, const std::string &path) {
    std::vector<std::string> candidates;
    if (!path.empty()) candidates.push_back(path);
    font.load(candidates);
    font.setScalePercent(path.find("SpaceGrotesk-") != std::string::npos ? 90 : 100);
}

}  // namespace

void applyLookFonts(ui::Font &regular, ui::Font &bold, const LookSettings &look) {
    loadFont(regular, look.regularFontPath);
    loadFont(bold, look.boldFontPath);
}

}  // namespace patsgui
