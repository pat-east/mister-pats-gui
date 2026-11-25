#include "GridView.h"

#include <algorithm>

void GridView::configure(const Rect &area, Theme &theme, int targetTileWidth, int tileAspectW,
                         int tileAspectH, int labelHeight) {
    area_ = area;
    gap_ = theme.gap();
    labelHeight_ = labelHeight;

    const int target = std::max(1, theme.px(targetTileWidth));
    columns_ = std::max(1, (area.w + gap_) / (target + gap_));
    tileWidth_ = (area.w - gap_ * (columns_ - 1)) / columns_;
    tileHeight_ = (tileAspectW > 0) ? tileWidth_ * tileAspectH / tileAspectW : tileWidth_;

    // As many whole rows as fit, and nothing else. An earlier version trimmed the tile height
    // so that exactly a third of a further row was always left showing — which worked, but
    // paid for it by distorting the tile. That is the wrong trade: the tile's proportions are
    // a deliberate choice, and squashing a square tile into a landscape one to gain a visual
    // hint loses more than the hint is worth.
    //
    // The hint comes from the geometry instead. `cellFrame` keeps laying rows out past the
    // last full one and the caller clips, so whatever space is left over shows the next row
    // cut off. It is not always the same size, and occasionally it is nearly nothing — but
    // the artwork is never wrong.
    const int rowHeight = tileHeight_ + labelHeight_ + gap_;
    visibleRows_ = std::max(1, (area.h + gap_) / rowHeight);
}

void GridView::centerContent(int itemCount) {
    if (itemCount <= 0) return;

    const int totalRows = (itemCount + columns_ - 1) / columns_;
    if (totalRows > visibleRows_) return;

    const int rowHeight = tileHeight_ + labelHeight_ + gap_;
    const int used = totalRows * rowHeight - gap_;
    if (used < area_.h) area_.y += (area_.h - used) / 2;
}

Rect GridView::cellFrame(int index, int scrollRow) const {
    const int column = index % columns_;
    const int row = index / columns_ - scrollRow;
    const int rowHeight = tileHeight_ + labelHeight_ + gap_;

    return {area_.x + column * (tileWidth_ + gap_), area_.y + row * rowHeight, tileWidth_,
            tileHeight_ + labelHeight_};
}

int GridView::clampScroll(int cursor, int itemCount, int scrollRow) const {
    if (itemCount <= 0) return 0;

    const int totalRows = (itemCount + columns_ - 1) / columns_;
    const int cursorRow = cursor / columns_;

    if (cursorRow < scrollRow) scrollRow = cursorRow;
    if (cursorRow >= scrollRow + visibleRows_) scrollRow = cursorRow - visibleRows_ + 1;

    const int maxScroll = std::max(0, totalRows - visibleRows_);
    return std::min(std::max(0, scrollRow), maxScroll);
}

int GridView::move(int cursor, int itemCount, int dx, int dy) const {
    if (itemCount <= 0) return 0;

    int index = std::min(std::max(cursor, 0), itemCount - 1);

    if (dx) {
        // Stop at the row edges: wrapping is disorienting on a television.
        const int column = index % columns_;
        const int next = column + dx;
        if (next >= 0 && next < columns_ && index - column + next < itemCount)
            index = index - column + next;
    }

    if (dy) {
        const int next = index + dy * columns_;
        if (next >= 0 && next < itemCount) index = next;
        else if (dy > 0 && index / columns_ < (itemCount - 1) / columns_) index = itemCount - 1;
    }

    return index;
}
