#include "Tile.h"

#include <algorithm>

#include "Canvas.h"

namespace {

constexpr float kFocusGrowth = 0.10f;

} // namespace

Rect Tile::focusedFrame(const Rect &frame, float focus) {
    if (focus <= 0.0f) return frame;
    const int dw = int(frame.w * kFocusGrowth * focus);
    const int dh = int(frame.h * kFocusGrowth * focus);
    return {frame.x - dw / 2, frame.y - dh / 2, frame.w + dw, frame.h + dh};
}

int Tile::focusMarginX(const Rect &frame) {
    return frame.x - focusedFrame(frame, 1.0f).x;
}

int Tile::focusMarginY(const Rect &frame) {
    return frame.y - focusedFrame(frame, 1.0f).y;
}

int Tile::captionHeight(Theme &theme, bool showLabel, bool hasSublabel) {
    // Mirrors what draw() reserves, so the two cannot drift apart.
    int height = showLabel ? theme.px(26) : 0;
    if (hasSublabel) height += theme.px(20);
    return height;
}

Rect Tile::footprint(Theme &theme, const Rect &frame) {
    const Rect grown = focusedFrame(frame, 1.0f);
    const Rect shadow = Canvas::shadowBounds(grown, theme.px(16));
    const int border = theme.px(3);

    const int x = std::min(grown.x, shadow.x) - border;
    const int y = std::min(grown.y, shadow.y) - border;
    const int right = std::max(grown.right(), shadow.right()) + border;
    const int bottom = std::max(grown.bottom(), shadow.bottom()) + border;
    return {x, y, right - x, bottom - y};
}

void Tile::drawPlaceholder(Canvas &canvas, Theme &theme, const Rect &body,
                           const Content &content, float focus) {
    // Typographic fallback: a tinted panel with the name set as large as it fits.
    const Color top = Color::lerp(theme.surface, theme.surfaceHi, 0.35f + 0.65f * focus);
    const Color bottom = Color::lerp(theme.backgroundLo, theme.surface, 0.25f + 0.5f * focus);

    canvas.pushClip(body);
    canvas.verticalGradient(body, top, bottom);
    canvas.popClip();

    if (!content.nameIsInside) {
        // The caption below already names it; a neutral mark keeps every tile the same shape.
        const int mark = std::min(body.w, body.h) / 4;
        const Rect box{body.x + (body.w - mark) / 2, body.y + (body.h - mark) / 2, mark, mark};
        canvas.strokeRoundedRect(box, mark / 4, theme.px(2),
                                 theme.textMuted.withAlpha(uint8_t(40 + 40 * focus)));
        return;
    }

    if (content.label.empty()) return;

    Font &font = theme.bold();
    const int available = body.w - theme.px(20);
    int size = theme.sizeHeading();
    while (size > theme.sizeSmall() && font.measure(content.label, size) > available) size -= 2;

    const std::string text = font.elide(content.label, size, available);
    font.drawCentered(canvas, body, text, size,
                      Color::lerp(theme.textMuted, theme.textPrimary, focus));
}

void Tile::draw(Canvas &canvas, Theme &theme, const Rect &frame, const Content &content,
                float focus) {
    const Rect outer = focusedFrame(frame, focus);
    const int radius = theme.radius();

    const bool hasArt = content.image && content.image->valid();

    // With a caption underneath, a missing image gets a neutral mark so every tile in a row
    // keeps the same shape. Without a caption the name itself becomes the tile's content.
    const bool nameInside = !hasArt && !content.showLabel;
    Content shown = content;
    shown.nameIsInside = nameInside;

    const int labelHeight = captionHeight(theme, content.showLabel, !content.sublabel.empty());

    const Rect body{outer.x, outer.y, outer.w, outer.h - labelHeight};
    if (body.empty()) return;

    // Only the focused tile gets a shadow: on a dark background the others gain almost
    // nothing from it, and every shadow layer is a full-tile alpha pass.
    if (focus > 0.05f) {
        const uint8_t alpha = uint8_t(170 * focus);
        canvas.dropShadow(body, radius, theme.px(16), theme.shadow.withAlpha(alpha));
    }

    if (hasArt) {
        canvas.fillRoundedRect(body, radius, theme.backgroundLo);

        Rect target = body;
        if (content.coverArt) {
            target = body.inset(theme.px(6))
                         .fitAspect(content.image->width(), content.image->height());
        }

        const uint8_t alpha = uint8_t(215 + 40 * focus > 255 ? 255 : 215 + 40 * focus);
        canvas.drawImage(*content.image, target, alpha, content.coverArt ? theme.px(4) : radius);
    } else {
        canvas.fillRoundedRect(body, radius, theme.surface);
        drawPlaceholder(canvas, theme, body.inset(theme.px(2)), shown, focus);
    }

    if (focus > 0.01f) {
        canvas.strokeRoundedRect(body, radius, theme.px(3),
                                 theme.accent.withAlpha(uint8_t(255 * focus)));
    }

    if (content.favorite) {
        const int badge = theme.px(30);
        const Rect star{body.right() - badge - theme.px(8), body.y + theme.px(8), badge, badge};
        canvas.fillRoundedRect(star, badge / 2, theme.background.withAlpha(190));
        theme.bold().drawCentered(canvas, star, "*", theme.sizeBody(), theme.favorite);
    }

    int labelBottom = body.bottom();

    if (content.showLabel && !nameInside) {
        const Rect labelArea{outer.x, body.bottom() + theme.px(4), outer.w, theme.px(24)};
        Font &font = theme.regular();
        const std::string label = font.elide(content.label, theme.sizeBody(), labelArea.w);
        font.drawCentered(canvas, labelArea, label, theme.sizeBody(),
                          Color::lerp(theme.textMuted, theme.textPrimary, focus));
        labelBottom = labelArea.bottom();
    }

    if (!content.sublabel.empty()) {
        const Rect subArea{outer.x, labelBottom + theme.px(2), outer.w, theme.px(20)};
        theme.regular().drawCentered(canvas, subArea, content.sublabel, theme.sizeSmall(),
                                     theme.textMuted.withAlpha(200));
    }
}
