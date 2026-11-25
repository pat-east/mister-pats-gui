#include "ui-framework/kit/List.h"

#include <algorithm>

namespace ui {

void ListLayout::configure(const Rect &area, int frame, int pitch) {
    area_ = area;
    frame_ = std::max(1, frame);
    pitch_ = std::max(1, pitch);
    visible_ = std::max(1, (area.h + (pitch_ - frame_)) / pitch_);
}

Rect ListLayout::row(int index) const {
    return {area_.x, area_.y + (index - scroll_) * pitch_, area_.w, frame_};
}

void ListLayout::ensureVisible(int cursor) {
    if (cursor < scroll_) scroll_ = cursor;
    if (cursor >= scroll_ + visible_) scroll_ = cursor - visible_ + 1;
    scroll_ = std::max(0, scroll_);
}

int ListLayout::move(int cursor, int count, int delta) {
    if (count <= 0) return 0;
    return std::max(0, std::min(count - 1, cursor + delta));
}

int scrollbarGutter(const Theme &t) { return t.px(20); }

void drawScrollbar(Painter &p, const Theme &t, const Rect &list, int total, int visible, int scroll) {
    if (total <= visible || visible <= 0) return;
    const int width = std::max(2, t.px(4));
    const Rect track{list.right() - t.px(8) - width, list.y, width, list.h};
    p.fillRoundedRect(track, width / 2, t.palette.surfaceHi);
    const int minThumb = t.px(24);
    const int thumbH = std::max(minThumb, int(long(track.h) * visible / total));
    const int range = total - visible;
    const int thumbY = track.y + (range > 0 ? int(long(track.h - thumbH) * std::min(scroll, range) / range) : 0);
    p.fillRoundedRect({track.x, thumbY, width, thumbH}, width / 2, t.palette.focus);
}

}  // namespace ui
