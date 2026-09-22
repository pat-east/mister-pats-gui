#pragma once

#include "Geometry.h"
#include "Theme.h"

// Shared tile grid geometry and cursor maths, so systems, games and favourites all navigate
// and lay out identically.
class GridView {
public:
    void configure(const Rect &area, Theme &theme, int targetTileWidth, int tileAspectW,
                   int tileAspectH, int labelHeight);

    int columns() const { return columns_; }
    int visibleRows() const { return visibleRows_; }
    int tileWidth() const { return tileWidth_; }
    int tileHeight() const { return tileHeight_; }

    // Centres the rows vertically when everything fits, so short lists do not hug the top.
    void centerContent(int itemCount);

    Rect cellFrame(int index, int scrollRow) const;

    // Keeps the cursor on screen; returns the possibly adjusted scroll row.
    int clampScroll(int cursor, int itemCount, int scrollRow) const;

    // Cursor movement within the grid; returns the new index.
    int move(int cursor, int itemCount, int dx, int dy) const;

private:
    Rect area_{};
    int columns_ = 1;
    int visibleRows_ = 1;
    int tileWidth_ = 0;
    int tileHeight_ = 0;
    int gap_ = 0;
    int labelHeight_ = 0;
};
