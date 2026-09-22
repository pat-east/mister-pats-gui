#pragma once

#include <cstdint>
#include <vector>

#include "Color.h"
#include "Geometry.h"
#include "Image.h"

// Software drawing surface in ARGB32. All primitives honour the current clip rect.
class Canvas {
public:
    Canvas(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    Rect bounds() const { return {0, 0, width_, height_}; }

    const uint32_t *row(int y) const { return &pixels_[size_t(y) * width_]; }

    void pushClip(const Rect &r);
    void popClip();
    const Rect &clip() const { return clipStack_.back(); }

    // Damage tracking. Every primitive records where it painted, so only those areas need
    // to be pushed to the framebuffer — a full 1080p frame costs ~26 ms in memory traffic
    // alone, which is most of a 30 fps budget.
    void clearDamage() { damage_.clear(); }
    void markDamage(const Rect &r);
    const std::vector<Rect> &damage() const { return damage_; }
    bool fullyDamaged() const { return damage_.size() == 1 && damage_[0].w >= width_ &&
                                       damage_[0].h >= height_; }

    // Copies a region back from a prepared background, so a changed element can be redrawn
    // without repainting the whole screen.
    void restoreFrom(const Canvas &background, const Rect &r);

    void clear(Color c);
    void fillRect(const Rect &r, Color c);
    void fillRoundedRect(const Rect &r, int radius, Color c);
    void strokeRoundedRect(const Rect &r, int radius, int thickness, Color c);
    void verticalGradient(const Rect &r, Color top, Color bottom);

    // Layered rounded rects standing in for a blur: cheap, and good enough at these sizes.
    void dropShadow(const Rect &r, int radius, int spread, Color c);

    // The largest area dropShadow can touch. Callers that redraw incrementally need
    // this to know how much background to restore — guessing it leaves debris.
    static Rect shadowBounds(const Rect &r, int spread);

    void drawImage(const Image &img, const Rect &dst, uint8_t alpha = 255, int radius = 0);

    // Coverage is 0..255; used by the glyph rasteriser.
    void blendPixel(int x, int y, Color c, uint8_t coverage);

private:
    void blendSpan(int x, int y, int count, Color c);

    int width_;
    int height_;
    std::vector<uint32_t> pixels_;
    std::vector<Rect> clipStack_;
    std::vector<Rect> damage_;
};
