#pragma once

#include "ui-framework/kit/Components.h"

namespace ui {

// ---- GridItem: one tile (docs/components/GridItem.md). Art is drawn at its native size and is never
//      scaled; focus is a border only, the tile does not grow.
enum class GridItemKind { Cover, Icon, Placeholder };
enum class Caption { None, Title, TitleSub };

struct GridItemSpec {
    GridItemKind kind = GridItemKind::Cover;
    ImagePtr art;               // Cover: the prepared picture (native size, never scaled)
    IconPtr icon;               // Icon tile: the icon, centered in the body
    Text label, sublabel;
    bool favorite = false;      // the mark on the art
};

// The size of a grid's items, in design pixels. The cell is the art plus 6 on every side.
struct GridSpec {
    int artW = 245, artH = 245;
    Caption caption = Caption::Title;
};

// ---- Grid: layout and navigation of tiles (docs/components/Grid.md). A value type: the owner keeps
//      the cursor, the layout keeps the scroll row. No wrap; Down on a short last row goes to the last
//      item; the scroll keeps the focused row fully visible.
class GridLayout {
public:
    void configure(Painter &measurer, const Theme &t, const Rect &area, const GridSpec &spec);

    int columns() const { return columns_; }
    int visibleRows() const { return visibleRows_; }
    int scrollRow() const { return scrollRow_; }
    int cellWidth() const { return cellW_; }
    int cellHeight() const { return cellH_; }
    int bodyHeight() const { return bodyH_; }
    int captionLabelHeight() const { return labelH_; }
    int captionSubHeight() const { return subH_; }
    int captionGap() const { return captionGap_; }

    // The frame of an item (art body plus caption) after scrolling; may lie outside the area.
    Rect cell(int index) const;
    bool visible(int index) const;
    int firstVisible() const { return scrollRow_ * columns_; }
    // One past the last item to draw: the visible rows plus one more row, which the caller's clip cuts off.
    // That cut-off row (the 'peek') shows that the grid goes on; draw `firstVisible()` up to `endWithPeek(count)`.
    int endWithPeek(int count) const { return count < firstVisible() + columns_ * (visibleRows_ + 1) ? count : firstVisible() + columns_ * (visibleRows_ + 1); }

    // The new cursor after a move (dx, dy in -1..1); the cursor stays where it cannot go.
    int move(int cursor, int count, int dx, int dy) const;
    void ensureVisible(int cursor);

private:
    Rect area_;
    int columns_ = 1, visibleRows_ = 1, scrollRow_ = 0;
    int cellW_ = 0, cellH_ = 0, bodyH_ = 0, labelH_ = 0, subH_ = 0, gap_ = 0, captionGap_ = 0;
};

void drawGridItem(Painter &p, const Theme &t, const GridLayout &layout, const Rect &cell, const GridSpec &spec,
                  const GridItemSpec &item, bool focused);

}  // namespace ui
