#include "app/ConfirmRemove.h"

#include "app/ModalInput.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

void ConfirmRemove::ask(What what, const GameInfo &game) {
    asked_.what = what;
    asked_.game = game.ref;
    asked_.title = game.title;
    modals_ = ui::ModalStack();
    const bool favorite = what == What::Favorite;
    modals_.push(ui::Modal::confirm(ui::Text::key(favorite ? "confirm.favorite.title" : "confirm.recent.title"),
                                    ui::Text::data(ui::format(favorite ? "confirm.favorite.body" : "confirm.recent.body", {game.title}))));
}

bool ConfirmRemove::handle(Event event, Result &out) {
    ui::ModalEvent me;
    if (!toModalEvent(event, me)) return false;   // swallowed: tabs and views wait until the question is answered
    const ui::ModalResult r = modals_.handle(me);
    if (r != ui::ModalResult::Confirmed) return false;
    out = asked_;
    return true;
}

}  // namespace patsgui
