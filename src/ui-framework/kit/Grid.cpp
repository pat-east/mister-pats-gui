#include "ui-framework/kit/Grid.h"

#include <algorithm>

namespace ui {

void GridLayout::configure(Painter &measurer, const Theme &t, const Rect &area, const GridSpec &spec) {
    area_ = area;
    gap_ = t.gap();
    cellW_ = t.px(spec.artW + 12);
    bodyH_ = t.px(spec.artH + 12);
    // The caption lines are as tall as the design says (26 and 20) or as the text needs at a larger size.
    labelH_ = spec.caption == Caption::None ? 0
              : std::max(t.px(26), measurer.lineHeight(FontRole::Regular, t.size(TextRole::Body)) + t.px(2));
    subH_ = spec.caption == Caption::TitleSub
                ? std::max(t.px(20), measurer.lineHeight(FontRole::Regular, t.size(TextRole::Small)) + t.px(2)) : 0;
    captionGap_ = spec.caption == Caption::None ? 0 : t.px(6);
    cellH_ = bodyH_ + captionGap_ + labelH_ + subH_;
    columns_ = std::max(1, (area.w + gap_) / (cellW_ + gap_));
    visibleRows_ = std::max(1, (area.h + gap_) / (cellH_ + gap_));
}

Rect GridLayout::cell(int index) const {
    const int row = index / columns_, col = index % columns_;
    return {area_.x + col * (cellW_ + gap_), area_.y + (row - scrollRow_) * (cellH_ + gap_), cellW_, cellH_};
}

bool GridLayout::visible(int index) const {
    const int row = index / columns_;
    return row >= scrollRow_ && row < scrollRow_ + visibleRows_;
}

int GridLayout::move(int cursor, int count, int dx, int dy) const {
    if (count <= 0) return 0;
    const int col = cursor % columns_;
    if (dx < 0) return col > 0 ? cursor - 1 : cursor;
    if (dx > 0) return (col < columns_ - 1 && cursor + 1 < count) ? cursor + 1 : cursor;
    if (dy < 0) return cursor - columns_ >= 0 ? cursor - columns_ : cursor;
    if (dy > 0) {
        if (cursor + columns_ < count) return cursor + columns_;
        // A short last row: Down from the row above goes to its last item.
        const int lastRow = (count - 1) / columns_;
        return cursor / columns_ < lastRow ? count - 1 : cursor;
    }
    return cursor;
}

void GridLayout::ensureVisible(int cursor) {
    const int row = cursor / columns_;
    if (row < scrollRow_) scrollRow_ = row;
    if (row >= scrollRow_ + visibleRows_) scrollRow_ = row - visibleRows_ + 1;
    scrollRow_ = std::max(0, scrollRow_);
}

namespace {

// The favorite mark on art: a round backdrop (legible over any picture) and a star-like "*".
void drawFavoriteBadge(Painter &p, const Theme &t, const Rect &body) {
    const int d = t.px(30);
    const Rect badge{body.right() - t.px(8) - d, body.y + t.px(8), d, d};
    p.fillRoundedRect(badge, d / 2, t.palette.background.withAlpha(190));
    drawLabelColor(p, t, badge, "*", TextRole::Body, t.palette.accentText, Align::Center, true);
}

}  // namespace

void drawGridItem(Painter &p, const Theme &t, const GridLayout &layout, const Rect &cell, const GridSpec &spec,
                  const GridItemSpec &item, bool focused) {
    const Rect body{cell.x, cell.y, layout.cellWidth(), layout.bodyHeight()};
    const int radius = t.px(Radius::panel);

    switch (item.kind) {
        case GridItemKind::Cover: {
            p.fillRoundedRect(body, radius, t.palette.backgroundLo);
            if (item.art && item.art->valid()) {
                // Native size, centered: the picture is prepared at the cell's art size and never scaled.
                const Rect art{body.x + (body.w - item.art->width()) / 2, body.y + (body.h - item.art->height()) / 2,
                               item.art->width(), item.art->height()};
                p.drawImage(*item.art, art, uint8_t(focused ? 255 : 230), t.px(Radius::chip));
            }
            break;
        }
        case GridItemKind::Icon: {
            p.fillRoundedRect(body, radius, t.palette.surface);
            if (item.icon) item.icon->drawIn(p, body, t.palette.textMuted);
            break;
        }
        case GridItemKind::Placeholder: {
            // No picture (not loaded yet, or none exists): a tinted panel with a soft vertical gradient and a
            // neutral outlined mark, so every empty tile keeps the same shape and has some depth (Classic's
            // Tile::drawPlaceholder); the caption names the item.
            const float f = focused ? 1.0f : 0.0f;
            // A little of the accent in the light end gives the tile some life and depth (a plastic look).
            const Color lit = Color::lerp(t.palette.surface, t.palette.surfaceHi, 0.55f + 0.45f * f);
            const Color top = Color::lerp(lit, t.palette.accent, 0.16f + 0.10f * f);
            const Color bottom = Color::lerp(t.palette.backgroundLo, t.palette.surface, 0.25f + 0.5f * f);
            p.fillRoundedRect(body, radius, t.palette.surface);
            const Rect inner = body.inset(t.px(2));
            p.pushClip(inner);
            p.fillGradient(inner, top, bottom);
            p.popClip();
            const int mark = std::min(body.w, body.h) / 4;
            const Rect box{body.x + (body.w - mark) / 2, body.y + (body.h - mark) / 2, mark, mark};
            p.strokeRoundedRect(box, mark / 4, std::max(1, t.px(2)), t.palette.textMuted.withAlpha(uint8_t(90 + 60 * f)));
            // A fine light edge along the tile, like the rim of a lit plastic case.
            p.strokeRoundedRect(body, radius, std::max(1, t.px(1)), t.palette.textPrimary.withAlpha(uint8_t(22 + 20 * f)));
            break;
        }
    }
    if (item.favorite) drawFavoriteBadge(p, t, body);

    // Focus: a border only; the tile does not grow, does not move, has no shadow.
    if (focused)
        p.strokeRoundedRect(body.inset(-t.px(3)), radius + t.px(3), std::max(1, t.px(Stroke::emphasis)),
                            t.palette.focus);

    if (spec.caption != Caption::None) {
        const int w = cell.w;
        const int top = body.bottom() + layout.captionGap();
        drawLabel(p, t, {cell.x, top, w, layout.captionLabelHeight()}, item.label.str(), TextRole::Body,
                  focused ? TextTier::Primary : TextTier::Muted, Align::Center);
        if (spec.caption == Caption::TitleSub)
            drawLabel(p, t, {cell.x, top + layout.captionLabelHeight(), w, layout.captionSubHeight()},
                      item.sublabel.str(), TextRole::Small, TextTier::Muted, Align::Center);
    }
}

}  // namespace ui
