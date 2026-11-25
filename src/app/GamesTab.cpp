#include "app/GamesTab.h"

#include <algorithm>

#include "app/GridPage.h"
#include "app/EmptyPages.h"
#include "app/Hints.h"
#include "app/LetterJump.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

namespace {

using ui::Text;

void strings(ui::Catalog &c) {
    c.set("games.scope.all", "All games");


    c.set("games.empty.nosystems.title", "No games yet");
    c.set("games.empty.nosystems.why", "No games are indexed yet.");
    c.set("games.empty.nosystems.next", "Open Settings, then Library, and choose Build game list.");
    c.set("games.empty.none.title", "No games here yet");
    c.set("games.empty.none.why", "No games were found for {1}.");
    c.set("games.empty.none.next", "Add games to its folder, then build the game list in Settings.");


}

}  // namespace

GamesTab::GamesTab(Services &services) : GameTab(services) {}

// ---------------------------------------------------------------- data

void GamesTab::readSystem() {
    systemName_ = system_;
    if (!system_.empty())
        for (const SystemInfo &s : library_.systems())
            if (s.id == system_) { systemName_ = s.name; break; }
    count_ = std::max(0, library_.gameCount(system_));
    revision_ = library_.revision();
    invalidateWindow();
    clampCursor();
}

void GamesTab::setSystem(const SystemId &id) {
    const bool scopeChanged = id.empty() != system_.empty();
    if (id != system_) {
        if (searching()) applySearch(std::string());
        system_ = id;
        cursor_ = 0;
        grid_.reset();
        list_.reset();
    }
    if (scopeChanged || arcade_) view_ = viewFromInt(startView(!id.empty()));
    zone_ = Zone::Games;
    readSystem();
}

void GamesTab::openSystem(const SystemId &system) {
    setSystem(system);
}

void GamesTab::onShow() {
    GameTab::onShow();
    view_ = viewFromInt(startView(!system_.empty()));
    readSystem();
    search_.close();
    if (searching()) refreshResults();
    ensureZone();
}

void GamesTab::onHide() { GameTab::onHide(); }

void GamesTab::clampCursor() {
    const int n = count();
    cursor_ = n <= 0 ? 0 : std::min(std::max(cursor_, 0), n - 1);
}

void GamesTab::ensureZone() {
    if (count() == 0 && zone_ == Zone::Games) zone_ = Zone::Search;
}

bool GamesTab::update(float dt) {
    GameInfo focused;
    const bool wantPreview = view_ == ViewMode::List && count() > 0 && infoAt(cursor_, focused);
    bool changed = updateGlue(dt, wantPreview ? focused.ref : GameRef());
    if (searching() && !searchDone_) {
        const size_t before = results_.size();
        refreshResults();
        changed = results_.size() != before || searchDone_;
    }
    if (library_.revision() != revision_) {
        revision_ = library_.revision();
        const int n = std::max(0, library_.gameCount(system_));
        if (n != count_) { count_ = n; invalidateWindow(); }
        if (searching() && !searchDone_) refreshResults();
        clampCursor();
        ensureZone();
        changed = true;
    }
    return changed;
}

void GamesTab::ensureWindow(int first, int n) {
    if (searching() || count_ <= 0) return;
    first = std::max(0, first);
    const int end = std::min(count_, first + n);
    if (windowValid_ && first >= windowFirst_ && end <= windowFirst_ + int(window_.size())) return;
    const int margin = std::max(24, n);
    windowFirst_ = std::max(0, first - margin);
    window_ = library_.games(system_, windowFirst_, (end - windowFirst_) + margin);
    windowValid_ = true;
}

bool GamesTab::infoAt(int index, GameInfo &out) const {
    if (searching()) {
        if (index < 0 || index >= int(results_.size())) return false;
        out = results_[size_t(index)];
        return true;
    }
    if (!windowValid_ || index < windowFirst_ || index >= windowFirst_ + int(window_.size())) return false;
    out = window_[size_t(index - windowFirst_)];
    return true;
}

void GamesTab::applySearch(const std::string &query) {
    search_.setQuery(query);
    results_.clear();
    searchDone_ = true;
    searchMore_ = false;
    cursor_ = 0;
    grid_.reset();
    list_.reset();
    search_.run(query.empty() ? SystemId() : system_);
    if (!query.empty()) {
        searchDone_ = false;
        refreshResults();
    }
    focusResults_ = searching();
    zone_ = count() > 0 ? Zone::Games : Zone::Search;
    if (count() > 0) focusResults_ = false;
}

void GamesTab::refreshResults() {
    const SearchState s = search_.results();
    if (s.query != search_.query()) return;   // the answer of an older search
    results_ = s.results;
    searchDone_ = s.done;
    searchMore_ = s.more;
    list_.invalidateDetail();
    clampCursor();
    if (focusResults_ && count() > 0) { zone_ = Zone::Games; focusResults_ = false; }
}

// ---------------------------------------------------------------- actions

void GamesTab::openGameOptions() {
    GameInfo g;
    if (infoAt(cursor_, g)) openOptions(g.ref);
}

bool GamesTab::focusedGame(GameInfo &out) const { return zone_ == Zone::Games && infoAt(cursor_, out); }

void GamesTab::libraryChanged(const GameRef &ref, bool favorite) {
    // Update the cached entries at once (the hints and the next frame read them); the library's
    // answer replaces them when the window is fetched again.
    for (GameInfo &g : window_) if (g.ref.system == ref.system && g.ref.id == ref.id) g.favorite = favorite;
    for (GameInfo &g : results_) if (g.ref.system == ref.system && g.ref.id == ref.id) g.favorite = favorite;
    list_.invalidateDetail();
    revision_ = library_.revision();
}

void GamesTab::jumpTo(int index) {
    cursor_ = std::min(std::max(index, 0), std::max(0, count() - 1));
    if (view_ == ViewMode::List) list_.showRowAtTop(); else grid_.showRowAtTop();   // the row of the letter goes to the top
}

// ---------------------------------------------------------------- input

void GamesTab::handleCursorMove(int dx, int dy) {
    if (view_ == ViewMode::List) {
        if (dy < 0 && list_.onTop(cursor_)) { zone_ = Zone::Search; return; }
        cursor_ = list_.move(cursor_, count(), dx, dy);   // a page: the visible rows (D10)
    } else {
        if (dy < 0 && grid_.onTopRow(cursor_)) { zone_ = Zone::Search; return; }
        cursor_ = grid_.move(cursor_, count(), dx, dy);
    }
}

bool GamesTab::handle(Event event) {
    ConfirmRemove::Result removal;
    switch (handleOverlay(event, removal)) {
        case Overlay::None: break;
        case Overlay::Used: return true;
        case Overlay::SearchSubmitted: applySearch(search_.query()); return true;
        case Overlay::RemovalConfirmed: applyRemoval(removal); return true;
    }
    ensureZone();
    focusResults_ = false;
    {   // Start and A on the field open the keyboard; B on the field clears the query (D3, D4)
        SearchHeader::Out out;
        if (search_.handle(event, zone_ == Zone::Search, out)) {
            if (out == SearchHeader::Out::Cleared) { applySearch(std::string()); zone_ = Zone::Search; }
            return true;
        }
    }
    switch (event) {
        case Event::Up:
            if (zone_ == Zone::Games) handleCursorMove(0, -1);
            return true;
        case Event::Down:
            if (zone_ != Zone::Games) { if (count() > 0) zone_ = Zone::Games; }
            else handleCursorMove(0, 1);
            return true;
        case Event::Left:
            if (zone_ == Zone::Games) handleCursorMove(-1, 0);
            return true;
        case Event::Right:
            if (zone_ == Zone::Games) handleCursorMove(1, 0);
            return true;
        case Event::Confirm:
            {   // (A on the search field was taken by the search header)
                GameInfo g;
                if (infoAt(cursor_, g)) play(g.ref);
            }
            return true;
        case Event::Y:
            if (count() == 0) return false;
            view_ = nextView(view_);
            if (!arcade_) library_.setGamesView(!system_.empty(), viewToInt(view_));
            return true;
        case Event::Select:
            if (zone_ == Zone::Games && count() > 0) openGameOptions();   // Classic: Select opens the Game options
            return true;
        case Event::XHold:
            {
                GameInfo g;
                if (zone_ == Zone::Games && infoAt(cursor_, g)) requestToggleFavorite(g);
            }
            return true;
        case Event::LetterPrev:
        case Event::LetterNext:
            if (zone_ == Zone::Games && count() > 1) {
                const int dir = event == Event::LetterNext ? +1 : -1;
                if (searching()) {   // Classic jumps inside the results (GamesScreen::jumpLetter over entries_)
                    const int target = LetterJump::step(results_, cursor_, dir, notice_);
                    if (target != cursor_) jumpTo(target);
                } else {
                    const int target = library_.jumpLetter(system_, cursor_, dir);
                    if (target != cursor_) {
                        jumpTo(target);
                        const std::vector<GameInfo> at = library_.games(system_, cursor_, 1);   // the game the jump landed on
                        if (!at.empty()) LetterJump::announce(notice_, at[0].title);
                    }
                }
            }
            return true;
        default:
            return false;   // Back, TabPrev, TabNext: the shell decides
    }
}

std::vector<ui::Hint> GamesTab::hints() const {
    std::vector<ui::Hint> overlay;
    if (overlayHints(overlay)) return overlay;
    const bool any = count() > 0;
    if (zone_ == Zone::Games && any) {
        GameInfo g;
        hints::Browse b;
        b.list = view_ == ViewMode::List;
        b.favorite = infoAt(cursor_, g) && g.favorite;
        b.jump = count() > 1;
        return hints::browseGames(b);
    }
    return hints::searchField(searching(), any);
}

// ---------------------------------------------------------------- drawing

ui::PageHeaderSpec GamesTab::headerSpec() {
    ui::PageHeaderSpec h;
    // The scope: all games, or the system opened from the Systems tab (also the Arcade tab's system).
    h.title = system_.empty() ? Text::key("games.scope.all") : Text::data(systemName_);
    // The placeholder says what is searched (D14): all games, or the system that is open.
    search_.setPlaceholder(system_.empty() ? Text::key("search.all") : Text::data(ui::format("search.system", {systemName_})));
    search_.fill(h, zone_ == Zone::Search);
    const int n = count();
    if (n > 0) {
        // More matches than the 500 shown: say so ("500+") instead of presenting it as the total.
        const bool capped = searching() && searchMore_;
        h.info = Text::data((searching() && !searchDone_)
                                ? gridInfoSearching(view_, n, capped)
                                : gridInfo(view_, cursor_ + 1, n, capped));
    }
    return h;
}

void GamesTab::drawEmpty(ui::Painter &p, const ui::Theme &t, const ui::Rect &area) {
    ui::EmptyPageSpec page;
    if (searching()) {
        page = searchEmptyPage(searchDone_, search_.query(), system_.empty() ? std::string() : systemName_);
    } else if (system_.empty()) {
        page = emptyPage("games.empty.nosystems.title", "games.empty.nosystems.why", "games.empty.nosystems.next");
    } else {
        page = emptyPage("games.empty.none.title", "games.empty.none.why", "games.empty.none.next");
        page.why = ui::Text::data(ui::format("games.empty.none.why", {systemName_}));
    }
    ui::drawEmptyPage(p, t, area, page);
}

void GamesTab::drawGrid(ui::Painter &p, const ui::Theme &t, const ui::Rect &whole, bool small) {
    GridPage::Options o;
    o.secondLine = system_.empty();   // the system under a title only where the list mixes systems (D6)
    const ArtSize artSize = small ? ArtSize::Small : o.art;
    grid_.draw(p, t, whole, small, count(), cursor_, zone_ == Zone::Games && !search_.typing(), o,
               [&](int i, bool onCursor, GridTile &tile) {
                   GameInfo g;
                   if (!infoAt(i, g)) return false;
                   tile.label = g.title;
                   tile.sublabel = g.systemName;
                   tile.favorite = g.favorite;
                   tile.art = library_.gameArt(g.ref, artSize, onCursor);
                   return true;
               },
               [&](int first, int last) { ensureWindow(first, last - first); });
}

void GamesTab::drawListView(ui::Painter &p, const ui::Theme &t, const ui::Rect &area) {
    ListDetail::Options o;
    o.favoriteMark = true;
    list_.draw(p, t, area, count(), cursor_, zone_ == Zone::Games, o,
               [&](int i, GameInfo &g) { return infoAt(i, g); },
               [&](int first, int last) { ensureWindow(first, last - first); });
}

void GamesTab::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) {
    const ui::Rect body = ui::drawPageHeader(p, t, c, headerSpec());
    if (count() == 0) {
        drawEmpty(p, t, body);
    } else if (view_ == ViewMode::List) {
        drawListView(p, t, body);
    } else {
        drawGrid(p, t, body, view_ == ViewMode::Small);
    }
    drawOverlays(p, t);
}

PATSGUI_REGISTER_TAB(Tab::Games, GamesTab, strings);

}  // namespace patsgui
