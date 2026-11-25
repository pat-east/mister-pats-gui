#include "ui-framework/kit/DetailPanel.h"

#include <algorithm>

namespace ui {

GridListRects splitGridList(const Theme &t, const Rect &area) {
    const int gap = t.gap();
    const int half = (area.w - gap) / 2;
    return {{area.x, area.y, half, area.h}, {area.x + half + gap, area.y, area.w - half - gap, area.h}};
}

namespace {

Rect detailPictureRect(const Rect &slot, const ImagePtr &art) {
    if (!art || !art->valid()) return Rect{};
    return Rect{slot.x + (slot.w - art->width()) / 2, slot.y + (slot.h - art->height()) / 2, art->width(), art->height()};
}

// The picture: its own size, centered in the slot, clipped to it. It is never scaled.
void drawDetailPicture(Painter &p, const Theme &t, const Rect &slot, const ImagePtr &art, bool square) {
    p.pushClip(slot);
    if (art && art->valid()) p.drawImage(*art, detailPictureRect(slot, art), 255, square ? 0 : t.px(Radius::chip));
    else drawSurface(p, t, slot, SurfaceRole::Raised, t.px(Radius::row));
    p.popClip();
}

}  // namespace

Rect detailPanelSlot(Painter &p, const Theme &t, const Rect &area, bool hasStatus) {
    const Rect inner = area.inset(t.px(24));
    const int titleH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    const int subH = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    const int statusH = hasStatus ? p.lineHeight(FontRole::Bold, t.size(TextRole::Body)) + t.px(12) : 0;
    const int captionH = titleH + t.px(4) + subH + statusH;
    return Rect{inner.x, inner.y, inner.w, std::max(0, inner.h - captionH - t.px(Space::titleGap))};
}

void drawDetailPanel(Painter &p, const Theme &t, const Rect &area, const DetailSpec &spec) {
    drawSurface(p, t, area, SurfaceRole::Faint, t.px(Radius::panel));
    const int pad = t.px(24);
    const Rect inner = area.inset(pad);

    // Caption block at the bottom: title, muted subtitle, optional status.
    const int titleH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    const int subH = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    const Rect slot = detailPanelSlot(p, t, area, !spec.status.empty());
    drawDetailPicture(p, t, slot, spec.art, spec.artSquare);

    int y = slot.bottom() + t.px(Space::titleGap);
    drawLabel(p, t, {inner.x, y, inner.w, titleH}, spec.title.str(), TextRole::Heading, TextTier::Primary, Align::Left, true);
    y += titleH + t.px(4);
    drawLabel(p, t, {inner.x, y, inner.w, subH}, spec.subtitle.str(), TextRole::Body, TextTier::Muted);
    if (!spec.status.empty()) {
        y += subH + t.px(12);
        drawStatus(p, t, inner.x, y, spec.statusTone, spec.status.str(), TextRole::Body);
    }
}

}  // namespace ui
