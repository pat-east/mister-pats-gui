#pragma once

#include <string>
#include <vector>

#include "ui-framework/core/Image.h"

namespace ui {

// A small SVG renderer for icons. It reads the subset icon sets use: <svg viewBox>, <path d fill-rule>,
// <circle> and <rect>, filled (no strokes, gradients or transforms). Paths support every command,
// relative and absolute, including arcs. It rasterizes to a coverage mask, so the same icon can be drawn
// in any color and at any size by any renderer, and a renderer that likes vectors can use the contours.
class SvgIcon {
public:
    // Parses SVG text. Returns false when nothing drawable was found.
    bool parse(const std::string &svg);

    float viewWidth() const { return viewW_; }
    float viewHeight() const { return viewH_; }
    bool empty() const { return shapes_.empty(); }

    // Rasterizes into a white image whose alpha is the coverage, fitted into w x h (aspect kept,
    // centered), anti-aliased by supersampling. The result is a mask: draw it with Painter::drawMask.
    ImagePtr rasterize(int w, int h) const;

private:
    struct Point { float x, y; };
    struct Shape {
        std::vector<std::vector<Point>> contours;   // closed polylines in view-box units
        bool evenOdd = false;
    };

    std::vector<Shape> shapes_;
    float viewW_ = 16.0f, viewH_ = 16.0f;
};

}  // namespace ui
