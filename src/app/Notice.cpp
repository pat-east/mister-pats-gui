#include "app/Notice.h"

namespace patsgui {

void noteFavorite(Notice *sink, const std::string &title, bool nowFavorite) {
    notice(sink, ui::Text::data(ui::format(nowFavorite ? "note.favorite.added" : "note.favorite.removed", {title})));
}

void noteRemovedRecent(Notice *sink, const std::string &title) {
    notice(sink, ui::Text::data(ui::format("note.recent.removed", {title})));
}

}  // namespace patsgui
