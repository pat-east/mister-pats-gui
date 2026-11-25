#pragma once

// The standard hint sets of the new GUI. A tab does not build the hint line of a game list itself: it says what
// is true (a list or a grid, a favorite, a search) and gets the line, so the same situation shows the same hints
// with the same words on every tab. The words are the shared keys of src/app/Strings.cpp ("Play", "Jump to letter",
// "Page", "Change view", ...). The bottom bar sorts by key rank and drops what is not `essential` from the right
// when the line is too long (src/ui-framework/docs/components/Hints.md).
//
// The other contexts have their standard hints in the component that owns them:
//   keyboard open     SearchHeader::hints() (ui::TextEntryDialog::hints(): Choose key, A Enter, X Delete, ...)
//   a modal question  ui::ModalStack::hints()
//   Game options      GameOptionsView::hints()
//   the shell         "Change tab" and "B Back" (not on Home) are added by the shell; a tab that lists its own B hint replaces Back.

#include <vector>

#include "app/Tab.h"
#include "ui-framework/kit/Components.h"

namespace patsgui {
namespace hints {

// The focus is on a game (grid, small grid or list).
struct Browse {
    bool list = false;            // List with details: Up/Down move, Left/Right turn a page; else the D-pad moves
    bool favorite = false;        // the focused game is a favorite: the hold says "Remove favorite"
    bool holdFavorite = true;     // Hold X adds or removes a favorite (off for Systems-like lists)
    bool removeRecent = false;    // Hold B removes it from Recently played
    bool jump = true;             // the triggers jump to a letter
    bool search = true;           // Start opens the search
    bool changeView = true;       // Y changes the view
};
std::vector<ui::Hint> browseGames(const Browse &b);

// The focus is on the search field: A opens the keyboard, B clears a query (Y still changes the view of what is
// listed below, when there is something listed).
std::vector<ui::Hint> searchField(bool hasQuery, bool changeView);

}  // namespace hints
}  // namespace patsgui
