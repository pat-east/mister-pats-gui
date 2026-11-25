#pragma once

#include "ui-framework/kit/Components.h"

namespace ui {

// ---- List: a column of equal rows with a cursor and a scroll position (docs/components/List.md). A
//      value type: the owner keeps the cursor, the layout keeps the scroll. No wrap; the scroll is the
//      smallest that keeps the cursor row visible.
class ListLayout {
public:
    // `area` is where the rows go; `frame` is a row's height, `pitch` the distance between rows.
    void configure(const Rect &area, int frame, int pitch);

    int visibleRows() const { return visible_; }
    int scroll() const { return scroll_; }
    int pageStep() const { return visible_ > 1 ? visible_ - 1 : 1; }

    Rect row(int index) const;              // the frame of a row after scrolling
    bool visible(int index) const { return index >= scroll_ && index < scroll_ + visible_; }
    void ensureVisible(int cursor);

    // The new cursor after a move by `delta` rows; clamped, never wraps.
    static int move(int cursor, int count, int delta);

private:
    Rect area_;
    int frame_ = 1, pitch_ = 1, visible_ = 1, scroll_ = 0;
};

// ---- Scrollbar: where the user is in a long list (docs/components/Scrollbar.md): a 4 px track, a thumb
//      in the focus color, 8 px from the edge. Drawn only when there is more than one page.
void drawScrollbar(Painter &p, const Theme &t, const Rect &list, int total, int visible, int scroll);
// The width a list keeps free at its right edge for the scrollbar, measured from the `list` rect given to
// drawScrollbar: rows end this far before the edge, so the track never overlaps a row or its focus frame.
int scrollbarGutter(const Theme &t);

}  // namespace ui
