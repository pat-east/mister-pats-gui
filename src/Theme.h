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

    // Colours
    Color background   = Color::rgb(0x0B0D14);
    Color backgroundLo = Color::rgb(0x05070C);
    Color surface      = Color::rgb(0x161A26);
    Color surfaceHi    = Color::rgb(0x222839);
    Color accent       = Color::rgb(0x4C8DFF);
    Color textPrimary  = Color::rgb(0xFFFFFF);
    Color textMuted    = Color::rgb(0x9AA4C0);
    Color warning      = Color::rgb(0xE06A5A);
    Color favorite     = Color::rgb(0xFFC65C);
    Color shadow       = Color::rgb(0x000000);

    // Metrics, in design pixels
    int marginX() const { return px(72); }
    int marginY() const { return px(32); }
    int topBarHeight() const { return px(96); }
    int bottomBarHeight() const { return px(72); }
    int radius() const { return px(10); }
    int gap() const { return px(26); }

    // Font sizes
    int sizeHeading() const { return px(34); }
    int sizeTab() const { return px(25); }
    int sizeBody() const { return px(19); }
    int sizeSmall() const { return px(15); }
    int sizeClock() const { return px(31); }
    int sizeTitle() const { return px(46); }

private:
    int width_;
    int height_;
    Font regular_;
    Font bold_;
};
