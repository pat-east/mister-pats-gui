#pragma once

#include "ui-framework/core/Color.h"
#include "ui-framework/kit/Buttons.h"
#include "ui-framework/kit/Palette.h"
#include "ui-framework/kit/Palettes.h"
#include "ui-framework/kit/Tokens.h"

namespace ui {

// Scale, text roles and the palette. Everything a component needs to decide how big and what color.
class Theme {
public:
    Theme(int screenWidth, int screenHeight) : width_(screenWidth), height_(screenHeight) {
        themes_ = builtinColorThemes(themeCount_);
        applyPalette();
    }

    Palette palette;

    // Dark or light mode, the color theme (an index into the application's themes, or the built-in set) and
    // high contrast with its highlight color. Any change rebuilds the palette; components read it next frame.
    void setDarkMode(bool dark) { dark_ = dark; applyPalette(); }
    void setColorTheme(int index) { if (index >= 0 && index < themeCount_) { colorTheme_ = index; applyPalette(); } }
    void setHighContrast(bool on, int highlight = kDefaultHighlight) {
        highContrast_ = on;
        highlight_ = highlight;
        applyPalette();
    }
    void setColorThemes(const ColorTheme *themes, int count) { themes_ = themes; themeCount_ = count; colorTheme_ = 0; applyPalette(); }
    // The face-button layout (Xbox, PlayStation, Nintendo): what hints, the action glyph and button placeholders
    // in texts print. The application sets it from the user's choice; it changes no behavior (src/ui-framework/kit/Buttons.h).
    void setButtonLayout(ButtonLayout layout) { buttonLayout_ = layout; }
    ButtonLayout buttonLayout() const { return buttonLayout_; }
    bool darkMode() const { return dark_; }
    int colorTheme() const { return colorTheme_; }
    int colorThemeCount() const { return themeCount_; }
    const char *colorThemeName(int index) const { return themes_[index].name; }
    bool highContrast() const { return highContrast_; }
    int highlight() const { return highlight_; }
    // High contrast with a highlight other than white also draws plain borders around rows and fields.
    bool borders() const { return palette.borders; }

    int screenWidth() const { return width_; }
    int screenHeight() const { return height_; }

    // Design pixels to real pixels; at least one pixel for any positive value.
    int px(int designPx) const {
        const int scaled = int(long(designPx) * height_ / 1080);
        return (designPx > 0 && scaled < 1) ? 1 : scaled;
    }

    // Text size setting (100 / 115 / 130 %) and the separate hint size setting.
    void setTextScale(int percent) { textScale_ = percent; }
    void setHintScale(int percent) { hintScale_ = percent; }
    int textScale() const { return textScale_; }

    int size(TextRole role) const {
        switch (role) {
            case TextRole::Title:   return fontPx(48);
            case TextRole::Heading: return fontPx(36);
            case TextRole::Clock:   return fontPx(33);
            case TextRole::Tab:     return fontPx(27);
            case TextRole::Body:    return fontPx(21);
            case TextRole::Small:   return fontPx(17);
            case TextRole::Hint:    return px(17 * textScale_ * hintScale_ / 10000);
        }
        return fontPx(21);
    }

    // Text tiers replace the scattered alpha values: Faint is the floor for text people must read.
    Color text(TextTier tier) const {
        switch (tier) {
            case TextTier::Primary:  return palette.textPrimary;
            case TextTier::Muted:    return palette.textMuted;
            case TextTier::Faint:    return palette.textFaint;
            case TextTier::Disabled: return palette.textDisabled;
        }
        return palette.textPrimary;
    }

    Color tone(Tone t) const {
        switch (t) {
            case Tone::Neutral: return palette.textMuted;
            case Tone::Done:    return palette.accentText;
            case Tone::Action:  return palette.accentText;
            case Tone::Warning: return palette.warning;
            case Tone::Error:   return palette.error;
        }
        return palette.textMuted;
    }

    // Layout metrics of the main-screen layout.
    int marginX() const { return px(72); }
    int marginY() const { return px(32); }
    int topBarHeight() const { return px(96); }
    int bottomBarHeight() const { return px(72); }
    int gap() const { return px(Space::gap); }

private:
    void applyPalette() {
        int count = 0;
        const HighlightColor *colors = highlightColors(count);
        const int h = highlight_ >= 0 && highlight_ < count ? highlight_ : kDefaultHighlight;
        palette = highContrast_ ? makeHighContrastPalette(colors[h]) : makePalette(themes_[colorTheme_], dark_);
    }

    int fontPx(int designPx) const { return px(designPx * textScale_ / 100); }

    const ColorTheme *themes_ = nullptr;
    int themeCount_ = 0;
    int colorTheme_ = 0;
    int highlight_ = kDefaultHighlight;
    bool dark_ = true;
    bool highContrast_ = false;
    ButtonLayout buttonLayout_ = ButtonLayout::Xbox;

    int width_;
    int height_;
    int textScale_ = 100;
    int hintScale_ = 100;
};

}  // namespace ui
