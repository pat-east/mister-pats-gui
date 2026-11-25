// Favorites tab: every favorite game as Grid, Grid-Small or Grid-List, sorted by title.
//   A  start the game (several versions: choose one)     X  remove from favorites (asks first)
//   Y  change the view (the cursor is kept)               D-pad  move
// The list is small and returned whole by Library::favorites(); it is re-read when the library's
// revision changes, never inside draw. Pictures are asked for only for what is on screen.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "app/EmptyPages.h"
#include "app/GameTab.h"
#include "app/Tab.h"
#include "ui-framework/kit/Grid.h"
#include "ui-framework/kit/Input.h"
#include "ui-framework/kit/List.h"
#include "app/GridPage.h"
#include "app/Hints.h"
#include "app/LetterJump.h"
#include "ui-framework/kit/PageHeader.h"
#include "app/SearchHeader.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace {

enum class Zone { Search, Games };

std::string lower(std::string s) {
    for (char &ch : s) ch = char(std::tolower((unsigned char)ch));
    return s;
}

class FavoritesTab : public GameTab {
public:
    explicit FavoritesTab(Services &s) : GameTab(s) {
        view_ = viewFromInt(library_.favoritesView());
        reload();
    }

    bool update(float dt) override {
        const bool wantPreview = view_ == ViewMode::List && cursor_ < int(items_.size());
        bool changed = updateGlue(dt, wantPreview ? items_[size_t(cursor_)].ref : GameRef());
        if (library_.revision() != seenRevision_) { reload(); changed = true; }
        return changed;
    }

    void onShow() override {
        GameTab::onShow();
        library_.refresh();
        reload();
    }

    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) override {
        const ui::Rect body = ui::drawPageHeader(p, t, c, headerSpec());   // always, also when empty (D12)
        if (all_.empty()) {
            ui::drawEmptyPage(p, t, body, emptyPage("favorites.empty.title", "favorites.empty.why", "favorites.empty.next"));
        } else if (items_.empty()) {
            drawNoMatch(p, t, body);
        } else if (view_ == ViewMode::List) {
            drawList(p, t, body);
        } else {
            drawGrid(p, t, body, view_ == ViewMode::Small);
        }
        drawOverlays(p, t);
    }

    bool handle(Event e) override {
        ConfirmRemove::Result removal;
        switch (handleOverlay(e, removal)) {
            case Overlay::None: break;
            case Overlay::Used: return true;
            case Overlay::SearchSubmitted: applySearch(search_.query()); if (!items_.empty()) zone_ = Zone::Games; return true;
            case Overlay::RemovalConfirmed: applyRemoval(removal); return true;   // the game that was asked about, whatever the list did meanwhile
        }
        if (all_.empty()) return false;
        ensureZone();
        {   // Start and A on the field open the keyboard; B on the field clears the query (D3, D4)
            SearchHeader::Out out;
            if (search_.handle(e, zone_ == Zone::Search, out)) {
                if (out == SearchHeader::Out::Cleared) { applySearch(std::string()); zone_ = Zone::Search; }
                return true;
            }
        }
        switch (e) {
            case Event::Up:
                if (zone_ == Zone::Games) move(0, -1);
                return true;
            case Event::Down:
                if (zone_ == Zone::Search) { if (!items_.empty()) zone_ = Zone::Games; }
                else move(0, 1);
                return true;
            case Event::Left:  if (zone_ == Zone::Games) move(-1, 0); return true;
            case Event::Right: if (zone_ == Zone::Games) move(1, 0); return true;
            case Event::Confirm:
                if (zone_ == Zone::Games) play(items_[size_t(cursor_)].ref);   // (A on the field was taken by the search header)
                return true;
            case Event::Select:     // Classic: Select opens the Game options
                if (zone_ == Zone::Games && !items_.empty()) openOptions(items_[size_t(cursor_)].ref);
                return true;
            case Event::Y:
                if (items_.empty()) return false;
                view_ = nextView(view_);
                library_.setFavoritesView(viewToInt(view_));
                return true;
            case Event::XHold:
                if (zone_ == Zone::Games) askRemove();
                return true;
            case Event::LetterPrev:
            case Event::LetterNext:
                if (zone_ == Zone::Games) {
                    const int target = LetterJump::step(items_, cursor_, e == Event::LetterNext ? 1 : -1, notice_);   // Classic's rule
                    if (target != cursor_) {
                        cursor_ = target;
                        if (view_ == ViewMode::List) list_.showRowAtTop(); else grid_.showRowAtTop();   // the row of the letter goes to the top
                    }
                }
                return true;
            default: return false;
        }
    }

    std::vector<ui::Hint> hints() const override {
        std::vector<ui::Hint> overlay;
        if (overlayHints(overlay)) return overlay;
        if (all_.empty()) return {};
        if (zone_ == Zone::Games && !items_.empty()) {
            hints::Browse b;
            b.list = view_ == ViewMode::List;
            b.favorite = true;            // every game here is one: the hold removes it
            return hints::browseGames(b);
        }
        return hints::searchField(search_.hasQuery(), !items_.empty());
    }

private:
    bool focusedGame(GameInfo &out) const override {
        if (zone_ != Zone::Games || cursor_ < 0 || cursor_ >= int(items_.size())) return false;
        out = items_[size_t(cursor_)];
        return true;
    }
    // A favorite was removed: read the list again, the cursor keeps its place as in Classic.
    void libraryChanged(const GameRef &, bool) override { reload(); }
    bool listShowing() const override { return view_ == ViewMode::List && !items_.empty(); }

    // ---- data

    static bool before(const GameInfo &a, const GameInfo &b) {
        const std::string x = lower(a.title), y = lower(b.title);
        return x != y ? x < y : a.title < b.title;
    }

    void reload() {
        seenRevision_ = library_.revision();
        all_ = library_.favorites();
        std::stable_sort(all_.begin(), all_.end(), before);
        filter(false);
        list_.invalidateDetail();
    }

    // Rebuilds the visible list from the full one and the submitted search. `resetCursor` for a new query.
    void filter(bool resetCursor) {
        if (!search_.hasQuery()) {
            items_ = all_;
        } else {
            items_.clear();
            for (const GameInfo &g : all_)
                if (search_.matches(g)) items_.push_back(g);   // the library's rule, as everywhere
        }
        if (resetCursor) cursor_ = 0;
        cursor_ = std::min(cursor_, std::max(0, int(items_.size()) - 1));
        ensureZone();
    }

    void ensureZone() {
        if (items_.empty()) zone_ = Zone::Search;
    }

    void applySearch(const std::string &query) {
        search_.setQuery(query);
        filter(true);
        zone_ = Zone::Search;
    }

    void move(int dx, int dy) {
        const int count = int(items_.size());
        if (view_ == ViewMode::List) {
            if (dy < 0 && list_.onTop(cursor_)) { zone_ = Zone::Search; return; }
            cursor_ = list_.move(cursor_, count, dx, dy);
        } else {
            if (dy < 0 && grid_.onTopRow(cursor_)) { zone_ = Zone::Search; return; }
            cursor_ = grid_.move(cursor_, count, dx, dy);
        }
    }

    void askRemove() { confirm_.ask(ConfirmRemove::What::Favorite, items_[size_t(cursor_)]); }

    // ---- drawing

    ui::PageHeaderSpec headerSpec() {
        ui::PageHeaderSpec h;
        h.title = ui::Text::key("tab.favorites");
        search_.setPlaceholder(ui::Text::key("search.favorites"));
        search_.fill(h, zone_ == Zone::Search && !all_.empty());
        if (!items_.empty()) {
            h.info = ui::Text::data(gridInfo(view_, cursor_ + 1, int(items_.size())));
        }
        return h;
    }

    void drawNoMatch(ui::Painter &p, const ui::Theme &t, const ui::Rect &area) {
        ui::EmptyPageSpec page = emptyPage("favorites.nomatch.title", "favorites.nomatch.why", "empty.search.next");
        page.why = ui::Text::data(ui::format("favorites.nomatch.why", {search_.query()}));
        ui::drawEmptyPage(p, t, area, page);
    }

    void drawGrid(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, bool small) {
        GridPage::Options o;
        o.secondLine = true;       // a favorite can be of any system: the caption names it (D6)
        o.favoriteMark = false;    // every tile is a favorite, so the mark would say nothing (D7)
        const ArtSize artSize = small ? ArtSize::Small : o.art;
        grid_.draw(p, t, area, small, int(items_.size()), cursor_, zone_ == Zone::Games, o,
                   [&](int i, bool onCursor, GridTile &tile) {
                       const GameInfo &g = items_[size_t(i)];
                       tile.label = g.title;
                       tile.sublabel = g.systemName;
                       tile.art = library_.gameArt(g.ref, artSize, onCursor);
                       return true;
                   });
    }

    void drawList(ui::Painter &p, const ui::Theme &t, const ui::Rect &area) {
        ListDetail::Options o;
        o.favoriteMark = false;    // every row is a favorite, so the mark would say nothing (D7)
        list_.draw(p, t, area, int(items_.size()), cursor_, zone_ == Zone::Games, o,
                   [&](int i, GameInfo &g) {
                       if (i < 0 || i >= int(items_.size())) return false;
                       g = items_[size_t(i)];
                       return true;
                   });
    }

    std::vector<GameInfo> all_, items_;
    uint64_t seenRevision_ = ~uint64_t(0);
    int cursor_ = 0;
    ViewMode view_ = ViewMode::Grid;
    Zone zone_ = Zone::Games;
    GridPage grid_;
};

void strings(ui::Catalog &c) {
    c.set("favorites.empty.title", "No favorites yet");
    c.set("favorites.empty.why", "You have not marked any game as a favorite.");
    c.set("favorites.empty.next", "Hold the favorite button for 2 seconds on a game to add it here.");
    c.set("favorites.nomatch.title", "No favorites found");
    c.set("favorites.nomatch.why", "No favorite matches \xE2\x80\x9C{1}\xE2\x80\x9D.");
}

}  // namespace
PATSGUI_REGISTER_TAB(Tab::Favorites, FavoritesTab, strings);
}  // namespace patsgui
