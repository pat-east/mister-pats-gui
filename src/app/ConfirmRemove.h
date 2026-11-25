#pragma once

// The one "remove it?" question of the new GUI: removing a game from the favorites or from Recently played
// always asks first, with the same words on every tab (the keys of src/app/Strings.cpp, owner decision D5).
//
//   ConfirmRemove confirm_;                                       // member
//   confirm_.ask(ConfirmRemove::What::Favorite, game);            // on the hold of X (or B for a recent game)
//   // handle():  if (confirm_.open()) { ConfirmRemove::Result r; if (confirm_.handle(event, r)) apply(r); return true; }
//   // hints():   if (confirm_.open()) return confirm_.hints();
//   // locked():  confirm_.open() || ...
//   // draw():    confirm_.draw(p, t, p.bounds());                  // over the tab; the shell draws the hints
//
// The question remembers the game it was asked about, not the cursor: the result names that game even when the
// list changed while the dialog was open.

#include <string>
#include <vector>

#include "app/Library.h"
#include "app/Tab.h"
#include "ui-framework/kit/Modal.h"

namespace patsgui {

class ConfirmRemove {
public:
    enum class What { Favorite, Recent };
    struct Result {
        What what = What::Favorite;
        GameRef game;
        std::string title;     // for the confirmation notice
    };

    void ask(What what, const GameInfo &game);
    bool open() const { return !modals_.empty(); }
    void close() { modals_ = ui::ModalStack(); }

    // Routes one event while open (it takes every event). True when the user confirmed; `out` is the game asked about.
    bool handle(Event event, Result &out);
    std::vector<ui::Hint> hints() const { return modals_.hints(); }
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &canvas) const { if (open()) modals_.draw(p, t, canvas); }

private:
    ui::ModalStack modals_;
    Result asked_;
};

}  // namespace patsgui
