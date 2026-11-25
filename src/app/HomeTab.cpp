// Home: quick access. A search field on top; below it either Recently played (one strip) above Favorites
// (a grid), or, while a search is active, the matching games in one of three views.
//
// Buttons: A plays (or opens the keyboard on the search field); hold X adds or removes a favorite; hold B
// removes a game from Recently played; the triggers jump to a letter; Start opens the search; Y changes
// the view of search results. Removing always asks first. The parity with the Classic Home screen is listed
// in src/app/PARITY-home.md.
//
// Built from src/ui-framework/ and the shared pieces of src/app/ only: GameTab (overlays, hold keys, actions), GridPage and ListDetail
// for the games, SearchHeader for the search, ConfirmRemove for the questions, EmptyPages for the empty states.
// Home keeps its own four-zone focus (search, recent, favorites, results).

#include <algorithm>
#include <cstdio>
#include <utility>

#include "app/EmptyPages.h"
#include "app/GameTab.h"
#include "app/GridPage.h"
#include "app/Hints.h"
#include "app/LetterJump.h"
#include "app/Tab.h"
#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Grid.h"
#include "ui-framework/kit/Input.h"
#include "ui-framework/kit/PageHeader.h"
#include "app/SearchHeader.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/Primitives.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace {

enum class Focus { Search, Recent, Favorites, Results };

class HomeTab : public GameTab {
public:
    explicit HomeTab(Services &s) : GameTab(s) { view_ = viewFromInt(library_.homeView()); reload(); fixFocus(); }

    bool update(float dt) override {
        // The List with details of the results shows the focused result's video preview; nothing else asks for one.
        GameRef want;
        if (searching() && view_ == ViewMode::List && focus_ == Focus::Results)
            if (const GameInfo *g = focused()) want = g->ref;
        bool redraw = updateGlue(dt, want);
        if (library_.revision() != seenRevision_) { reload(); redraw = true; }
        return redraw;
    }

    // Like Classic: coming back to Home starts at the first recent game; the search stays.
    void onShow() override {
        GameTab::onShow();
        reload();
        recentCursor_ = favoriteCursor_ = resultCursor_ = 0;
        stripGrid_.reset();
        favoriteGrid_.reset();
        resultGrid_.reset();
        list_.reset();
        view_ = viewFromInt(library_.homeView());
        focus_ = Focus::Recent;
        if (searching() && !results_.empty()) focus_ = Focus::Results;
        else if (searching()) focus_ = Focus::Search;
        fixFocus();
    }

    std::string removeKey() const override {
        GameInfo g;
        return focus_ == Focus::Recent && !overlayOpen() && focusedGame(g) ? gameKey(g.ref) : std::string();
    }

    bool handle(Event event) override {
        ConfirmRemove::Result removal;
        switch (handleOverlay(event, removal)) {
            case Overlay::None: break;
            case Overlay::Used: return true;
            case Overlay::SearchSubmitted: applySearch(search_.query()); return true;
            case Overlay::RemovalConfirmed: applyRemoval(removal); return true;
        }
        {   // Start and A on the field open the keyboard; B on the field clears the query (D3, D4)
            SearchHeader::Out out;
            if (search_.handle(event, focus_ == Focus::Search, out)) {
                backArmed_ = false;
                if (out == SearchHeader::Out::Cleared) applySearch(std::string());
                return true;
            }
        }
        if (event == Event::Back) {
            // The host sends Back the moment B goes down. On a recent a hold may follow, so the first Back is
            // consumed; a second Back (nothing else in between) leaves, so B can never trap the user.
            if (!removeKey().empty() && !backArmed_) { backArmed_ = true; return true; }
            return false;
        }
        backArmed_ = false;
        switch (event) {
            case Event::Left:    move(-1, 0); return true;
            case Event::Right:   move(1, 0); return true;
            case Event::Up:      move(0, -1); return true;
            case Event::Down:    move(0, 1); return true;
            case Event::Confirm: if (const GameInfo *g = focused()) play(g->ref); return true;   // (A on the field was taken by the search header)
            case Event::Select:
                if (const GameInfo *g = focused()) { openOptions(g->ref); return true; }
                return false;
            case Event::XHold:   if (const GameInfo *g = focused()) requestToggleFavorite(*g); return true;
            case Event::BHold:   if (focus_ == Focus::Recent) if (const GameInfo *g = focused()) askRemoveRecent(*g); return true;
            case Event::Y:
                if (!searching() || results_.empty()) return false;
                view_ = nextView(view_);
                library_.setHomeView(viewToInt(view_));   // remembered, like the Games and Favorites views (D15)
                return true;
            case Event::LetterPrev:
            case Event::LetterNext: jumpLetter(event == Event::LetterNext ? 1 : -1); return true;
            default:             return false;
        }
    }

    std::vector<ui::Hint> hints() const override {
        std::vector<ui::Hint> overlay;
        if (overlayHints(overlay)) return overlay;
        if (focus_ == Focus::Search) return hints::searchField(searching(), searching() && !results_.empty() && library_.viewsIndividual());
        const GameInfo *g = focused();
        if (!g) return {};
        hints::Browse b;
        b.list = focus_ == Focus::Results && view_ == ViewMode::List;
        b.favorite = g->favorite;
        b.removeRecent = focus_ == Focus::Recent;
        b.changeView = focus_ == Focus::Results && library_.viewsIndividual();   // Classic names it only for the Individual default view
        return hints::browseGames(b);
    }

    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) override {
        ui::PageHeaderSpec header;
        header.title = ui::Text::key("tab.home");
        search_.setPlaceholder(ui::Text::key("search.all"));
        search_.fill(header, focus_ == Focus::Search);
        header.info = ui::Text::data(infoText());
        const ui::Rect body = ui::drawPageHeader(p, t, c, header);
        if (searching()) drawResults(p, t, body);
        else drawBrowse(p, t, body);
        drawOverlays(p, t);   // over everything; the shell redraws the bottom bar above them (locked())
    }

private:
    // ---- what the base needs
    bool focusedGame(GameInfo &out) const override {
        const GameInfo *g = focused();
        if (g) out = *g;
        return g != nullptr;
    }
    void libraryChanged(const GameRef &, bool) override { reload(); }
    bool listShowing() const override { return searching() && view_ == ViewMode::List && focus_ == Focus::Results; }

    // ---- state helpers
    bool searching() const { return search_.hasQuery(); }
    const std::string &query() const { return search_.query(); }
    int resultCount() const { return int(results_.size()); }

    const GameInfo *focused() const {
        const std::vector<GameInfo> *l = nullptr;
        int c = 0;
        switch (focus_) {
            case Focus::Recent:    l = &recents_; c = recentCursor_; break;
            case Focus::Favorites: l = &favorites_; c = favoriteCursor_; break;
            case Focus::Results:   l = &results_; c = resultCursor_; break;
            default: return nullptr;
        }
        return c >= 0 && c < int(l->size()) ? &(*l)[size_t(c)] : nullptr;
    }

    // Moves the focus somewhere that exists.
    void fixFocus() {
        if (searching()) {
            if (focus_ == Focus::Recent || focus_ == Focus::Favorites || (focus_ == Focus::Results && results_.empty()))
                focus_ = results_.empty() ? Focus::Search : Focus::Results;
            return;
        }
        if (focus_ == Focus::Results) focus_ = Focus::Recent;
        if (focus_ == Focus::Recent && recents_.empty()) focus_ = favorites_.empty() ? Focus::Search : Focus::Favorites;
        if (focus_ == Focus::Favorites && favorites_.empty()) focus_ = recents_.empty() ? Focus::Search : Focus::Recent;
    }

    void reload() {
        seenRevision_ = library_.revision();
        if (searching()) refreshResults();
        if (!searching() || searchDone_) {
            recents_ = library_.recents();
            favorites_ = library_.homeFavorites();
        }
        recentCursor_ = std::min(recentCursor_, std::max(0, int(recents_.size()) - 1));
        favoriteCursor_ = std::min(favoriteCursor_, std::max(0, int(favorites_.size()) - 1));
        fixFocus();
    }

    void refreshResults() {
        const SearchState s = search_.results();
        if (s.query != query()) return;   // the answer of an older search
        results_ = s.results;
        searchDone_ = s.done;
        searchMore_ = s.more;
        resultCursor_ = std::min(resultCursor_, std::max(0, resultCount() - 1));
        if (justSearched_ && !results_.empty()) { focus_ = Focus::Results; justSearched_ = false; }
    }

    void applySearch(const std::string &query) {
        search_.setQuery(query);
        results_.clear();
        searchDone_ = true;
        searchMore_ = false;
        resultCursor_ = 0;
        resultGrid_.reset();
        list_.reset();
        justSearched_ = false;
        search_.run(SystemId());
        if (searching()) {
            searchDone_ = false;
            justSearched_ = true;   // the cursor goes to the results as soon as there are some
            refreshResults();
            if (focus_ != Focus::Results) focus_ = Focus::Search;
        } else {
            reload();
            focus_ = Focus::Search;
        }
        fixFocus();
    }

    // ---- input
    void move(int dx, int dy) {
        switch (focus_) {
            case Focus::Search:
                if (dy > 0) {
                    if (searching()) focus_ = results_.empty() ? Focus::Search : Focus::Results;
                    else focus_ = !recents_.empty() ? Focus::Recent : (!favorites_.empty() ? Focus::Favorites : Focus::Search);
                }
                return;
            case Focus::Recent: {
                if (dx != 0) recentCursor_ = std::max(0, std::min(int(recents_.size()) - 1, recentCursor_ + dx));
                else if (dy < 0) focus_ = Focus::Search;
                else if (dy > 0 && !favorites_.empty()) focus_ = Focus::Favorites;
                return;
            }
            case Focus::Favorites: {
                if (dy < 0 && favoriteGrid_.onTopRow(favoriteCursor_)) {
                    focus_ = recents_.empty() ? Focus::Search : Focus::Recent;
                    return;
                }
                favoriteCursor_ = favoriteGrid_.move(favoriteCursor_, int(favorites_.size()), dx, dy);
                return;
            }
            case Focus::Results: {
                const int n = resultCount();
                if (view_ == ViewMode::List) {
                    if (dy < 0 && list_.onTop(resultCursor_)) { focus_ = Focus::Search; return; }
                    resultCursor_ = list_.move(resultCursor_, n, dx, dy);
                } else {
                    if (dy < 0 && resultGrid_.onTopRow(resultCursor_)) { focus_ = Focus::Search; return; }
                    resultCursor_ = resultGrid_.move(resultCursor_, n, dx, dy);
                }
                return;
            }
        }
    }

    void jumpLetter(int dir) {
        const std::vector<GameInfo> *l = nullptr;
        int *c = nullptr;
        switch (focus_) {
            case Focus::Recent:    l = &recents_; c = &recentCursor_; break;
            case Focus::Favorites: l = &favorites_; c = &favoriteCursor_; break;
            case Focus::Results:   l = &results_; c = &resultCursor_; break;
            default: return;
        }
        const int target = LetterJump::step(*l, *c, dir, notice_);   // Classic's rule, the same notice on every tab
        if (target == *c) return;
        *c = target;
        if (focus_ == Focus::Favorites) favoriteGrid_.showRowAtTop();                    // the row of the letter goes to the top
        else if (focus_ == Focus::Results) { if (view_ == ViewMode::List) list_.showRowAtTop(); else resultGrid_.showRowAtTop(); }
    }

    // ---- drawing
    // The tiles of one of the lists, in the shared tile shape (GridPage).
    GridPage::TileFn tilesOf(const std::vector<GameInfo> &list, ArtSize size) {
        return [this, &list, size](int i, bool onCursor, GridTile &tile) {
            const GameInfo &g = list[size_t(i)];
            tile.label = g.title;
            tile.sublabel = g.systemName;
            tile.favorite = g.favorite;
            tile.art = library_.gameArt(g.ref, size, onCursor);
            return true;
        };
    }

    // The header's info: where the user is in the results.
    std::string infoText() const {
        if (!searching() || resultCount() == 0) return std::string();
        const bool capped = searchMore_;
        return searchDone_ ? gridInfo(view_, resultCursor_ + 1, resultCount(), capped)
                           : gridInfoSearching(view_, resultCount(), capped);
    }

    // Section title on the left; the number of games on the right (and the position while it has the focus).
    void drawSectionHeader(ui::Painter &p, const ui::Theme &t, const ui::Rect &r, const char *titleKey, Focus section) {
        const bool active = focus_ == section;
        ui::drawLabel(p, t, r, ui::Text::key(titleKey).str(), ui::TextRole::Heading,
                      active ? ui::TextTier::Primary : ui::TextTier::Muted, ui::Align::Left, true);
        const bool recent = section == Focus::Recent;
        const int count = int(recent ? recents_.size() : favorites_.size());
        if (count == 0) return;
        const int pos = (recent ? recentCursor_ : favoriteCursor_) + 1;
        const std::string text = active ? ui::position(pos, count) : std::to_string(count);
        ui::drawLabel(p, t, r, text, ui::TextRole::Body, ui::TextTier::Muted, ui::Align::Right);
    }

    void drawLine(ui::Painter &p, const ui::Theme &t, const ui::Rect &r, const char *key) {
        ui::drawLabel(p, t, r, ui::Text::key(key).str(), ui::TextRole::Body, ui::TextTier::Muted);
    }

    void drawBrowse(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) {
        if (recents_.empty() && favorites_.empty()) {
            ui::drawEmptyPage(p, t, c, emptyPage("home.empty.title", "home.empty.why", "home.empty.next"));
            return;
        }
        const int headerH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Heading)) + t.px(8);
        const int lineH = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body)) + t.px(ui::Space::lineExtra);
        const int gap = t.gap();

        // Home lists games of every system, so the tiles name their system (D6); Home's tile is a size of its own so
        // the strip and a few rows of favorites fit one screen. The favorites are all favorites: no mark on them (D7).
        GridPage::Options recent;
        recent.art = ArtSize::Home;
        recent.secondLine = true;
        GridPage::Options favorite = recent;
        favorite.favoriteMark = false;

        int y = c.y;
        drawSectionHeader(p, t, {c.x, y, c.w, headerH}, "home.recent.title", Focus::Recent);
        y += headerH;
        if (recents_.empty()) {
            drawLine(p, t, {c.x, y, c.w, lineH}, "home.recent.empty");
            y += lineH;
        } else {
            y += stripGrid_.drawStrip(p, t, ui::Rect{c.x, y, c.w, t.px(600)}, int(recents_.size()), recentCursor_,
                                      focus_ == Focus::Recent && !search_.typing(), recent, tilesOf(recents_, recent.art));
        }
        y += gap;

        drawSectionHeader(p, t, {c.x, y, c.w, headerH}, "tab.favorites", Focus::Favorites);
        y += headerH;
        if (favorites_.empty()) {
            drawLine(p, t, {c.x, y, c.w, lineH}, "home.favorites.empty");
            return;
        }
        favoriteGrid_.draw(p, t, ui::Rect{c.x, y, c.w, std::max(1, c.bottom() - y)}, false, int(favorites_.size()),
                           favoriteCursor_, focus_ == Focus::Favorites && !search_.typing(), favorite,
                           tilesOf(favorites_, favorite.art));
    }

    // ---- search results
    void drawResults(ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        if (results_.empty()) {
            ui::drawEmptyPage(p, t, body, searchEmptyPage(searchDone_, query()));
            return;
        }
        if (view_ == ViewMode::List) drawResultList(p, t, body);
        else drawResultGrid(p, t, body, view_ == ViewMode::Small);
    }

    void drawResultGrid(ui::Painter &p, const ui::Theme &t, const ui::Rect &whole, bool small) {
        GridPage::Options o;
        o.secondLine = true;   // results come from every system (D6)
        resultGrid_.draw(p, t, whole, small, resultCount(), resultCursor_, focus_ == Focus::Results && !search_.typing(), o,
                         tilesOf(results_, small ? ArtSize::Small : o.art));
    }

    void drawResultList(ui::Painter &p, const ui::Theme &t, const ui::Rect &area) {
        ListDetail::Options o;
        list_.draw(p, t, area, resultCount(), resultCursor_, focus_ == Focus::Results, o,
                   [&](int i, GameInfo &g) {
                       if (i < 0 || i >= resultCount()) return false;
                       g = results_[size_t(i)];
                       return true;
                   });
    }

    std::vector<GameInfo> recents_, favorites_, results_;
    uint64_t seenRevision_ = 0;
    Focus focus_ = Focus::Recent;
    ViewMode view_ = ViewMode::Grid;
    int recentCursor_ = 0, favoriteCursor_ = 0, resultCursor_ = 0;
    GridPage stripGrid_, favoriteGrid_, resultGrid_;
    bool searchDone_ = true, searchMore_ = false, justSearched_ = false;
    bool backArmed_ = false;
};

void strings(ui::Catalog &c) {
    c.set("home.recent.title", "Recently played");
    c.set("home.recent.empty", "Games you start show up here.");
    c.set("home.favorites.empty", "Mark a game as a favorite and it shows up here.");
    c.set("home.empty.title", "Nothing here yet");
    c.set("home.empty.why", "Games you play and games you mark as favorites show up here, ready to start.");
    c.set("home.empty.next", "Open Systems, pick a game and start it. You can also search all games above.");
}

}  // namespace

PATSGUI_REGISTER_TAB(Tab::Home, HomeTab, strings);

}  // namespace patsgui
