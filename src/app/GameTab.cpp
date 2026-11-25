#include "app/GameTab.h"

namespace patsgui {

GameTab::GameTab(Services &services)
    : library_(services.library), notice_(services.notice), options_(services.library), search_(services.library),
      list_(services.library) {}

void GameTab::onShow() {
    list_.show();
    options_.show();
}

void GameTab::onHide() {
    list_.hide();
    options_.hide();
    confirm_.close();
    search_.close();
}

bool GameTab::redrawPreview(ui::Painter &p, const ui::Theme &t, ui::Rect &damage) {
    if (options_.isOpen()) return options_.redrawPreview(p, t, damage);   // the dialog's video
    return !search_.typing() && !confirm_.open() && listShowing() && list_.redrawVideo(p, t, damage);
}

std::string GameTab::favoriteKey() const {
    GameInfo g;
    if (overlayOpen() || !focusedGame(g)) return std::string();
    return gameKey(g.ref);
}

GameTab::Overlay GameTab::handleOverlay(Event event, ConfirmRemove::Result &removal) {
    if (options_.isOpen()) return options_.handle(event) ? Overlay::Used : Overlay::None;   // it takes every event while open
    if (search_.typing()) {
        SearchHeader::Out out;
        search_.handle(event, false, out);
        return out == SearchHeader::Out::Submitted ? Overlay::SearchSubmitted : Overlay::Used;
    }
    if (confirm_.open()) return confirm_.handle(event, removal) ? Overlay::RemovalConfirmed : Overlay::Used;
    return Overlay::None;
}

bool GameTab::overlayHints(std::vector<ui::Hint> &out) const {
    if (options_.isOpen()) out = options_.hints();
    else if (search_.typing()) out = search_.hints();
    else if (confirm_.open()) out = confirm_.hints();
    else return false;
    return true;
}

void GameTab::drawOverlays(ui::Painter &p, const ui::Theme &t) {
    // The shell draws the bottom bar with the overlay's hints above the scrim while locked().
    const ui::Rect all = p.bounds();
    options_.draw(p, t, all);
    if (!search_.typing()) confirm_.draw(p, t, all);
    search_.drawDialog(p, t, all);
}

bool GameTab::updateGlue(float deltaSeconds, const GameRef &listFocus) {
    const bool changed = options_.update(deltaSeconds);
    // The list's detail panel shows the focused game's video preview; nothing else asks for one while an overlay is open.
    list_.update(overlayOpen() ? GameRef() : listFocus);
    return changed;
}

void GameTab::play(const GameRef &game) {
    // Started: the host hands the screen to the core. Failed: the host shows the error. Several versions and no
    // recent choice: the Game options open, as in the Classic GUI, and the user picks a version and starts it.
    if (library_.launch(game).status == LaunchStatus::NeedsVersion) options_.open(game);
}

void GameTab::openOptions(const GameRef &game) {
    if (search_.typing() || confirm_.open()) return;
    options_.open(game);
}

void GameTab::requestToggleFavorite(const GameInfo &game) {
    if (game.favorite) {   // removing asks first; adding does not
        confirm_.ask(ConfirmRemove::What::Favorite, game);
        return;
    }
    const bool now = library_.toggleFavorite(game.ref);
    noteFavorite(notice_, game.title, now);
    libraryChanged(game.ref, now);
}

void GameTab::askRemoveRecent(const GameInfo &game) { confirm_.ask(ConfirmRemove::What::Recent, game); }

void GameTab::applyRemoval(const ConfirmRemove::Result &removal) {
    if (removal.what == ConfirmRemove::What::Favorite) {
        const bool now = library_.toggleFavorite(removal.game);
        noteFavorite(notice_, removal.title, now);
        libraryChanged(removal.game, now);
    } else {
        library_.removeRecent(removal.game);
        noteRemovedRecent(notice_, removal.title);
        libraryChanged(removal.game, false);
    }
}

}  // namespace patsgui
