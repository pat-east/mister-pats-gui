#pragma once

// "List with details", the third view of a game list: the rows on the left, the detail of the focused game
// (its picture or video preview, title, one facts line, the preview's status) on the right. The one component
// behind Home's results, Games, Arcade and Favorites, so the view looks and behaves alike everywhere:
//
//   rows        dense rows with a favorite mark (left off only where every game is a favorite, D7);
//   facts line  "system · year · players", each part only when it is known (ui::joinParts), the same everywhere;
//   picture     the preview video while one plays, else the cover art, in the slot the DetailPanel itself
//               defines (ui::detailPanelSlot: one computation of the slot size);
//   status      the preview's status line ("Video plays in: 5 s") from the PreviewSlot;
//   keys        Up/Down move one row, Left/Right turn a page of the visible rows (D10), Up on the first row
//               leaves the list; after a letter jump the row of the letter goes to the top (showRowAtTop()).
//
// A tab owns one ListDetail and gives it its rows:
//
//   ListDetail list_{library_};                                  // member (it owns the PreviewSlot)
//   list_.show() / hide()                                        // onShow() / onHide()
//   list_.update(focusedRef)                                     // every frame while the list is on screen
//   list_.draw(p, t, body, count, cursor, paneActive, o, [&](int i, GameInfo &g) { return infoAt(i, g); },
//              [&](int first, int last) { /* fetch the window */ });
//   cursor_ = list_.move(cursor_, count, dx, dy);                // in handle()
//   list_.redrawVideo(p, t, damage)                              // in redrawPreview()

#include <functional>
#include <string>

#include "app/Library.h"
#include "app/PreviewSlot.h"
#include "ui-framework/kit/List.h"

namespace patsgui {

class ListDetail {
public:
    explicit ListDetail(Library &library) : library_(library), slot_(library) {}

    struct Options {
        bool favoriteMark = true;   // D7: off only where every row is a favorite
    };
    // Fills `out` with game `index`; false when it is not available yet (a window that is still loading).
    using RowFn = std::function<bool(int index, GameInfo &out)>;
    // Called once per draw with the visible range [first, last), before any RowFn call, so a windowed list can fetch it.
    using PrepareFn = std::function<void(int first, int last)>;

    void show() { slot_.show(); }
    void hide() { slot_.hide(); }
    // Once per frame: asks for the video preview of this game (an empty ref stops it).
    void update(const GameRef &focused) { slot_.update(focused); }

    // Draws rows and detail into `body`, the rectangle drawPageHeader returned. `paneActive`: the list has the
    // focus (the cursor row is outlined).
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &body, int count, int cursor, bool paneActive,
              const Options &o, const RowFn &row, const PrepareFn &prepare = PrepareFn());

    // The cursor after a move: dy rows up or down, dx pages (the visible rows). The cursor stays where it cannot go.
    int move(int cursor, int count, int dx, int dy) const;
    // True on the first row: Up goes to the search field.
    bool onTop(int cursor) const { return cursor <= 0; }
    // After a letter jump: the cursor's row goes to the top of the page on the next draw.
    void showRowAtTop() { pendingTop_ = true; }
    // A new list or a new scope: scroll back to the top and look the detail up again.
    void reset() { list_ = ui::ListLayout(); pendingTop_ = false; detailValid_ = false; }
    // The game's data changed (a favorite toggled, a new revision): read the detail again.
    void invalidateDetail() { detailValid_ = false; }
    int visibleRows() const { return list_.visibleRows(); }

    // Repaints only the video frame after a full draw (TabView::redrawPreview).
    bool redrawVideo(ui::Painter &p, const ui::Theme &t, ui::Rect &damage) { return slot_.redrawVideo(p, t, damage); }

    // The facts line of a game: system, year, players; the parts that are known, one format for every tab.
    static std::string factsLine(const GameInfo &g, const GameDetail &d);

private:
    Library &library_;
    PreviewSlot slot_;
    ui::ListLayout list_;
    bool pendingTop_ = false;
    GameRef detailRef_;
    GameDetail detail_;
    bool detailValid_ = false;
};

}  // namespace patsgui
