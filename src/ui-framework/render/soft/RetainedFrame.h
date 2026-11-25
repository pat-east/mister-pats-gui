#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui-framework/core/Painter.h"
#include "ui-framework/render/soft/Canvas.h"
#include "ui-framework/render/soft/Font.h"

namespace ui {

// A Painter that does not draw: it records the frame's draw calls (a display list) and, in commit(), paints only what
// changed since the previous frame onto a persistent Canvas. The result is pixel-identical to drawing the whole frame
// with SoftPainter, whatever the components draw (no component knows about it).
//
// How: the screen is cut into 32 px tiles. For every tile the frame keeps one hash over the ORDERED list of draw calls
// that can paint into it (kind, geometry, colours, text, font size, clip, image identity). A tile whose hash equals the
// previous frame's shows the same pixels and is left alone; the others are repainted by replaying every call that touches
// them, clipped to the dirty rectangles (so alpha scrims and shadows are never applied twice). The page behind a dialog
// is therefore not repainted while only the dialog changes, and a moving cursor repaints only its old and new place.
//
// What the hash cannot see (the caller must call invalidate()): a font file, theme or any state that changes how an
// unchanged call renders; an Image whose pixels change in place (images are keyed by Image::id()); drawing outside this
// painter on the same canvas (the video picture, see src/app/README.md "Rendering and damage").
class RetainedFrame : public Painter {
public:
    RetainedFrame(int width, int height, Font &regular, Font &bold);

    // Starts a frame: forgets the recorded calls (the previous frame's tile hashes stay for the comparison).
    void begin();
    // Paints the changed tiles onto `canvas` (same size as constructed) and returns the rectangles it painted (disjoint; the
    // canvas's damage list holds them too). Empty: nothing changed.
    const std::vector<Rect> &commit(Canvas &canvas);
    // The next commit() repaints everything (a look or font change, a canvas that was drawn on elsewhere).
    void invalidate() { valid_ = false; }

    struct Stats {
        int calls = 0;           // draw calls recorded in the frame
        int replayed = 0;        // calls (re)drawn: one per call and dirty rectangle it touches
        int dirtyTiles = 0, tiles = 0;
        long dirtyPixels = 0;    // area of the painted rectangles
        bool full = false;       // everything was repainted
    };
    const Stats &stats() const { return stats_; }

    // ---- Painter
    Rect bounds() const override { return {0, 0, width_, height_}; }
    Capabilities caps() const override { Capabilities c; c.damageTracking = true; return c; }
    void pushClip(const Rect &r) override { clips_.push_back(clips_.back().intersect(r)); }
    void popClip() override { if (clips_.size() > 1) clips_.pop_back(); }
    void clear(Color c) override;
    void fillRect(const Rect &r, Color c) override;
    void fillRoundedRect(const Rect &r, int radius, Color c) override;
    void strokeRoundedRect(const Rect &r, int radius, int thickness, Color c) override;
    void fillGradient(const Rect &r, Color top, Color bottom) override;
    void drawLine(int x0, int y0, int x1, int y1, int thickness, Color c) override;
    void drawImage(const Image &img, const Rect &dst, uint8_t alpha, int radius) override;
    void drawMask(const Image &mask, int x, int y, Color c) override;
    void drawText(FontRole role, int px, int x, int y, const std::string &text, Color c) override;
    int lineHeight(FontRole role, int px) override { return font(role).lineHeight(px); }
    int measure(FontRole role, int px, const std::string &text) override { return font(role).measure(text, px); }
    std::string elide(FontRole role, int px, const std::string &text, int maxWidth) override {
        return font(role).elide(text, px, maxWidth);
    }

private:
    enum class Kind : uint8_t { Clear, Fill, Rounded, Stroke, Gradient, Line, Image, Mask, Text };
    struct Op {
        explicit Op(Kind k) : kind(k) {}
        Kind kind;
        FontRole role = FontRole::Regular;
        Rect box;            // what the call can paint (clipped)
        Rect clip;           // the clip in force
        Rect r;              // the call's rectangle (or x, y, w, h of a mask/text origin)
        int a = 0, b = 0, c2 = 0, d = 0;
        Color color, color2;
        const Image *image = nullptr;
        uint32_t textAt = 0, textLen = 0;
        uint64_t hash = 0;
    };
    Font &font(FontRole role) { return role == FontRole::Bold ? bold_ : regular_; }
    void add(Op &op, const Rect &paints);
    void replay(Canvas &canvas, const Op &op, const Rect &area);

    int width_, height_, tilesX_, tilesY_;
    Font &regular_;
    Font &bold_;
    std::vector<Rect> clips_;
    std::vector<Op> ops_;
    std::string text_;
    std::vector<uint64_t> current_, previous_;
    bool valid_ = false;   // previous_ describes what the canvas shows
    std::vector<Rect> damage_;
    Stats stats_;
};

}  // namespace ui
