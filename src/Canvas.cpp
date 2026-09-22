#include "Canvas.h"

#include <cmath>
#include <cstring>

namespace {

inline uint32_t blend(uint32_t dst, Color src, uint32_t alpha) {
    if (alpha == 0) return dst;
    if (alpha >= 255) return src.argb() | 0xFF000000u;

    const uint32_t inv = 255 - alpha;
    const uint32_t dr = (dst >> 16) & 0xFF;
    const uint32_t dg = (dst >> 8) & 0xFF;
    const uint32_t db = dst & 0xFF;

    const uint32_t r = (src.r * alpha + dr * inv) / 255;
    const uint32_t g = (src.g * alpha + dg * inv) / 255;
    const uint32_t b = (src.b * alpha + db * inv) / 255;

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

} // namespace

Canvas::Canvas(int width, int height)
    : width_(width), height_(height), pixels_(size_t(width) * height, 0xFF000000u) {
    clipStack_.push_back({0, 0, width, height});
}

void Canvas::pushClip(const Rect &r) { clipStack_.push_back(clip().intersect(r)); }

void Canvas::popClip() {
    if (clipStack_.size() > 1) clipStack_.pop_back();
}

void Canvas::clear(Color c) { fillRect(bounds(), c); }

void Canvas::markDamage(const Rect &r) {
    const Rect t = r.intersect({0, 0, width_, height_});
    if (t.empty()) return;

    // A short list of merged rectangles beats an exact region: merging costs a few wasted
    // pixels, tracking hundreds of rectangles costs more than it saves.
    constexpr size_t kMaxRects = 12;

    for (Rect &existing : damage_) {
        const Rect grown{existing.x - 8, existing.y - 8, existing.w + 16, existing.h + 16};
        if (!grown.intersect(t).empty()) {
            const int x = std::min(existing.x, t.x);
            const int y = std::min(existing.y, t.y);
            existing = {x, y, std::max(existing.right(), t.right()) - x,
                        std::max(existing.bottom(), t.bottom()) - y};
            return;
        }
    }

    if (damage_.size() < kMaxRects) {
        damage_.push_back(t);
        return;
    }

    // Full: fold the newcomer into whichever rectangle grows least by taking it.
    size_t best = 0;
    long bestCost = -1;
    for (size_t i = 0; i < damage_.size(); ++i) {
        const Rect &e = damage_[i];
        const int x = std::min(e.x, t.x);
        const int y = std::min(e.y, t.y);
        const long cost = long(std::max(e.right(), t.right()) - x) *
                          (std::max(e.bottom(), t.bottom()) - y) - long(e.w) * e.h;
        if (bestCost < 0 || cost < bestCost) { bestCost = cost; best = i; }
    }

    Rect &e = damage_[best];
    const int x = std::min(e.x, t.x);
    const int y = std::min(e.y, t.y);
    e = {x, y, std::max(e.right(), t.right()) - x, std::max(e.bottom(), t.bottom()) - y};
}

void Canvas::restoreFrom(const Canvas &background, const Rect &r) {
    const Rect t = r.intersect(clip());
    if (t.empty() || background.width() != width_ || background.height() != height_) return;

    for (int y = t.y; y < t.bottom(); ++y) {
        std::memcpy(&pixels_[size_t(y) * width_ + t.x], background.row(y) + t.x,
                    size_t(t.w) * 4);
    }
    markDamage(t);
}

void Canvas::blendSpan(int x, int y, int count, Color c) {
    uint32_t *p = &pixels_[size_t(y) * width_ + x];
    if (c.a >= 255) {
        const uint32_t v = c.argb() | 0xFF000000u;
        for (int i = 0; i < count; ++i) p[i] = v;
    } else {
        for (int i = 0; i < count; ++i) p[i] = blend(p[i], c, c.a);
    }
}

void Canvas::blendPixel(int x, int y, Color c, uint8_t coverage) {
    const Rect &cl = clip();
    if (!cl.contains(x, y)) return;
    const uint32_t alpha = uint32_t(c.a) * coverage / 255;
    uint32_t &p = pixels_[size_t(y) * width_ + x];
    p = blend(p, c, alpha);
}

void Canvas::fillRect(const Rect &r, Color c) {
    if (c.a == 0) return;
    const Rect t = r.intersect(clip());
    if (t.empty()) return;
    for (int y = t.y; y < t.bottom(); ++y) blendSpan(t.x, y, t.w, c);
    markDamage(t);
}

namespace {

// Signed distance to a rounded rect: negative inside. Gives both fill and outline
// antialiasing from one formula.
inline float roundedRectDistance(float px, float py, const Rect &r, float rad) {
    const float cx = r.x + r.w * 0.5f;
    const float cy = r.y + r.h * 0.5f;
    const float qx = std::fabs(px - cx) - (r.w * 0.5f - rad);
    const float qy = std::fabs(py - cy) - (r.h * 0.5f - rad);
    const float ax = qx > 0 ? qx : 0.0f;
    const float ay = qy > 0 ? qy : 0.0f;
    const float outside = std::sqrt(ax * ax + ay * ay);
    const float inside = std::min(std::max(qx, qy), 0.0f);
    return outside + inside - rad;
}

inline uint8_t coverageFrom(float distance) {
    const float cov = 0.5f - distance;
    if (cov <= 0) return 0;
    if (cov >= 1) return 255;
    return uint8_t(cov * 255.0f);
}

} // namespace

void Canvas::fillRoundedRect(const Rect &r, int radius, Color c) {
    if (c.a == 0 || r.empty()) return;

    const int rad = std::max(0, std::min(radius, std::min(r.w, r.h) / 2));
    if (rad == 0) { fillRect(r, c); return; }

    markDamage(r.intersect(clip()));

    // The middle band has no curvature, so only the corner bands need per-pixel work.
    fillRect({r.x, r.y + rad, r.w, r.h - 2 * rad}, c);

    const Rect cl = clip();
    for (int band = 0; band < 2; ++band) {
        const int y0 = band ? r.bottom() - rad : r.y;
        const Rect strip = Rect{r.x, y0, r.w, rad}.intersect(cl);
        if (strip.empty()) continue;

        for (int y = strip.y; y < strip.bottom(); ++y) {
            // Straight section between the two corners of this row.
            const Rect mid = Rect{r.x + rad, y, r.w - 2 * rad, 1}.intersect(cl);
            if (!mid.empty()) blendSpan(mid.x, mid.y, mid.w, c);

            for (int x = strip.x; x < strip.right(); ++x) {
                if (x >= r.x + rad && x < r.right() - rad) continue;
                const uint8_t cov = coverageFrom(
                    roundedRectDistance(x + 0.5f, y + 0.5f, r, float(rad)));
                if (cov) blendPixel(x, y, c, cov);
            }
        }
    }
}

void Canvas::strokeRoundedRect(const Rect &r, int radius, int thickness, Color c) {
    if (thickness <= 0 || c.a == 0 || r.empty()) return;

    const int rad = std::max(0, std::min(radius, std::min(r.w, r.h) / 2));
    const float half = thickness * 0.5f;
    const Rect area = Rect{r.x - thickness, r.y - thickness,
                           r.w + 2 * thickness, r.h + 2 * thickness}.intersect(clip());
    if (area.empty()) return;
    markDamage(area);

    // Only the ring can be covered, so the interior is skipped instead of distance-tested.
    const int skip = thickness + rad + 1;
    const Rect hollow{r.x + skip, r.y + skip, r.w - 2 * skip, r.h - 2 * skip};

    for (int y = area.y; y < area.bottom(); ++y) {
        const bool insideRows = !hollow.empty() && y >= hollow.y && y < hollow.bottom();
        for (int x = area.x; x < area.right(); ++x) {
            if (insideRows && x >= hollow.x && x < hollow.right()) {
                x = hollow.right() - 1;
                continue;
            }
            const float d = roundedRectDistance(x + 0.5f, y + 0.5f, r, float(rad));
            const uint8_t cov = coverageFrom(std::fabs(d) - half);
            if (cov) blendPixel(x, y, c, cov);
        }
    }
}

void Canvas::verticalGradient(const Rect &r, Color top, Color bottom) {
    const Rect t = r.intersect(clip());
    if (t.empty()) return;
    for (int y = t.y; y < t.bottom(); ++y) {
        const float f = (r.h > 1) ? float(y - r.y) / (r.h - 1) : 0.0f;
        blendSpan(t.x, y, t.w, Color::lerp(top, bottom, f));
    }
    markDamage(t);
}

Rect Canvas::shadowBounds(const Rect &r, int spread) {
    if (spread <= 0) return r;
    // Mirrors the outermost layer below, which is offset downwards by half the spread.
    const int grow = spread;
    return {r.x - grow, r.y - grow + grow / 2, r.w + 2 * grow, r.h + 2 * grow};
}

void Canvas::dropShadow(const Rect &r, int radius, int spread, Color c) {
    if (spread <= 0 || c.a == 0) return;

    // Each layer alpha-blends, so the count is fixed rather than tied to the spread, and only
    // the area outside `r` is touched — the caller paints an opaque body over the rest.
    constexpr int kLayers = 4;

    for (int i = kLayers; i >= 1; --i) {
        const float f = float(i) / kLayers;
        const int grow = std::max(1, int(spread * f));
        const uint8_t a = uint8_t(std::min(255.0f, c.a * (1.0f - f + 0.25f) * 0.6f));
        if (!a) continue;

        const Rect outer{r.x - grow, r.y - grow + grow / 2, r.w + 2 * grow, r.h + 2 * grow};
        const Color layer = c.withAlpha(a);
        const float rad = float(radius + grow);

        const Rect area = outer.intersect(clip());
        if (area.empty()) continue;
        markDamage(area);

        for (int y = area.y; y < area.bottom(); ++y) {
            const bool insideRows = y >= r.y && y < r.bottom();
            for (int x = area.x; x < area.right(); ++x) {
                if (insideRows && x >= r.x && x < r.right()) {
                    x = r.right() - 1;  // jump over the area the body will cover
                    continue;
                }
                const uint8_t cov =
                    coverageFrom(roundedRectDistance(x + 0.5f, y + 0.5f, outer, rad));
                if (cov) blendPixel(x, y, layer, cov);
            }
        }
    }
}

void Canvas::drawImage(const Image &img, const Rect &dst, uint8_t alpha, int radius) {
    if (!img.valid() || dst.empty() || alpha == 0) return;

    const Rect t = dst.intersect(clip());
    if (t.empty()) return;
    markDamage(t);

    const int rad = std::max(0, std::min(radius, std::min(dst.w, dst.h) / 2));

    for (int y = t.y; y < t.bottom(); ++y) {
        const int sy = int(long(y - dst.y) * img.height() / dst.h);
        const uint32_t *src = img.row(std::min(sy, img.height() - 1));
        uint32_t *out = &pixels_[size_t(y) * width_ + t.x];

        for (int i = 0; i < t.w; ++i) {
            const int x = t.x + i;
            const int sx = int(long(x - dst.x) * img.width() / dst.w);
            const uint32_t p = src[std::min(sx, img.width() - 1)];

            uint32_t a = ((p >> 24) & 0xFF) * alpha / 255;
            if (!a) continue;

            if (rad > 0) {
                // Corner coverage, so rounded artwork does not show square edges.
                const int lx = x - dst.x, ly = y - dst.y;
                const int cx = (lx < rad) ? rad : (lx >= dst.w - rad ? dst.w - rad - 1 : lx);
                const int cy = (ly < rad) ? rad : (ly >= dst.h - rad ? dst.h - rad - 1 : ly);
                if (cx != lx || cy != ly) {
                    const float d = std::sqrt(float((lx - cx) * (lx - cx) + (ly - cy) * (ly - cy)));
                    const float cov = float(rad) + 0.5f - d;
                    if (cov <= 0) continue;
                    if (cov < 1.0f) a = uint32_t(a * cov);
                }
            }

            // Boxart is normally fully opaque; storing beats blending for those pixels.
            if (a >= 255) {
                out[i] = p | 0xFF000000u;
                continue;
            }

            const Color c{uint8_t((p >> 16) & 0xFF), uint8_t((p >> 8) & 0xFF), uint8_t(p & 0xFF), 255};
            out[i] = blend(out[i], c, a);
        }
    }
}
