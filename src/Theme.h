#pragma once

#include "Color.h"
#include "Font.h"

// Look and metrics in one place. All sizes are authored against a 1080p canvas and scaled
// to the actual framebuffer, so 480p and 4K both stay proportional.
class Theme {
public:
    Theme(int screenWidth, int screenHeight);

    int px(int designPx) const;

    int screenWidth() const { return width_; }
    int screenHeight() const { return height_; }

    Font &regular() { return regular_; }
    Font &bold() { return bold_; }
    void setSpaceGroteskEnabled(bool enabled);
    bool spaceGroteskEnabled() const { return spaceGroteskEnabled_; }
    bool spaceGroteskActive() const;
    static bool spaceGroteskAvailable();
    void setFontSizeScale(int percent) { fontSizeScalePercent_ = percent; }
    void setHintSizeScale(int percent) { hintSizeScalePercent_ = percent; }
    void setHighContrast(bool enabled);
    // Which color High contrast uses for focus, progress, hints and other highlights.
    static constexpr int kHighContrastColors = 8;
    static constexpr int kDefaultHighContrastColor = 2; // Yellow
    static constexpr int kWhiteHighContrastColor = 0;
    // High contrast adds plain white borders around fields and list rows. With White as the
    // highlight color they would be indistinguishable from the focus outline, so they are left
    // out then.
    bool highContrastBorders() const {
        return highContrast_ && highContrastColor_ != kWhiteHighContrastColor;
    }
    static const char *highContrastColorName(int index);
    void setHighContrastColor(int index);
    bool highContrast() const { return highContrast_; }

    // Colours
    Color background   = Color::rgb(0x0B0D14);
    Color backgroundLo = Color::rgb(0x05070C);
    Color surface      = Color::rgb(0x161A26);
    Color surfaceHi    = Color::rgb(0x222839);
    Color accent       = Color::rgb(0x4C8DFF);
    Color textPrimary  = Color::rgb(0xFFFFFF);
    Color textMuted    = Color::rgb(0x9AA4C0);
    Color warning      = Color::rgb(0xE06A5A);
    Color favorite     = Color::rgb(0x4C8DFF);
    Color shadow       = Color::rgb(0x000000);
    Color focus        = Color::rgb(0xB9D5FF);

    // Metrics, in design pixels
    int marginX() const { return px(72); }
    int marginY() const { return px(32); }
    int topBarHeight() const { return px(96); }
    int bottomBarHeight() const { return px(72); }
    int radius() const { return px(10); }
    int gap() const { return px(26); }

    // Font sizes
    int sizeHeading() const { return fontPx(36); }
    int sizeTab() const { return fontPx(27); }
    int sizeBody() const { return fontPx(21); }
    int sizeSmall() const { return fontPx(17); }
    // Controller hints at the bottom left: small text with its own 100/115/130 % setting on top of
    // the general text size.
    int sizeHint() const { return px(17 * fontSizeScalePercent_ * hintSizeScalePercent_ / 10000); }
    int hintIconSize() const { return px(18 * hintSizeScalePercent_ / 100); }
    int sizeClock() const { return fontPx(33); }
    int sizeTitle() const { return fontPx(48); }
    int sizeGameLabel() const { return regular_.usingTrueType() ? sizeBody() : fontPx(18); }
    int sizeGameDetail() const { return bold_.usingTrueType() ? sizeTitle() : fontPx(34); }

private:
    int width_;
    int height_;
    int fontSizeScalePercent_ = 100;
    int hintSizeScalePercent_ = 100;
    bool highContrast_ = false;
    int highContrastColor_ = kDefaultHighContrastColor;
    void applyPalette();
    bool spaceGroteskEnabled_ = true;
    Font regular_;
    Font bold_;
    void loadFonts();
    int fontPx(int designPx) const { return px(designPx * fontSizeScalePercent_ / 100); }
};
