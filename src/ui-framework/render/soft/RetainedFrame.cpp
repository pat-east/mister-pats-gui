#include "ui-framework/render/soft/RetainedFrame.h"

#include <algorithm>
#include <cstring>

#include "ui-framework/render/soft/SoftPainter.h"

namespace ui {

namespace {

constexpr int kTile = 32;

inline uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 32;
    return h;
}
inline uint64_t mixRect(uint64_t h, const Rect &r) {
    h = mix(h, uint64_t(uint32_t(r.x)) << 32 | uint32_t(r.y));
    return mix(h, uint64_t(uint32_t(r.w)) << 32 | uint32_t(r.h));
}
inline uint32_t packed(Color c) { return uint32_t(c.r) << 24 | uint32_t(c.g) << 16 | uint32_t(c.b) << 8 | c.a; }

}  // namespace

RetainedFrame::RetainedFrame(int width, int height, Font &regular, Font &bold)
    : width_(width), height_(height), tilesX_((width + kTile - 1) / kTile), tilesY_((height + kTile - 1) / kTile),
      regular_(regular), bold_(bold) {
    current_.assign(size_t(tilesX_) * tilesY_, 0);
    previous_ = current_;
    clips_.push_back(bounds());
}

void RetainedFrame::begin() {
    ops_.clear();
    text_.clear();
    clips_.assign(1, bounds());
    std::fill(current_.begin(), current_.end(), 0x1234567ull);
}

void RetainedFrame::add(Op &op, const Rect &paints) {
    op.clip = clips_.back();
    op.box = paints.intersect(op.clip);
    if (op.box.empty()) return;   // paints nothing: it cannot change a pixel
    uint64_t h = mix(uint64_t(op.kind), packed(op.color));
    h = mix(h, packed(op.color2));
    h = mixRect(h, op.r);
    h = mixRect(h, op.clip);
    h = mix(h, uint64_t(uint32_t(op.a)) << 32 | uint32_t(op.b));
    h = mix(h, uint64_t(uint32_t(op.c2)) << 32 | uint32_t(op.d));
    h = mix(h, uint64_t(op.role));
    if (op.image) h = mix(h, op.image->id() ? op.image->id() : uint64_t(reinterpret_cast<uintptr_t>(op.image)));
    if (op.textLen) {
        uint64_t th = 1469598103934665603ull;
        for (uint32_t i = 0; i < op.textLen; ++i) th = (th ^ uint8_t(text_[op.textAt + i])) * 1099511628211ull;
        h = mix(h, th);
    }
    op.hash = h;
    const int x0 = op.box.x / kTile, x1 = (op.box.right() - 1) / kTile;
    const int y0 = op.box.y / kTile, y1 = (op.box.bottom() - 1) / kTile;
    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx) {
            uint64_t &t = current_[size_t(ty) * tilesX_ + tx];
            t = (t ^ h) * 1099511628211ull;
        }
    ops_.push_back(op);
}

void RetainedFrame::clear(Color c) {
    Op op(Kind::Clear);
    op.color = c;
    op.r = bounds();
    add(op, bounds());
}
void RetainedFrame::fillRect(const Rect &r, Color c) {
    if (c.a == 0) return;
    Op op(Kind::Fill);
    op.r = r; op.color = c;
    add(op, r);
}
void RetainedFrame::fillRoundedRect(const Rect &r, int radius, Color c) {
    if (c.a == 0 || r.empty()) return;
    Op op(Kind::Rounded);
    op.r = r; op.a = radius; op.color = c;
    add(op, r);
}
void RetainedFrame::strokeRoundedRect(const Rect &r, int radius, int thickness, Color c) {
    if (thickness <= 0 || c.a == 0 || r.empty()) return;
    Op op(Kind::Stroke);
    op.r = r; op.a = radius; op.b = thickness; op.color = c;
    add(op, Rect{r.x - thickness, r.y - thickness, r.w + 2 * thickness, r.h + 2 * thickness});
}
void RetainedFrame::fillGradient(const Rect &r, Color top, Color bottom) {
    Op op(Kind::Gradient);
    op.r = r; op.color = top; op.color2 = bottom;
    add(op, r);
}
void RetainedFrame::drawLine(int x0, int y0, int x1, int y1, int thickness, Color c) {
    const int pad = int((std::max(1, thickness) + 1) / 2) + 2;
    Op op(Kind::Line);
    op.r = Rect{x0, y0, x1, y1}; op.a = thickness; op.color = c;
    add(op, Rect{std::min(x0, x1) - pad, std::min(y0, y1) - pad, std::abs(x1 - x0) + 2 * pad + 1, std::abs(y1 - y0) + 2 * pad + 1});
}
void RetainedFrame::drawImage(const Image &img, const Rect &dst, uint8_t alpha, int radius) {
    if (!img.valid() || dst.empty() || alpha == 0) return;
    Op op(Kind::Image);
    op.r = dst; op.a = alpha; op.b = radius; op.image = &img;
    add(op, dst);
}
void RetainedFrame::drawMask(const Image &mask, int x, int y, Color c) {
    Op op(Kind::Mask);
    op.r = Rect{x, y, mask.width(), mask.height()}; op.color = c; op.image = &mask;
    add(op, op.r);
}
void RetainedFrame::drawText(FontRole role, int px, int x, int y, const std::string &text, Color c) {
    if (text.empty()) return;
    Op op(Kind::Text);
    op.role = role; op.a = px; op.color = c;
    const int w = font(role).measure(text, px), h = font(role).lineHeight(px);
    op.r = Rect{x, y, w, h};
    op.textAt = uint32_t(text_.size());
    op.textLen = uint32_t(text.size());
    text_ += text;
    // Glyphs may overhang the advance box (bearings, italics) and Font::draw marks lineHeight + size / 4 as its area.
    const int padX = px / 4 + 4, padTop = px / 4 + 2, padBottom = px / 3 + 3;
    add(op, Rect{x - padX, y - padTop, w + 2 * padX, h + padTop + padBottom});
}

void RetainedFrame::replay(Canvas &canvas, const Op &op, const Rect &area) {
    SoftPainter p(canvas, regular_, bold_);
    p.pushClip(area);
    p.pushClip(op.clip);
    switch (op.kind) {
        case Kind::Clear: p.clear(op.color); break;
        case Kind::Fill: p.fillRect(op.r, op.color); break;
        case Kind::Rounded: p.fillRoundedRect(op.r, op.a, op.color); break;
        case Kind::Stroke: p.strokeRoundedRect(op.r, op.a, op.b, op.color); break;
        case Kind::Gradient: p.fillGradient(op.r, op.color, op.color2); break;
        case Kind::Line: p.drawLine(op.r.x, op.r.y, op.r.w, op.r.h, op.a, op.color); break;
        case Kind::Image: p.drawImage(*op.image, op.r, uint8_t(op.a), op.b); break;
        case Kind::Mask: p.drawMask(*op.image, op.r.x, op.r.y, op.color); break;
        case Kind::Text: p.drawText(op.role, op.a, op.r.x, op.r.y, text_.substr(op.textAt, op.textLen), op.color); break;
    }
    p.popClip();
    p.popClip();
}

const std::vector<Rect> &RetainedFrame::commit(Canvas &canvas) {
    damage_.clear();
    stats_ = Stats();
    stats_.calls = int(ops_.size());
    stats_.tiles = tilesX_ * tilesY_;
    if (canvas.width() != width_ || canvas.height() != height_) { valid_ = false; }
    const bool all = !valid_;
    std::vector<uint8_t> dirty(current_.size(), 0);
    for (size_t i = 0; i < current_.size(); ++i)
        if (all || current_[i] != previous_[i]) { dirty[i] = 1; ++stats_.dirtyTiles; }
    stats_.full = all || stats_.dirtyTiles == stats_.tiles;
    if (stats_.dirtyTiles > 0) {
        // Row runs of dirty tiles, merged downwards while the next row has the identical run: disjoint rectangles.
        struct Run { int x0, x1, y0, y1; bool open; };
        std::vector<Run> open;
        std::vector<Rect> rects;
        auto emit = [&](const Run &r) {
            rects.push_back(Rect{r.x0 * kTile, r.y0 * kTile, std::min(width_, r.x1 * kTile) - r.x0 * kTile,
                                 std::min(height_, (r.y1 + 1) * kTile) - r.y0 * kTile});
        };
        for (int ty = 0; ty < tilesY_; ++ty) {
            std::vector<Run> next;
            for (int tx = 0; tx < tilesX_;) {
                if (!dirty[size_t(ty) * tilesX_ + tx]) { ++tx; continue; }
                int end = tx;
                while (end < tilesX_ && dirty[size_t(ty) * tilesX_ + end]) ++end;
                next.push_back(Run{tx, end, ty, ty, true});
                tx = end;
            }
            std::vector<Run> carried;
            for (Run &o : open) {
                bool extended = false;
                for (auto it = next.begin(); it != next.end(); ++it)
                    if (it->x0 == o.x0 && it->x1 == o.x1) { o.y1 = ty; carried.push_back(o); next.erase(it); extended = true; break; }
                if (!extended) emit(o);
            }
            for (const Run &n : next) carried.push_back(n);
            open.swap(carried);
        }
        for (const Run &o : open) emit(o);
        for (const Rect &area : rects) {
            for (const Op &op : ops_) {
                if (op.box.intersect(area).empty()) continue;
                replay(canvas, op, area);
                ++stats_.replayed;
            }
            stats_.dirtyPixels += long(area.w) * area.h;
            damage_.push_back(area);
            canvas.markDamage(area);
        }
    }
    previous_ = current_;
    valid_ = true;
    return damage_;
}

}  // namespace ui
