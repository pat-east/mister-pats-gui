#pragma once

// The one grid page of the new GUI: the tiles under a PageHeader, the same on every tab (Games, Arcade, Favorites,
// Systems, Home's favorites and search results). A tab supplies its items and a few facts; GridPage owns what makes
// the grids look and behave alike: the tile size, the caption, the gaps, the body rectangle, the clip with the
// "peek" row, the focused tile drawn last, the small grid with the focused title on a line of its own, the
// row-to-top after a jump, the placeholder for a picture that has not arrived, and the one-row strip (Home's
// recently played) with the same tile.
//
// Rules the page applies (owner decisions, 2026-11-23):
//   D6  the tile's second line (the system) is drawn only where the list mixes systems (`secondLine`);
//   D7  the favorite mark is left off only where every item is a favorite (`favoriteMark = false`);
//   D17 Systems shows "No core" on the tiles that have no core.
//
// Use (see GamesTab / FavoritesTab / SystemsTab):
//   GridPage grid_;                                              // member
//   const ui::Rect body = ui::drawPageHeader(...);
//   GridPage::Options o; o.secondLine = true;
//   grid_.draw(p, t, body, small, count, cursor, focused, o, [&](int i, bool onCursor, GridTile &tile) { ... });
//   cursor_ = grid_.move(cursor_, count, dx, dy);                // in handle()
// The view modes (Grid, Small grid, List with details) are `ViewMode`; the list is drawn by the tab (ListDetail).

#include <functional>
#include <algorithm>
#include <functional>
#include <string>

#include "app/Library.h"
#include "ui-framework/kit/Grid.h"
#include "ui-framework/kit/PageHeader.h"

namespace patsgui {

// ---- The three views of a list of games: the names are shared strings (view.grid, view.small, view.list).
enum class ViewMode { Grid, Small, List };
inline ViewMode viewFromInt(int v) { return v == 1 ? ViewMode::Small : v == 2 ? ViewMode::List : ViewMode::Grid; }
inline int viewToInt(ViewMode m) { return int(m); }                // what Library::...View() remembers
inline ViewMode nextView(ViewMode m) { return ViewMode((int(m) + 1) % 3); }
const char *viewNameKey(ViewMode m);                               // "view.grid", "view.small", "view.list"
// The header info "Grid · 3 of 42" ("42+" when `capped`: a search stopped at its limit).
std::string gridInfo(ViewMode m, int position, int total, bool capped = false);
// While a search still runs: "Grid · Searching… 12 found" ("12+" when `capped`).
std::string gridInfoSearching(ViewMode m, int found, bool capped = false);

// What a tile shows. `art.image` empty (not loaded or none) draws the calm placeholder.
struct GridTile {
    std::string label, sublabel;
    Art art;
    bool favorite = false;
};

class GridPage {
public:
    struct Options {
        ArtSize art = ArtSize::Grid;   // the size of the normal grid's tile (Home uses ArtSize::Home)
        bool secondLine = false;       // the caption has a second line (the system; "No core" on Systems), D6
        bool favoriteMark = true;      // the favorite mark on the art, D7
    };
    // Fills the tile of item `index`; false when it is not available yet (a window that is still loading).
    // `onCursor` is true for the item under the cursor (ask the library for its picture first).
    using TileFn = std::function<bool(int index, bool onCursor, GridTile &out)>;
    // Called once per draw with the range [first, last) that is about to be drawn, before any TileFn call,
    // so a windowed list can fetch it.
    using PrepareFn = std::function<void(int first, int last)>;

    // Draws the grid (small: the small grid, no captions, the focused title in a line below) into `body`, the
    // rectangle drawPageHeader returned. `focused` draws the outline on the cursor's tile.
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &body, bool small, int count, int cursor,
              bool focused, const Options &o, const TileFn &tile, const PrepareFn &prepare = PrepareFn());

    // One row of the same tiles that scrolls sideways (Home's recently played). `area` is the strip's room
    // (x, y, width, any height >= one tile); returns the strip's height, so the caller can place what follows.
    int drawStrip(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, int count, int cursor, bool focused,
                  const Options &o, const TileFn &tile);

    // Navigation. The cursor after a move; the cursor stays where it cannot go.
    int move(int cursor, int count, int dx, int dy) const { return grid_.move(cursor, count, dx, dy); }
    // True when the cursor is on the first row: Up goes to the search field.
    bool onTopRow(int cursor) const { return cursor / std::max(1, grid_.columns()) == 0; }
    // After a letter jump: the cursor's row goes to the top of the page on the next draw.
    void showRowAtTop() { pendingTop_ = true; }
    // A new list or a new scope: scroll back to the top.
    void reset() { grid_ = ui::GridLayout(); stripScroll_ = 0; pendingTop_ = false; }
    int columns() const { return grid_.columns(); }
    int visibleRows() const { return grid_.visibleRows(); }

private:
    ui::GridLayout grid_;
    int stripScroll_ = 0;
    bool pendingTop_ = false;
};

}  // namespace patsgui
