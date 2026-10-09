#include "LoadingIndicator.h"

#include <algorithm>
#include <cmath>

#include "Canvas.h"
#include "Input.h"   // nowMs
#include "Theme.h"

namespace LoadingIndicator {
namespace {

void drawFittedLine(Canvas &canvas, Font &font, const Rect &area, const std::string &text,
                    int preferredSize, int minimumSize, Color color) {
    int size = preferredSize;
    while (size > minimumSize && font.measure(text, size) > area.w) size -= 2;

    const std::string visible = font.elide(text, size, area.w);
    font.draw(canvas, area.x, area.y, visible, size, color);
}

} // namespace

void draw(Canvas &canvas, Theme &theme, const Rect &area, const std::string &title,
          float fraction, const std::string &status, const std::string &counts,
          const std::string &extra) {
    drawFittedLine(canvas, theme.bold(), area, title, theme.sizeTitle(), theme.sizeBody(),
                   theme.textPrimary);

    const int barY = area.y + theme.px(110);
    const Rect track{area.x, barY, area.w, theme.px(14)};
    canvas.fillRoundedRect(track, theme.px(7), theme.surface);

    const int filled =
        int(float(track.w) * std::min(1.0f, std::max(0.0f, fraction)));
    if (filled > 0)
        canvas.fillRoundedRect({track.x, track.y, std::max(filled, theme.px(14)), track.h},
                               theme.px(7), theme.accent);

    int y = barY + theme.px(40);
    if (!status.empty()) {
        drawFittedLine(canvas, theme.regular(), {area.x, y, area.w, 0}, status,
                       theme.sizeBody(), theme.sizeSmall(), theme.textPrimary);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(18);
    }

    drawFittedLine(canvas, theme.regular(), {area.x, y, area.w, 0}, counts,
                   theme.sizeBody(), theme.sizeSmall(), theme.textMuted);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(20);

    if (!extra.empty()) {
        drawFittedLine(canvas, theme.regular(), {area.x, y, area.w, 0}, extra,
                       theme.sizeBody(), theme.sizeSmall(), theme.textMuted.withAlpha(190));
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(20);
    }

    // A moving dot, because a bar that sits still on a big directory looks stuck. Driven by
    // the clock rather than an accumulator each screen would otherwise have to keep of its
    // own just for this. Placed a fixed distance below whatever text came last, rather than
    // pinned to the bottom of `area` — a caller that skips the optional status line, or picks
    // a shorter area, must not end up with the dot overlapping the counts line.
    const float spinner = float(nowMs()) / 1000.0f;
    const int dotX = area.x + int((std::sin(spinner * 3.0f) * 0.5f + 0.5f) * theme.px(40));
    canvas.fillRoundedRect({dotX, y, theme.px(8), theme.px(8)}, theme.px(4),
                           theme.accent.withAlpha(200));
}

} // namespace LoadingIndicator
