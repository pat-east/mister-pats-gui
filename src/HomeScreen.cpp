#include "HomeScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

#include "Alphabet.h"
#include "ButtonLabels.h"
#include "Canvas.h"
#include "Tile.h"

namespace {

GameView configuredHomeView(const Preferences &preferences) {
    return gameViewFromName(preferences.defaultView() == "individual"
                                ? preferences.homeView() : preferences.defaultView());
}

// Design pixels; everything else follows from the tile width.
constexpr int kTileWidth = 190;
// Square: see the note in GamesScreen. Box art comes both ways round, and a square tile is
// the shape that suits both equally.
constexpr int kTileAspectW = 1;
constexpr int kTileAspectH = 1;

} // namespace

HomeScreen::HomeScreen(Context &context) : context_(context), searchResults_(context) {
    searchResults_.setView(configuredHomeView(context.preferences));
    refresh();
}

void HomeScreen::invalidateSearchIndex() {
    searchIndex_.clear();
    searchBuildPaths_.clear();
    searchBuildSystem_ = searchBuildPath_ = 0;
    searchIndexReady_ = false;
    searchResults_.showSearchResults({});
    if (search_.hasQuery()) {
        pendingSearchQuery_ = search_.query();
        searchBuildPending_ = true;
    } else {
        pendingSearchQuery_.clear();
        searchBuildPending_ = false;
    }
}

void HomeScreen::beginSearch(const std::string &query) {
    if (query.empty()) {
        pendingSearchQuery_.clear();
        searchBuildPending_ = false;
        return;
    }
    pendingSearchQuery_ = query;
    if (searchIndexReady_) showSearchResults();
    else {
        searchBuildSystem_ = searchBuildPath_ = 0;
        searchBuildPaths_.clear();
        searchIndex_.clear();
        searchBuildPending_ = true;
    }
}

void HomeScreen::showSearchResults() {
    searchResults_.setView(configuredHomeView(context_.preferences));
    std::vector<std::pair<const GameSystem *, Game>> matches;
    for (const auto &entry : searchIndex_)
        if (entry.first && GameSearch::matches(entry.second.name, entry.first->name,
                                               pendingSearchQuery_, true))
            matches.push_back(entry);
    searchResults_.showSearchResults(matches);
}

void HomeScreen::buildSearchIndex() {
    if (!searchBuildPending_) return;
    constexpr size_t kBatchSize = 160;
    size_t budget = kBatchSize;
    const auto &systems = context_.library.systems();
    while (budget && searchBuildSystem_ < systems.size()) {
        const GameSystem &system = systems[searchBuildSystem_];
        if (searchBuildPaths_.empty() && searchBuildPath_ == 0)
            searchBuildPaths_ = context_.library.pathsOf(system);
        while (budget && searchBuildPath_ < searchBuildPaths_.size()) {
            const std::string &path = searchBuildPaths_[searchBuildPath_++];
            --budget;
            if (!Library::isSystemFile(system, path))
                searchIndex_.emplace_back(&system, Library::makeStub(system, path));
        }
        if (searchBuildPath_ >= searchBuildPaths_.size()) {
            searchBuildPaths_.clear();
            searchBuildPath_ = 0;
            ++searchBuildSystem_;
        }
    }
    if (searchBuildSystem_ >= systems.size()) {
        std::vector<std::pair<const GameSystem *, Game>> organized;
        size_t offset = 0;
        for (const GameSystem &system : systems) {
            std::vector<Game> systemGames;
            while (offset < searchIndex_.size() && searchIndex_[offset].first == &system)
                systemGames.push_back(std::move(searchIndex_[offset++].second));
            if (context_.preferences.organizeGameVersions())
                systemGames = Library::organizeGames(system, std::move(systemGames));
            for (Game &game : systemGames) organized.emplace_back(&system, std::move(game));
        }
        searchIndex_ = std::move(organized);
        searchIndexReady_ = true;
        searchBuildPending_ = false;
        showSearchResults();
    }
}

void HomeScreen::refresh() {
    rows_.clear();

    std::set<std::string> usedSystems;
    for (const HistoryEntry &entry : context_.history.entries()) {
        if (!entry.system.empty()) usedSystems.insert(entry.system);
        else if (const GameSystem *system = context_.library.systemForPath(entry.path))
            usedSystems.insert(system->name);
    }
    for (const FavoriteEntry &entry : context_.favorites.entries()) usedSystems.insert(entry.system);
    std::map<std::string, const std::vector<Game> *> systemGames;
    if (context_.preferences.organizeGameVersions()) {
        for (const std::string &name : usedSystems) {
            if (const GameSystem *system = context_.library.findSystem(name))
                systemGames.emplace(name, &context_.library.gamesOf(*system));
        }
    }

    const auto groupForPath = [&](const GameSystem &system, const std::string &path) -> const Game * {
        auto found = systemGames.find(system.name);
        if (found == systemGames.end()) return nullptr;
        for (const Game &game : *found->second) {
            if (game.path == path) return &game;
            for (const Game::Variant &version : game.versions)
                if (version.path == path) return &game;
        }
        return nullptr;
    };
    const auto homeGame = [&](const GameSystem &system, const std::string &path,
                              const Game *group) {
        Game game = Library::makeGame(system, path, ArtworkVariant::Home);
        if (group) {
            game.name = group->name;
            game.versions = group->versions;
            Library::resolveArtwork(system, game, ArtworkVariant::Home);
        }
        return game;
    };

    Row recent;
    recent.title = "Recently played";
    recent.emptyHint = "Nothing started yet";
    std::set<std::string> recentGroups;
    for (const HistoryEntry &entry : context_.history.entries()) {
        const GameSystem *system = entry.system.empty()
                                       ? context_.library.systemForPath(entry.path)
                                       : context_.library.findSystem(entry.system);
        if (!system || Library::isSystemFile(*system, entry.path)) continue;
        const Game *group = context_.preferences.organizeGameVersions()
                                ? groupForPath(*system, entry.path) : nullptr;
        Game game = homeGame(*system, entry.path, group);
        if (group) {
            const std::string identity = system->name + "\t" + group->name;
            if (!recentGroups.insert(identity).second) continue;
            game.path = group->path;
        }
        recent.items.push_back({system, std::move(game), {}});
    }

    Row favorites;
    favorites.title = "Favorites";
    favorites.emptyHint = "Hold X for 2s on a game to mark it";
    std::set<std::string> favoriteGroups;
    for (const FavoriteEntry &entry : context_.favorites.entries()) {
        const GameSystem *system = context_.library.findSystem(entry.system);
        if (!system || Library::isSystemFile(*system, entry.path)) continue;
        const Game *group = context_.preferences.organizeGameVersions()
                                ? groupForPath(*system, entry.path) : nullptr;
        Game game = homeGame(*system, entry.path, group);
        if (group) {
            const std::string identity = system->name + "\t" + group->name;
            if (!favoriteGroups.insert(identity).second) continue;
            game.path = group->path;
        }
        favorites.items.push_back({system, std::move(game), {}});
    }
    // Unlike Recently played, where the order itself is the information, favourites carry
    // none — sorted alphabetically is what makes a specific one fast to find as the list
    // grows, rather than however toggling them on happened to order it.
    std::sort(favorites.items.begin(), favorites.items.end(), [](const Item &a, const Item &b) {
        return strcasecmp(a.game.name.c_str(), b.game.name.c_str()) < 0;
    });

    for (Row *row : {&recent, &favorites}) {
        row->focus.assign(row->items.size(), 0.0f);
        rows_.push_back(std::move(*row));
    }

    activeRow_ = 0;
    pageScroll_ = 0;
    paintedPageScroll_ = -1;
    paintedImages_ = uint64_t(-1);
    needsPaint_ = true;

}

HomeScreen::Metrics HomeScreen::metrics() const {
    Theme &theme = context_.theme;

    Metrics m;
    m.tileWidth = theme.px(kTileWidth);
    m.tileHeight = m.tileWidth * kTileAspectH / kTileAspectW;
    // Games carry a second line naming their system, and that line needs reserving here or
    // the tile draws it below itself, outside the row, where the clip removes it.
    m.caption = Tile::captionHeight(theme, true, true);

    const Rect probe{0, 0, m.tileWidth, m.tileHeight + m.caption};
    m.growX = Tile::focusMarginX(probe);
    m.growY = Tile::focusMarginY(probe);

    // The focused tile grows around its frame, so the row has to be taller than the tile by
    // that much at the top and the bottom. Without it the highlight is cut off along the
    // edge it grew into.
    m.rowHeight = theme.px(40) + m.tileHeight + m.caption + 2 * m.growY + theme.gap();
    return m;
}

const HomeScreen::Item *HomeScreen::current() const {
    if (rows_.empty()) return nullptr;
    const Row &row = rows_[size_t(activeRow_)];
    if (row.items.empty()) return nullptr;
    return &row.items[size_t(std::min(std::max(row.cursor, 0), int(row.items.size()) - 1))];
}

std::string HomeScreen::favoritePath() const {
    if (search_.hasQuery()) return searchResults_.favoritePath();
    const Item *item = current();
    return item ? item->game.path : std::string();
}

bool HomeScreen::selectedGame(const GameSystem *&system, Game &game) const {
    if (search_.hasQuery()) return searchResults_.selectedGame(system, game);
    const Item *item = current();
    if (!item) return false;
    system = item->system;
    game = item->game;
    return true;
}

void HomeScreen::launch() {
    const Item *item = current();
    if (!item) return;

    Game selected = item->game;
    if (context_.preferences.organizeGameVersions() && selected.versions.size() > 1) {
        std::vector<std::string> candidates;
        for (const Game::Variant &variant : selected.versions) candidates.push_back(variant.path);
        const std::string recent = context_.history.mostRecentVersion(
            item->system->name, selected.name, candidates);
        if (recent.empty()) {
            if (context_.onChooseGameVersion) context_.onChooseGameVersion(*item->system, selected);
            return;
        }
        selected.path = recent;
    }

    if (context_.launcher.launchGame(*item->system, selected)) {
        context_.history.remember(item->system->name, selected.path, selected.name);
        context_.notify("Starting " + selected.name);
        context_.standDown = true;
    } else {
        context_.showError("Could not start " + selected.name, context_.launcher.lastError());
    }
}

void HomeScreen::toggleFavorite() {
    const Item *item = current();
    if (!item) return;

    bool was = context_.favorites.contains(item->game.path);
    for (const Game::Variant &version : item->game.versions)
        was = was || context_.favorites.contains(version.path);
    if (was && !item->game.versions.empty()) {
        if (context_.favorites.contains(item->game.path))
            context_.favorites.toggle(item->system->name, item->game.path, item->game.name);
        for (const Game::Variant &version : item->game.versions)
            if (context_.favorites.contains(version.path))
                context_.favorites.toggle(item->system->name, version.path, item->game.name);
    } else {
        context_.favorites.toggle(item->system->name, item->game.path, item->game.name);
    }
    context_.notify(was ? item->game.name + " removed from favorites"
                        : item->game.name + " added to favorites");

    // Rebuild the favorites grid in place.
    const int previous = rows_[1].cursor;
    refresh();
    rows_[1].cursor = std::min(previous, std::max(0, int(rows_[1].items.size()) - 1));
}

void HomeScreen::handle(Action action) {
    if (search_.handle(action, search_.hasQuery() ? !searchResults_.canMoveUp()
                                                   : activeRow_ == 0,
                       [this](const std::string &query) { beginSearch(query); })) return;
    if (search_.hasQuery()) {
        searchResults_.handle(action);
        if (action == Action::ToggleFavorite) refresh();
        return;
    }
    if (rows_.empty()) return;
    Row &row = rows_[size_t(activeRow_)];

    switch (action) {
    case Action::Left:
        if (activeRow_ == 0) {
            if (row.cursor > 0) --row.cursor;
        } else if (row.cursor % favoriteColumns_ > 0) {
            --row.cursor;
        }
        break;
    case Action::Right:
        if (row.cursor + 1 < int(row.items.size()) &&
            (activeRow_ == 0 || row.cursor % favoriteColumns_ + 1 < favoriteColumns_))
            ++row.cursor;
        break;
    case Action::Up:
        if (activeRow_ == 1) {
            if (row.cursor >= favoriteColumns_) row.cursor -= favoriteColumns_;
            else activeRow_ = 0;
        }
        break;
    case Action::Down:
        if (activeRow_ == 0) {
            activeRow_ = 1;
        } else if (row.cursor + favoriteColumns_ < int(row.items.size())) {
            row.cursor += favoriteColumns_;
        } else if (row.cursor / favoriteColumns_ <
                   (int(row.items.size()) - 1) / favoriteColumns_) {
            row.cursor = int(row.items.size()) - 1;
        }
        break;
    case Action::Confirm:
        launch();
        break;
    case Action::ToggleFavorite:
        toggleFavorite();
        break;
    case Action::JumpPrev:
        jumpLetter(row, -1);
        break;
    case Action::JumpNext:
        jumpLetter(row, 1);
        break;
    default:
        break;
    }
}

void HomeScreen::jumpLetter(Row &row, int direction) {
    const int count = int(row.items.size());
    const int target = Alphabet::jump(row.cursor, count, direction,
                                      [&row](int i) -> const std::string & {
                                          return row.items[size_t(i)].game.name;
                                      });
    if (target == row.cursor) return;

    row.cursor = target;

    const char letter = Alphabet::initial(row.items[size_t(row.cursor)].game.name);
    context_.notify(std::string("Games starting with ") +
                    (letter == '#' ? "0–9" : std::string(1, letter)), 1.2f);
}

void HomeScreen::update(float deltaSeconds) {
    buildSearchIndex();
    if (search_.hasQuery()) {
        searchResults_.update(deltaSeconds);
        return;
    }
    const float speed = Tile::focusStep(deltaSeconds);

    for (size_t r = 0; r < rows_.size(); ++r) {
        Row &row = rows_[r];
        const bool active = int(r) == activeRow_;
        for (size_t i = 0; i < row.focus.size(); ++i) {
            const float target = (active && int(i) == row.cursor) ? 1.0f : 0.0f;
            row.focus[i] += (target - row.focus[i]) * speed;
        }
    }
}

void HomeScreen::renderRow(Canvas &canvas, const Rect &area, Row &row, bool active) {
    Theme &theme = context_.theme;

    const int titleHeight = theme.px(40);
    theme.bold().draw(canvas, area.x, area.y, row.title, theme.sizeBody(),
                      active ? theme.textPrimary : theme.textMuted.withAlpha(150));

    if (!row.items.empty()) {
        char count[32];
        std::snprintf(count, sizeof(count), "%d", int(row.items.size()));
        const int width = theme.regular().measure(count, theme.sizeSmall());
        theme.regular().draw(canvas, area.right() - width, area.y + theme.px(4), count,
                             theme.sizeSmall(), theme.textMuted.withAlpha(120));
    }

    const Rect strip{area.x, area.y + titleHeight, area.w, area.h - titleHeight};
    if (strip.empty()) return;

    if (row.items.empty()) {
        theme.regular().draw(canvas, strip.x, strip.y + theme.px(10),
                             formatButtonLabels(row.emptyHint),
                             theme.sizeSmall(), theme.textMuted.withAlpha(110));
        return;
    }

    const int gap = theme.gap();
    const Metrics m = metrics();
    const int tileWidth = m.tileWidth;
    const int tileHeight = m.tileHeight;
    const int labelHeight = m.caption;

    // Lay the tiles out inside the growth margin, so a focused one expands into reserved
    // space instead of into the clip.
    const Rect lane{strip.x + m.growX, strip.y + m.growY, strip.w - 2 * m.growX,
                    strip.h - m.growY};
    const int perPage = std::max(1, (lane.w + gap) / (tileWidth + gap));

    if (row.cursor < row.scroll) row.scroll = row.cursor;
    if (row.cursor >= row.scroll + perPage) row.scroll = row.cursor - perPage + 1;
    row.scroll = std::min(std::max(0, row.scroll),
                          std::max(0, int(row.items.size()) - perPage));

    canvas.pushClip(strip);

    const int last = std::min(int(row.items.size()), row.scroll + perPage + 1);

    // See the identical note in GamesScreen::renderGrid: the focused tile is drawn last so
    // its shadow sits above its neighbours, which used to mean its own image request was
    // also last and lost the frame's decode budget to whichever tile came before it.
    if (context_.preferences.showBoxArt() && active && row.cursor >= row.scroll &&
        row.cursor < last) {
        const Item &focused = row.items[size_t(row.cursor)];
        context_.images.requestAsyncCandidates(focused.game.boxartCandidates, tileWidth,
                                              tileHeight, true);
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = row.scroll; i < last; ++i) {
            const bool isCursor = active && i == row.cursor;
            if ((pass == 0) == isCursor) continue;

            Item &item = row.items[size_t(i)];
            const Rect frame{lane.x + (i - row.scroll) * (tileWidth + gap), lane.y, tileWidth,
                             tileHeight + labelHeight};

            Tile::Content content;
            content.label = item.game.name;
            content.sublabel = item.system->name;
            content.coverArt = true;
            content.favorite = context_.favorites.contains(item.game.path);
            if (context_.preferences.showBoxArt())
                content.image = context_.images.requestAsyncCandidates(
                    item.game.boxartCandidates, tileWidth, tileHeight);
            content.imageRevealPercent = item.boxartFadeIn.revealPercent();

            Tile::draw(canvas, theme, frame, content, row.focus[size_t(i)]);
            item.boxartFadeIn.advance();
        }
    }

    canvas.popClip();
}

bool HomeScreen::requestImages(Row &row, int laneWidth, bool active) {
    if (!context_.preferences.showBoxArt() || row.items.empty()) return false;

    const Metrics m = metrics();
    const int gap = context_.theme.gap();
    const int lane = laneWidth - 2 * m.growX;
    const int perPage = std::max(1, (lane + gap) / (m.tileWidth + gap));

    if (row.cursor < row.scroll) row.scroll = row.cursor;
    if (row.cursor >= row.scroll + perPage) row.scroll = row.cursor - perPage + 1;
    row.scroll = std::min(std::max(0, row.scroll),
                          std::max(0, int(row.items.size()) - perPage));

    const int last = std::min(int(row.items.size()), row.scroll + perPage + 1);
    bool revealActive = false;
    for (int i = row.scroll; i < last; ++i) {
        Item &item = row.items[size_t(i)];
        const ImagePtr image = context_.images.requestAsyncCandidates(
            item.game.boxartCandidates, m.tileWidth, m.tileHeight,
            active && i == row.cursor);
        item.boxartFadeIn.observe(image, active && i == row.cursor);
        revealActive = revealActive || item.boxartFadeIn.active();
    }
    return revealActive;
}

bool HomeScreen::requestFavoriteImages(const Rect &body, int gridY, const Metrics &m) {
    if (!context_.preferences.showBoxArt()) return false;
    Row &favorites = rows_[1];
    if (favorites.items.empty()) return false;

    const int step = m.tileHeight + m.caption + context_.theme.gap();
    const int firstRow = std::max(0, (body.y - gridY) / step - 1);
    const int lastRow = std::min((int(favorites.items.size()) + favoriteColumns_ - 1) /
                                     favoriteColumns_,
                                 (body.bottom() - gridY) / step + 2);

    // Put the selected cover ahead of nearby prefetches in the worker queue.
    if (activeRow_ == 1 && favorites.cursor >= firstRow * favoriteColumns_ &&
        favorites.cursor < std::min(int(favorites.items.size()), lastRow * favoriteColumns_)) {
        const Item &focused = favorites.items[size_t(favorites.cursor)];
        context_.images.requestAsyncCandidates(focused.game.boxartCandidates, m.tileWidth,
                                               m.tileHeight, true);
    }
    bool revealActive = false;
    for (int i = firstRow * favoriteColumns_;
         i < std::min(int(favorites.items.size()), lastRow * favoriteColumns_); ++i) {
        Item &item = favorites.items[size_t(i)];
        const bool focused = activeRow_ == 1 && i == favorites.cursor;
        const ImagePtr image = context_.images.requestAsyncCandidates(
            item.game.boxartCandidates, m.tileWidth, m.tileHeight, focused);
        item.boxartFadeIn.observe(image, focused);
        revealActive = revealActive || item.boxartFadeIn.active();
    }
    return revealActive;
}

void HomeScreen::renderFavorites(Canvas &canvas, const Rect &body, int sectionY,
                                 const Metrics &m) {
    Theme &theme = context_.theme;
    Row &favorites = rows_[1];
    const int titleHeight = theme.px(40);
    const int gridY = sectionY + titleHeight + m.growY;
    const int step = m.tileHeight + m.caption + theme.gap();

    theme.bold().draw(canvas, body.x, sectionY, favorites.title, theme.sizeBody(),
                      activeRow_ == 1 ? theme.textPrimary : theme.textMuted.withAlpha(150));
    if (!favorites.items.empty()) {
        char count[32];
        std::snprintf(count, sizeof(count), "%d", int(favorites.items.size()));
        const int width = theme.regular().measure(count, theme.sizeSmall());
        theme.regular().draw(canvas, body.right() - width, sectionY + theme.px(4), count,
                             theme.sizeSmall(), theme.textMuted.withAlpha(120));
    } else {
        theme.regular().draw(canvas, body.x, sectionY + titleHeight + theme.px(10),
                             formatButtonLabels(favorites.emptyHint), theme.sizeSmall(),
                             theme.textMuted.withAlpha(110));
        return;
    }

    const int firstRow = std::max(0, (body.y - gridY) / step - 1);
    const int lastRow = std::min((int(favorites.items.size()) + favoriteColumns_ - 1) /
                                     favoriteColumns_,
                                 (body.bottom() - gridY) / step + 2);
    const int first = firstRow * favoriteColumns_;
    const int last = std::min(int(favorites.items.size()), lastRow * favoriteColumns_);

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = first; i < last; ++i) {
            const bool isCursor = activeRow_ == 1 && i == favorites.cursor;
            if ((pass == 0) == isCursor) continue;

            Item &item = favorites.items[size_t(i)];
            const Rect frame{body.x + m.growX + (i % favoriteColumns_) *
                                  (m.tileWidth + theme.gap()),
                             gridY + (i / favoriteColumns_) * step, m.tileWidth,
                             m.tileHeight + m.caption};

            Tile::Content content;
            content.label = item.game.name;
            content.sublabel = item.system->name;
            content.coverArt = true;
            content.favorite = true;
            if (context_.preferences.showBoxArt())
                content.image = context_.images.requestAsyncCandidates(
                    item.game.boxartCandidates, m.tileWidth, m.tileHeight);
            content.imageRevealPercent = item.boxartFadeIn.revealPercent();
            Tile::draw(canvas, theme, frame, content, favorites.focus[size_t(i)]);
            item.boxartFadeIn.advance();
        }
    }
}

bool HomeScreen::rowChanged(const Row &row, bool active) const {
    if (row.paintedScroll != row.scroll || row.paintedCount != row.items.size() ||
        row.paintedActive != active || row.paintedFocus.size() != row.focus.size())
        return true;

    for (size_t i = 0; i < row.focus.size(); ++i) {
        if (std::fabs(row.focus[i] - row.paintedFocus[i]) > 0.002f) return true;
    }
    return false;
}

void HomeScreen::render(Canvas &canvas, const Rect &area, bool fullRedraw) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    const int searchHeight = theme.px(54);
    const int searchGap = theme.px(20);
    const Rect searchField{area.x, area.y + headerHeight + theme.px(2), area.w,
                           searchHeight - theme.px(4)};
    const Rect body{area.x, searchField.bottom() + searchGap, area.w,
                    area.bottom() - searchField.bottom() - searchGap};
    if (context_.background)
        canvas.restoreFrom(*context_.background,
                           Rect{area.x, area.y, area.w, searchField.bottom() - area.y});
    theme.bold().draw(canvas, area.x, area.y, "Home", theme.sizeHeading(), theme.textPrimary);
    search_.renderField(canvas, theme, searchField);

    if (search_.keyboardVisible()) {
        search_.renderKeyboard(canvas, theme, body);
        keyboardWasVisible_ = true;
        return;
    }
    const bool keyboardClosed = keyboardWasVisible_;
    if (keyboardClosed && context_.background) canvas.restoreFrom(*context_.background, body);
    keyboardWasVisible_ = false;

    if (search_.hasQuery()) {
        if (searchBuildPending_ || !searchIndexReady_) {
            theme.regular().draw(canvas, body.x + theme.px(18), body.y + theme.px(10),
                                 "Preparing matching games…", theme.sizeSmall(), theme.textMuted);
        } else {
            searchResults_.renderBody(canvas, body, fullRedraw || keyboardClosed);
        }
        return;
    }

    const Metrics m = metrics();
    const int recentHeight = rows_[0].items.empty() ? theme.px(66) : m.rowHeight;
    const int laneWidth = body.w - 2 * m.growX;
    favoriteColumns_ = std::max(1, (laneWidth + theme.gap()) /
                                      (m.tileWidth + theme.gap()));
    const int favoriteRows = (int(rows_[1].items.size()) + favoriteColumns_ - 1) /
                             favoriteColumns_;
    const int favoriteHeight = rows_[1].items.empty()
                                   ? theme.px(66)
                                   : theme.px(40) + 2 * m.growY +
                                         favoriteRows * (m.tileHeight + m.caption + theme.gap());
    const int contentHeight = recentHeight + favoriteHeight;

    // Scroll the entire Home page until the selected grid tile is fully visible.
    if (activeRow_ == 0) {
        pageScroll_ = 0;
    } else if (!rows_[1].items.empty()) {
        const int step = m.tileHeight + m.caption + theme.gap();
        const int top = recentHeight + theme.px(40) +
                        (rows_[1].cursor / favoriteColumns_) * step;
        const int bottom = top + step + m.growY;
        if (top < pageScroll_) pageScroll_ = top;
        if (bottom > pageScroll_ + body.h) pageScroll_ = bottom - body.h;
    }
    pageScroll_ = std::min(std::max(0, pageScroll_), std::max(0, contentHeight - body.h));

    // Asking here, before anything decides whether to repaint, is what keeps artwork
    // loading on a screen that has otherwise gone still: the request (and any decode it
    // triggers) used to live inside renderRow(), which only ran when something had
    // already changed — so once the focus animation settled, nothing ever asked again,
    // and any picture that missed the decode budget by then stayed missing forever.
    bool revealActive = requestImages(rows_[0], body.w, activeRow_ == 0);
    revealActive = requestFavoriteImages(
                       body,
                       body.y + recentHeight - pageScroll_ + theme.px(40) + m.growY, m) ||
                   revealActive;

    // Taken after that request, so a decode it just triggered still counts as a change
    // this frame rather than waiting until the next one.
    const uint64_t imagesBefore = context_.images.generation();

    // The caller may have already wiped the canvas this frame for a reason of its own — a
    // tab switch, returning from a detail view — in which case skipping our own repaint
    // because nothing *we* track has changed would leave that wipe on screen.
    bool anyChange = needsPaint_ || fullRedraw || keyboardClosed || revealActive ||
                     pageScroll_ != paintedPageScroll_ ||
                     imagesBefore != paintedImages_;
    for (size_t r = 0; r < rows_.size() && !anyChange; ++r)
        anyChange = rowChanged(rows_[r], int(r) == activeRow_);

    if (!anyChange) return;

    // Focus growth can reach the next section, so repaint the entire body.
    if (context_.background) canvas.restoreFrom(*context_.background, body);

    canvas.pushClip(body);

    const Rect recentArea{body.x, body.y - pageScroll_, body.w,
                          recentHeight - theme.gap()};
    if (recentArea.bottom() >= body.y && recentArea.y <= body.bottom())
        renderRow(canvas, recentArea, rows_[0], activeRow_ == 0);

    const int favoriteY = body.y + recentHeight - pageScroll_;
    if (favoriteY + favoriteHeight >= body.y && favoriteY <= body.bottom())
        renderFavorites(canvas, body, favoriteY, m);

    for (size_t r = 0; r < rows_.size(); ++r) {
        Row &row = rows_[r];
        row.paintedFocus = row.focus;
        row.paintedScroll = row.scroll;
        row.paintedCount = row.items.size();
        row.paintedActive = int(r) == activeRow_;
    }

    canvas.popClip();
    paintedPageScroll_ = pageScroll_;
    paintedImages_ = imagesBefore;
    needsPaint_ = false;
}

std::string HomeScreen::hints() const {
    std::string hints = "A Play game   Hold X 2s to favorite   L2/R2 Jump to letter   {DPAD} Move";
    if (search_.hasQuery() && context_.preferences.defaultView() == "individual")
        hints += "   Y Change view";
    return search_.hints(hints);
}
