#pragma once

#include <string>

#include "ui-framework/core/Color.h"
#include "ui-framework/core/Geometry.h"
#include "ui-framework/core/Image.h"

namespace ui {

enum class FontRole { Regular, Bold };

// What a renderer can do beyond the basic primitives (see "Rendering is exchangeable").
struct Capabilities {
    bool damageTracking = false;   // the renderer can present only changed regions
    bool cheapEffects = false;     // shadows, scaling and zoom are cheap (never used for the look)
};

// Text measurement, eliding and wrapping use the same font on every renderer, so a layout is the
// same everywhere. The first implementation lives with the CPU renderer; a GPU renderer can reuse it.
class TextMeasurer {
public:
    virtual ~TextMeasurer() = default;
    virtual int lineHeight(FontRole role, int px) = 0;
    virtual int measure(FontRole role, int px, const std::string &text) = 0;
    virtual std::string elide(FontRole role, int px, const std::string &text, int maxWidth) = 0;
};

// The one drawing interface components use. Everything a component draws goes through here, so the
// renderer behind it (CPU today, GPU later) can change without touching a component.
class Painter : public TextMeasurer {
public:
    virtual Rect bounds() const = 0;
    virtual Capabilities caps() const = 0;

    virtual void pushClip(const Rect &r) = 0;
    virtual void popClip() = 0;

    virtual void clear(Color c) = 0;
    virtual void fillRect(const Rect &r, Color c) = 0;
    virtual void fillRoundedRect(const Rect &r, int radius, Color c) = 0;
    virtual void strokeRoundedRect(const Rect &r, int radius, int thickness, Color c) = 0;
    virtual void fillGradient(const Rect &r, Color top, Color bottom) = 0;
    virtual void drawLine(int x0, int y0, int x1, int y1, int thickness, Color c) = 0;
    virtual void drawImage(const Image &img, const Rect &dst, uint8_t alpha = 255, int radius = 0) = 0;
    // Draws a coverage mask (the alpha of the image) in one color, at its own size, top-left at dst.
    // Icons are masks: the same picture in any color.
    virtual void drawMask(const Image &mask, int x, int y, Color c) = 0;

    // `y` is the top of the line.
    virtual void drawText(FontRole role, int px, int x, int y, const std::string &text, Color c) = 0;
};

}  // namespace ui
