#include "PreviewDisplay.h"

#include "Canvas.h"
#include "Theme.h"

int PreviewDisplay::statusHeight(Theme &theme) const {
    return preview_.engaged() ? theme.regular().lineHeight(theme.sizeBody()) + theme.px(16) : 0;
}

void PreviewDisplay::drawStatus(Canvas &canvas, Theme &theme, const Rect &line) const {
    const std::string text = preview_.statusText();
    if (text.empty()) return;
    theme.regular().drawCentered(canvas, line, theme.regular().elide(text, theme.sizeBody(), line.w),
                                 theme.sizeBody(), theme.textPrimary.withAlpha(220));
}

// Draws the playing video, unscaled and centred in the cover area.
void PreviewDisplay::drawVideo(Canvas &canvas, const Rect &coverArea, bool repainted) {
    if (!preview_.videoPlaying()) {
        if (!frame_.empty()) frame_.clear();
        return;
    }
    const int w = preview_.videoWidth();
    const int h = preview_.videoHeight();
    if (w <= 0 || h <= 0) return;
    bool draw = repainted;
    if (const uint32_t *frame = preview_.acquireFrame()) {
        frame_.assign(frame, frame + size_t(w) * size_t(h));
        preview_.releaseFrame();
        draw = true;
    }
    if (!draw || frame_.size() != size_t(w) * size_t(h)) return;
    canvas.blit(frame_.data(), w, h, coverArea.x + (coverArea.w - w) / 2,
                coverArea.y + (coverArea.h - h) / 2);
}
