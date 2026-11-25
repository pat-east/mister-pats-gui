#include "ui-framework/render/soft/SoftPainter.h"

#include <algorithm>
#include <cmath>

namespace ui {

// An anti-aliased line of the given thickness: coverage by distance to the segment. Used for the few
// glyph icons; not a hot path.
void SoftPainter::drawLine(int x0, int y0, int x1, int y1, int thickness, Color c) {
    const float half = std::max(1, thickness) * 0.5f;
    const int pad = int(std::ceil(half)) + 1;
    const int minX = std::min(x0, x1) - pad, maxX = std::max(x0, x1) + pad;
    const int minY = std::min(y0, y1) - pad, maxY = std::max(y0, y1) + pad;
    const float dx = float(x1 - x0), dy = float(y1 - y0);
    const float len2 = dx * dx + dy * dy;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float t = len2 > 0 ? ((x - x0) * dx + (y - y0) * dy) / len2 : 0.0f;
            t = std::max(0.0f, std::min(1.0f, t));
            const float px = x0 + t * dx - x, py = y0 + t * dy - y;
            const float dist = std::sqrt(px * px + py * py);
            const float coverage = std::max(0.0f, std::min(1.0f, half + 0.5f - dist));
            if (coverage > 0.0f) canvas_.blendPixel(x, y, c, uint8_t(coverage * 255.0f));
        }
    }
    canvas_.markDamage({minX, minY, maxX - minX + 1, maxY - minY + 1});
}

// A mask in one color: the alpha of every pixel is the coverage. Used for icons (small images).
void SoftPainter::drawMask(const Image &mask, int x, int y, Color c) {
    for (int my = 0; my < mask.height(); ++my) {
        const uint32_t *row = mask.row(my);
        for (int mx = 0; mx < mask.width(); ++mx) {
            const uint32_t coverage = row[mx] >> 24;
            if (coverage) canvas_.blendPixel(x + mx, y + my, c, uint8_t(coverage * c.a / 255));
        }
    }
    canvas_.markDamage({x, y, mask.width(), mask.height()});
}

}  // namespace ui
