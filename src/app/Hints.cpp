#include "app/Hints.h"

namespace patsgui {
namespace hints {

std::vector<ui::Hint> browseGames(const Browse &b) {
    using ui::HintKey;
    std::vector<ui::Hint> h;
    if (b.list) {
        h.push_back(ui::hint(HintKey::UpDown, "hint.move"));
        h.push_back(ui::hint(HintKey::LeftRight, "hint.page"));
    } else {
        h.push_back(ui::hint(HintKey::Dpad, "hint.move"));
    }
    h.push_back(ui::hint(HintKey::A, "hint.play", true));
    if (b.holdFavorite) {
        ui::Hint fav = holdFavoriteHint(b.favorite);
        fav.essential = true;
        h.push_back(fav);
    }
    if (b.removeRecent) h.push_back(holdRemoveHint());
    if (b.search) h.push_back(ui::hint(HintKey::Start, "hint.search"));
    if (b.jump) h.push_back(ui::hint(HintKey::Triggers, "hint.jump"));
    if (b.changeView) h.push_back(ui::hint(HintKey::Y, "hint.view", true));
    return h;
}

std::vector<ui::Hint> searchField(bool hasQuery, bool changeView) {
    using ui::HintKey;
    std::vector<ui::Hint> h;
    h.push_back(ui::hint(HintKey::Dpad, "hint.move"));
    h.push_back(ui::hint(HintKey::A, hasQuery ? "hint.editSearch" : "hint.search", true));
    if (hasQuery) h.push_back(ui::hint(HintKey::B, "hint.clearSearch", true));
    if (changeView) h.push_back(ui::hint(HintKey::Y, "hint.view", true));
    return h;
}

}  // namespace hints
}  // namespace patsgui
