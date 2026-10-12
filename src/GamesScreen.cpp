#include "GamesScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <unordered_set>

#include "Alphabet.h"
#include "ButtonLabels.h"
#include "Canvas.h"
#include "DebugLog.h"
#include "Tile.h"

namespace {

struct ViewSpec {
    int targetTileWidth;   // design pixels
    int aspectW, aspectH;
    bool coverArt;
    bool showLabel;
    const char *name;
};

// Square, deliberately. Box art is portrait on some systems and landscape on others, and the
// artwork is fitted inside the tile with its own proportions kept either way. A portrait tile
// wastes a band down both sides of every landscape cover; a square one is the shape that
// suits both worst-cases equally — and it is shorter, so more rows fit on screen.
ViewSpec specFor(GameView view) {
    switch (view) {
    case GameView::Grid:        return {250, 1, 1, true, true, "Grid"};
    case GameView::BoxartSmall: return {150, 1, 1, true, false, "Boxart small"};
    case GameView::List:
    default:                    return {0, 0, 0, false, false, "List"};
    }
}

ArtworkVariant artworkVariantFor(GameView view, bool loadSmallBoxartsInGridView) {
    switch (view) {
    case GameView::List: return ArtworkVariant::Detail;
    case GameView::BoxartSmall:
        return ArtworkVariant::Small;
    case GameView::Grid:
        return loadSmallBoxartsInGridView ? ArtworkVariant::Small : ArtworkVariant::Grid;
    default: return ArtworkVariant::Grid;
    }
}

std::vector<std::string> wrapText(Font &font, const std::string &text, int size, int maxWidth) {
    std::istringstream words(text);
    std::vector<std::string> lines;
    std::string word;
    std::string line;
    while (words >> word) {
        if (font.measure(word, size) > maxWidth) word = font.elide(word, size, maxWidth);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && font.measure(candidate, size) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

} // namespace

GameView gameViewFromName(const std::string &name) {
    if (name == "list") return GameView::List;
    if (name == "small") return GameView::BoxartSmall;
    return GameView::Grid;
}

const char *nameForGameView(GameView view) {
    switch (view) {
    case GameView::List: return "list";
    case GameView::BoxartSmall: return "small";
    case GameView::Grid:
    default: return "grid";
    }
}

const char *displayNameForGameView(GameView view) { return specFor(view).name; }

GamesScreen::GamesScreen(Context &context) : context_(context) {}

void GamesScreen::setView(GameView view) {
    view_ = view;
    scrollRow_ = 0;
    needsPaint_ = true;
}

void GamesScreen::showSystem(const GameSystem &system, const std::string &title) {
    system_ = &system;
    favoritesMode_ = false;
    allMode_ = false;
    searchResultsMode_ = false;
    title_ = title.empty() ? system.name : title;
    reload();
}

void GamesScreen::showFavorites() {
    system_ = nullptr;
    favoritesMode_ = true;
    allMode_ = false;
    searchResultsMode_ = false;
    title_ = "Favorites";
    reload();
}

void GamesScreen::showAllGames() {
    system_ = nullptr;
    favoritesMode_ = false;
    allMode_ = true;
    searchResultsMode_ = false;
    title_ = "All games";
    reload();
}

void GamesScreen::showSearchResults(
    const std::vector<std::pair<const GameSystem *, Game>> &games) {
    system_ = nullptr;
    favoritesMode_ = false;
    allMode_ = false;
    searchResultsMode_ = true;
    title_ = "Search results";
    entries_.clear();
    entries_.reserve(games.size());
    for (const auto &item : games)
        if (item.first)
            entries_.push_back({item.first, item.second, false, ArtworkVariant::Full, {}});
    unfilteredEntries_ = entries_;
    if (entries_.empty())
        searchEmptyMessage_ = "No games match this search. Try a different title or a shorter search.";
    std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) {
        return strcasecmp(a.game.name.c_str(), b.game.name.c_str()) < 0;
    });
    focus_.assign(entries_.size(), 0.0f);
    cursor_ = 0;
    scrollRow_ = 0;
    paintedFocus_.clear();
    paintedArtwork_.clear();
    paintedScrollRow_ = -1;
    paintedCount_ = size_t(-1);
    needsPaint_ = true;
}

void GamesScreen::reload() {
    entries_.clear();
    focus_.clear();
    cursor_ = 0;
    scrollRow_ = 0;

    // Every entry starts as a stub: path and name, no artwork, no I/O (see
    // Library::makeStub). That is enough for sorting, the letter jump and the header count,
    // and cheap enough to do for the whole 10,500-game library in one go, synchronously —
    // resolving artwork for all of it up front, not just what gets shown, is what used to
    // make opening a big system look like a hang. Artwork is resolved lazily, per entry, the
    // moment something is actually about to draw it — see ensureArtwork().
    bool preSorted = false;
    if (favoritesMode_) {
        std::map<std::string, std::unordered_set<std::string>> favoritePaths;
        for (const FavoriteEntry &favorite : context_.favorites.entries())
            favoritePaths[favorite.system].insert(favorite.path);
        for (const auto &bucket : favoritePaths) {
            const GameSystem *system = context_.library.findSystem(bucket.first);
            if (!system) continue;
            if (context_.preferences.organizeGameVersions()) {
                for (const Game &game : context_.library.gamesOf(*system)) {
                    bool favored = bucket.second.count(game.path) != 0;
                    for (const Game::Variant &version : game.versions)
                        favored = favored || bucket.second.count(version.path) != 0;
                    if (favored) entries_.push_back({system, game, false,
                                                     ArtworkVariant::Full, {}});
                }
            } else {
                for (const std::string &path : bucket.second)
                    if (!Library::isSystemFile(*system, path))
                        entries_.push_back({system, Library::makeStub(*system, path), false,
                                            ArtworkVariant::Full, {}});
            }
        }
    } else if (allMode_) {
        for (const GameSystem &system : context_.library.systems())
            for (Game game : context_.library.gamesOf(system, context_.preferences.organizeGameVersions()))
                entries_.push_back({&system, std::move(game), false, ArtworkVariant::Full, {}});
    } else if (system_) {
        for (Game game : context_.library.gamesOf(*system_, context_.preferences.organizeGameVersions()))
            entries_.push_back({system_, std::move(game), false, ArtworkVariant::Full, {}});
        preSorted = context_.library.pathsPreSorted(*system_);
    }

    // Merging several systems' worth of paths (or a source that was never sorted by display
    // name to begin with) means whatever order they arrived in is not the display order.
    if (!preSorted) {
        std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) {
            return strcasecmp(a.game.name.c_str(), b.game.name.c_str()) < 0;
        });
    }

    unfilteredEntries_ = entries_;
    if (search_.hasQuery()) applySearch(search_.query());

    focus_.assign(entries_.size(), 0.0f);
    paintedFocus_.clear();
    paintedArtwork_.clear();
    paintedScrollRow_ = -1;
    paintedCount_ = size_t(-1);
    needsPaint_ = true;

    char line[160];
    std::snprintf(line, sizeof(line), "games: %zu entries for %s", entries_.size(),
                 title_.c_str());
    DebugLog::info(line);
}

void GamesScreen::applySearch(const std::string &query) {
    entries_.clear();
    if (query.empty()) {
        entries_ = unfilteredEntries_;
        searchResultsMode_ = false;
    } else {
        searchResultsMode_ = true;
        for (const Entry &entry : unfilteredEntries_) {
            if (entry.system && GameSearch::matches(entry.game.name, entry.system->name,
                                                    query, allMode_ || favoritesMode_))
                entries_.push_back(entry);
        }
        if (entries_.empty())
            searchEmptyMessage_ = "No games match this search. Try a different title or a shorter search.";
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) {
        return strcasecmp(a.game.name.c_str(), b.game.name.c_str()) < 0;
    });
    focus_.assign(entries_.size(), 0.0f);
    cursor_ = 0;
    scrollRow_ = 0;
    paintedFocus_.clear();
    paintedArtwork_.clear();
    paintedScrollRow_ = -1;
    paintedCount_ = size_t(-1);
    needsPaint_ = true;
}

void GamesScreen::setSearchQuery(const std::string &query) {
    search_.setQuery(query);
    applySearch(query);
}

void GamesScreen::ensureArtwork(Entry &entry, ArtworkVariant variant) {
    if (entry.artworkResolved && entry.artworkVariant == variant) return;
    Library::resolveArtwork(*entry.system, entry.game, variant);
    entry.artworkResolved = true;
    entry.artworkVariant = variant;
}

const GamesScreen::Entry *GamesScreen::current() const {
    if (entries_.empty()) return nullptr;
    const int index = std::min(std::max(cursor_, 0), int(entries_.size()) - 1);
    return &entries_[size_t(index)];
}

std::string GamesScreen::favoritePath() const {
    const Entry *entry = current();
    return entry ? entry->game.path : std::string();
}

bool GamesScreen::selectedGame(const GameSystem *&system, Game &game) const {
    const Entry *entry = current();
    if (!entry) return false;
    system = entry->system;
    game = entry->game;
    return true;
}

bool GamesScreen::canMoveUp() const {
    if (entries_.empty() || cursor_ <= 0) return false;
    if (view_ == GameView::List) return true;
    return grid_.move(cursor_, int(entries_.size()), 0, -1) != cursor_;
}

void GamesScreen::moveCursor(int dx, int dy) {
    if (entries_.empty()) return;

    if (view_ == GameView::List) {
        const int count = int(entries_.size());
        if (dy) cursor_ = std::min(std::max(cursor_ + dy, 0), count - 1);
        if (dx) cursor_ = std::min(std::max(cursor_ + dx * 10, 0), count - 1);
        return;
    }

    cursor_ = grid_.move(cursor_, int(entries_.size()), dx, dy);
}

void GamesScreen::jumpLetter(int direction) {
    const int count = int(entries_.size());
    const int target = Alphabet::jump(cursor_, count, direction,
                                      [this](int i) -> const std::string & {
                                          return entries_[size_t(i)].game.name;
                                      });
    if (target == cursor_) return;

    cursor_ = target;

    // Put the new letter at the top of the screen. Plain clamping would pin the cursor to
    // whichever edge it came from, which makes a jump look like a single step.
    scrollRow_ = (view_ == GameView::List) ? cursor_
                                           : cursor_ / std::max(1, grid_.columns());

    const char letter = Alphabet::initial(entries_[size_t(cursor_)].game.name);
    context_.notify(std::string("Games starting with ") +
                    (letter == '#' ? "0–9" : std::string(1, letter)), 1.2f);
}

void GamesScreen::toggleFavorite() {
    const Entry *entry = current();
    if (!entry) return;

    bool wasFavorite = context_.favorites.contains(entry->game.path);
    if (context_.preferences.organizeGameVersions())
        for (const Game::Variant &version : entry->game.versions)
            wasFavorite = wasFavorite || context_.favorites.contains(version.path);
    if (wasFavorite && !entry->game.versions.empty()) {
        if (context_.favorites.contains(entry->game.path))
            context_.favorites.toggle(entry->system->name, entry->game.path, entry->game.name);
        for (const Game::Variant &version : entry->game.versions)
            if (context_.favorites.contains(version.path))
                context_.favorites.toggle(entry->system->name, version.path, entry->game.name);
    } else {
        context_.favorites.toggle(entry->system->name, entry->game.path, entry->game.name);
    }
    needsPaint_ = true;
    context_.notify(wasFavorite ? entry->game.name + " removed from favorites"
                                : entry->game.name + " added to favorites");

    if (favoritesMode_) {
        const int previous = cursor_;
        reload();
        cursor_ = std::min(previous, int(entries_.size()) - 1);
        if (cursor_ < 0) cursor_ = 0;
    }
}

void GamesScreen::launch() {
    const Entry *entry = current();
    if (!entry) return;

    Game selected = entry->game;
    if (context_.preferences.organizeGameVersions() && selected.versions.size() > 1) {
        std::vector<std::string> candidates;
        for (const Game::Variant &variant : selected.versions) candidates.push_back(variant.path);
        const std::string recent = context_.history.mostRecentVersion(
            entry->system->name, selected.name, candidates);
        if (recent.empty()) {
            if (context_.onChooseGameVersion) context_.onChooseGameVersion(*entry->system, selected);
            return;
        }
        selected.path = recent;
    }

    if (context_.launcher.launchGame(*entry->system, selected)) {
        context_.history.remember(entry->system->name, selected.path, selected.name);
        context_.notify("Starting " + selected.name);
        context_.standDown = true;
    }
    else {
        context_.showError("Could not start " + selected.name, context_.launcher.lastError());
    }
}

void GamesScreen::handle(Action action) {
    if (search_.handle(action, !canMoveUp(),
                       [this](const std::string &query) { applySearch(query); })) return;

    switch (action) {
    case Action::Up:    moveCursor(0, -1); break;
    case Action::Down:  moveCursor(0, 1); break;
    case Action::Left:  moveCursor(-1, 0); break;
    case Action::Right: moveCursor(1, 0); break;
    case Action::Confirm: launch(); break;
    case Action::Start: launch(); break;
    case Action::ToggleFavorite: toggleFavorite(); break;
    case Action::JumpPrev: jumpLetter(-1); break;
    case Action::JumpNext: jumpLetter(1); break;
    case Action::CycleView:
        setView(static_cast<GameView>((int(view_) + 1) % int(GameView::kCount)));
        if (viewChanged_) viewChanged_(view_);
        context_.notify(std::string("Game view: ") + specFor(view_).name, 2.0f);
        break;
    default:
        break;
    }
}

void GamesScreen::update(float deltaSeconds) {
    // The current entry's artwork is what the detail-fade tracking below reads, and in the
    // grid views it is what the focus animation grows — both need it resolved before this
    // frame, not whenever renderGrid() next happens to ask for it.
    if (context_.preferences.showBoxArt() && !entries_.empty()) {
        const int index = std::min(std::max(cursor_, 0), int(entries_.size()) - 1);
        ensureArtwork(entries_[size_t(index)], artworkVariantFor(
            view_, context_.preferences.loadSmallBoxartsInGridView()));
    }

    const float speed = std::min(1.0f, deltaSeconds * 9.0f);
    for (size_t i = 0; i < focus_.size(); ++i) {
        const float target = (int(i) == cursor_) ? 1.0f : 0.0f;
        focus_[i] += (target - focus_[i]) * speed;
    }

    const Entry *entry = current();
    const std::string path = entry ? entry->game.background : std::string();
    if (path != detailPath_) {
        detailPath_ = path;
        detailFade_ = 0.0f;
    }
    detailFade_ += (1.0f - detailFade_) * std::min(1.0f, deltaSeconds * 5.0f);
}

void GamesScreen::renderHeader(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;

    theme.bold().draw(canvas, area.x, area.y, title_, theme.sizeHeading(), theme.textPrimary);

    // The position, not just the total: in a list of a few thousand it is the only cue for
    // how far along the alphabet the cursor sits.
    char info[96];
    std::snprintf(info, sizeof(info), "%d / %d  ·  %s", entries_.empty() ? 0 : cursor_ + 1,
                  int(entries_.size()), specFor(view_).name);
    const int width = theme.regular().measure(info, theme.sizeBody());
    theme.regular().draw(canvas, area.right() - width, area.y + theme.px(10), info,
                         theme.sizeBody(), theme.textMuted.withAlpha(160));
}

void GamesScreen::renderDetail(Canvas &canvas, const Rect &area, const Entry &entry, bool force) {
    Theme &theme = context_.theme;

    const int pad = theme.px(24);
    const int footerHeight = favoritesMode_ ? theme.px(112) : 0;
    const Rect artArea{area.x + pad, area.y + pad,
                       std::max(0, area.w - 2 * pad),
                       std::max(0, area.h - 2 * pad - footerHeight)};

    // Request only the selected game's cover. Detail backgrounds are reserved for a future
    // dedicated game detail page, where they can be useful without slowing down this view.
    ImagePtr art;
    if (context_.preferences.showBoxArt())
        art = context_.images.requestAsyncCandidates(entry.game.boxartCandidates,
                                                     artArea.w, artArea.h, true);

    const bool boxArtEnabled = context_.preferences.showBoxArt();
    const bool loading = boxArtEnabled && context_.images.isPendingCandidates(
                                               entry.game.boxartCandidates,
                                               artArea.w, artArea.h);
    const bool changed = force || !paintedDetail_ || paintedDetailPath_ != entry.game.path ||
                         paintedDetailArt_ != art.get() || paintedDetailLoading_ != loading ||
                         paintedDetailBoxArtEnabled_ != boxArtEnabled;
    if (!changed) return;

    if (context_.background) canvas.restoreFrom(*context_.background, area);

    canvas.fillRoundedRect(area, theme.radius(), theme.backgroundLo.withAlpha(150));

    if (art) {
        const Rect target = artArea.fitAspect(art->width(), art->height());
        canvas.pushClip(area);
        canvas.dropShadow(target, theme.px(6), theme.px(18), theme.shadow.withAlpha(170));
        canvas.drawImage(*art, target, 255, theme.px(6));
        canvas.popClip();
    }

    if (!art) {
        canvas.fillRoundedRect(artArea, theme.radius(), theme.surface.withAlpha(120));
        if (loading) {
            theme.regular().drawCentered(canvas, artArea, "Loading cover art...",
                                         theme.sizeBody(), theme.textMuted.withAlpha(180));
        } else if (!favoritesMode_) {
            const int titleSize = theme.sizeGameDetail();
            const int maxWidth = std::max(theme.px(40), artArea.w - theme.px(32));
            std::vector<std::string> lines = wrapText(theme.bold(), entry.game.name,
                                                       titleSize, maxWidth);
            const int lineHeight = theme.bold().lineHeight(titleSize);
            const int maxLines = std::max(1, artArea.h / std::max(1, lineHeight));
            if (int(lines.size()) > maxLines) {
                lines.resize(size_t(maxLines));
                lines.back() = theme.bold().elide(lines.back() + "...", titleSize, maxWidth);
            }
            const int blockHeight = int(lines.size()) * lineHeight;
            int y = artArea.y + (artArea.h - blockHeight) / 2;
            for (const std::string &line : lines) {
                const int width = theme.bold().measure(line, titleSize);
                theme.bold().draw(canvas, artArea.x + (artArea.w - width) / 2, y, line,
                                  titleSize, theme.textPrimary);
                y += lineHeight;
            }
        }
    }

    if (favoritesMode_) {
        const int titleSize = theme.sizeGameDetail();
        const int lineHeight = theme.bold().lineHeight(titleSize);
        const int systemSize = theme.sizeBody();
        const int systemHeight = theme.regular().lineHeight(systemSize);
        const int gap = theme.px(4);
        const Rect footer{area.x + pad, artArea.bottom() + theme.px(4),
                          std::max(0, area.w - 2 * pad), footerHeight - theme.px(4)};
        const Rect titleArea{footer.x, footer.y, footer.w,
                             std::max(0, footer.h - systemHeight - gap)};
        const int maxWidth = std::max(theme.px(40), titleArea.w - theme.px(16));
        std::vector<std::string> lines = wrapText(theme.bold(), entry.game.name,
                                                   titleSize, maxWidth);
        const int maxLines = std::max(1, titleArea.h / std::max(1, lineHeight));
        if (int(lines.size()) > maxLines) {
            lines.resize(size_t(maxLines));
            lines.back() = theme.bold().elide(lines.back() + "...", titleSize, maxWidth);
        }
        const int blockHeight = int(lines.size()) * lineHeight;
        int y = titleArea.y + (titleArea.h - blockHeight) / 2;
        for (const std::string &line : lines) {
            const int width = theme.bold().measure(line, titleSize);
            theme.bold().draw(canvas, titleArea.x + (titleArea.w - width) / 2, y, line,
                              titleSize, theme.textPrimary);
            y += lineHeight;
        }
        const Rect systemArea{footer.x, footer.bottom() - systemHeight, footer.w, systemHeight};
        const std::string system = theme.regular().elide(entry.system->name, systemSize,
                                                         footer.w - theme.px(16));
        theme.regular().drawCentered(canvas, systemArea, system, systemSize,
                                     theme.textMuted.withAlpha(190));
    }

    paintedDetailPath_ = entry.game.path;
    paintedDetailArt_ = art.get();
    paintedDetailLoading_ = loading;
    paintedDetailBoxArtEnabled_ = boxArtEnabled;
    paintedDetail_ = true;
}

void GamesScreen::renderDetailBackground(Canvas &canvas, const Rect &area, const Entry &entry) {
    Theme &theme = context_.theme;
    if (!context_.preferences.showBoxArt()) return;

    if (ImagePtr background = context_.images.requestAsyncCandidates(
            entry.game.backgroundCandidates, area.w, area.h, false)) {
        // Fill and crop the panel so no bars appear beside the artwork.
        const long scaleW = long(area.w) * background->height();
        const long scaleH = long(area.h) * background->width();
        const int coverW = (scaleW > scaleH) ? area.w
                                             : int(long(background->width()) * area.h /
                                                   background->height());
        const int coverH = (scaleW > scaleH) ? int(long(background->height()) * area.w /
                                                   background->width())
                                             : area.h;
        const Rect cover{area.x + (area.w - coverW) / 2,
                         area.y + (area.h - coverH) / 2, coverW, coverH};

        canvas.pushClip(area);
        canvas.drawImage(*background, cover, uint8_t(72 * detailFade_), 0);
        canvas.popClip();
    }
}

void GamesScreen::renderList(Canvas &canvas, const Rect &area, bool fullRedraw) {
    Theme &theme = context_.theme;

    const int detailWidth = std::max(0, (area.w - theme.gap()) / 2);
    const Rect listArea{area.x, area.y, detailWidth, area.h};
    const Rect detailArea{listArea.right() + theme.gap(), area.y, detailWidth, area.h};

    const int rowHeight = theme.px(favoritesMode_ ? 68 : 52);
    const int visible = std::max(1, listArea.h / rowHeight);

    if (cursor_ < scrollRow_) scrollRow_ = cursor_;
    if (cursor_ >= scrollRow_ + visible) scrollRow_ = cursor_ - visible + 1;
    const int maxScroll = std::max(0, int(entries_.size()) - visible);
    scrollRow_ = std::min(std::max(0, scrollRow_), maxScroll);

    const bool layoutChanged = fullRedraw || needsPaint_ ||
                               scrollRow_ != paintedListScrollRow_ ||
                               entries_.size() != paintedListCount_ ||
                               paintedListFocus_.size() != focus_.size();
    if (paintedListFocus_.size() != focus_.size())
        paintedListFocus_.resize(focus_.size(), -1.0f);
    canvas.pushClip(listArea);
    for (int i = scrollRow_; i < std::min(int(entries_.size()), scrollRow_ + visible); ++i) {
        const Entry &entry = entries_[size_t(i)];
        const Rect row{listArea.x, listArea.y + (i - scrollRow_) * rowHeight, listArea.w,
                       rowHeight - theme.px(6)};
        const float focus = focus_[size_t(i)];
        const float paintedFocus = paintedListFocus_[size_t(i)];
        const bool focusChanged = std::fabs(focus - paintedFocus) >= 0.002f ||
                                  ((focus > 0.01f) != (paintedFocus > 0.01f));
        if (!layoutChanged && !focusChanged) continue;
        if (!fullRedraw && context_.background) {
            const int borderBleed = std::max(1, theme.px(3));
            const Rect repaint{row.x - borderBleed, row.y - borderBleed,
                               row.w + 2 * borderBleed, row.h + 2 * borderBleed};
            canvas.restoreFrom(*context_.background, repaint);
        }

        if (focus > 0.01f) {
            canvas.fillRoundedRect(row, theme.px(8),
                                   Color::lerp(theme.surface, theme.surfaceHi, focus)
                                       .withAlpha(uint8_t(220 * focus)));
            canvas.fillRoundedRect({row.x, row.y, theme.px(4), row.h}, theme.px(2),
                                   theme.accent.withAlpha(uint8_t(255 * focus)));
            canvas.strokeRoundedRect(row, theme.px(8), std::max(1, theme.px(2)),
                                     theme.focus.withAlpha(uint8_t(255 * focus)));
        }

        const bool isFavorite = context_.favorites.contains(entry.game.path);
        const int textX = row.x + theme.px(20);
        const int textW = row.w - theme.px(40) - (isFavorite ? theme.px(28) : 0);

        const int nameSize = theme.sizeGameLabel();
        const int nameHeight = theme.regular().lineHeight(nameSize);
        const std::string name = theme.regular().elide(entry.game.name, nameSize, textW);
        if (favoritesMode_) {
            const int systemSize = theme.sizeBody();
            const int systemHeight = theme.regular().lineHeight(systemSize);
            const int gap = theme.px(2);
            const int blockHeight = nameHeight + gap + systemHeight;
            const int textY = row.y + (row.h - blockHeight) / 2;
            theme.regular().draw(canvas, textX, textY, name, nameSize,
                                 Color::lerp(theme.textMuted, theme.textPrimary, focus));
            const std::string system = theme.regular().elide(entry.system->name, systemSize,
                                                              textW);
            theme.regular().draw(canvas, textX, textY + nameHeight + gap, system, systemSize,
                                 theme.textMuted.withAlpha(190));
        } else {
            theme.regular().draw(canvas, textX,
                                 row.y + (row.h - nameHeight) / 2, name, nameSize,
                                 Color::lerp(theme.textMuted, theme.textPrimary, focus));
        }

        if (isFavorite) {
            theme.bold().draw(canvas, row.right() - theme.px(26),
                              row.y + (row.h - theme.bold().lineHeight(theme.sizeBody())) / 2,
                              "*", theme.sizeBody(), theme.favorite);
        }
        paintedListFocus_[size_t(i)] = focus;
    }
    canvas.popClip();

    if (const Entry *entry = current()) renderDetail(canvas, detailArea, *entry,
                                                      fullRedraw || needsPaint_);
    paintedListScrollRow_ = scrollRow_;
    paintedListCount_ = entries_.size();
    needsPaint_ = false;
}

void GamesScreen::renderGrid(Canvas &canvas, const Rect &area, bool fullRedraw) {
    Theme &theme = context_.theme;
    const ViewSpec spec = specFor(view_);

    const int labelHeight = Tile::captionHeight(theme, spec.showLabel, false);

    // Reserve what a focused tile actually needs rather than a fixed guess: it grows by a
    // proportion of its own size, so the margin has to follow the tile size too. Measured
    // from a tile of the target width, which is within a few pixels of the final one.
    const Rect probe{0, 0, theme.px(spec.targetTileWidth),
                     theme.px(spec.targetTileWidth) * spec.aspectH /
                         std::max(1, spec.aspectW)};
    const int growth = std::max(Tile::focusMarginX(probe), Tile::focusMarginY(probe)) + theme.px(3);

    grid_.configure(area.inset(growth), theme, spec.targetTileWidth, spec.aspectW, spec.aspectH,
                    labelHeight);
    grid_.centerContent(int(entries_.size()));
    scrollRow_ = grid_.clampScroll(cursor_, int(entries_.size()), scrollRow_);

    const int first = scrollRow_ * grid_.columns();
    // Two rows beyond the last full one: the first may be only partly visible, and drawing it
    // is what shows the list continuing past the bottom edge.
    const int last = std::min(int(entries_.size()),
                              first + grid_.columns() * (grid_.visibleRows() + 2));

    canvas.pushClip(area);

    // Build candidates for newly visible entries with string operations only. The worker does
    // the existence checks and file loads so they cannot stall scrolling.
    const bool useSmallGridArt = view_ == GameView::Grid &&
                                 context_.preferences.loadSmallBoxartsInGridView();
    const ArtworkVariant variant = artworkVariantFor(
        view_, context_.preferences.loadSmallBoxartsInGridView());
    if (context_.preferences.showBoxArt()) {
        for (int i = first; i < last; ++i)
            ensureArtwork(entries_[size_t(i)], variant);
    }

    // Grid is using the prepared small image at native size for a loading-speed comparison.
    // Keep JPEG fallbacks bounded to the same source dimensions so they are not enlarged either.
    const int artworkWidth = useSmallGridArt
                                 ? std::min(grid_.tileWidth(),
                                            artworkBounds(ArtworkVariant::Small).width)
                                 : grid_.tileWidth();
    const int artworkHeight = useSmallGridArt
                                  ? std::max(1, std::min(grid_.tileHeight() - labelHeight,
                                            artworkBounds(ArtworkVariant::Small).height))
                                  : grid_.tileHeight();

    // Ask for the focused tile's picture before anything else. The two passes below draw it
    // last on purpose — so its shadow and grown edge sit above its neighbours rather than
    // under them — but that made its own request the last one issued too, and the per-frame
    // decode budget is normally spent by the first cache miss it meets. On a screen full of
    // misses the focused tile lost every single time; asking here first means it never has to
    // wait on whichever neighbour happened to be drawn before it.
    if (context_.preferences.showBoxArt() && cursor_ >= first && cursor_ < last) {
        const Entry &focused = entries_[size_t(cursor_)];
        context_.images.requestAsyncCandidates(focused.game.boxartCandidates,
                                               artworkWidth, artworkHeight, true);
    }

    // Keep the current image per visible tile. A cache-wide generation counter makes one
    // arriving cover repaint every tile; comparing these pointers limits that repaint to the
    // cover that actually changed.
    std::vector<ImagePtr> visibleArtwork(size_t(last - first));
    if (context_.preferences.showBoxArt()) {
        for (int i = first; i < last; ++i) {
            const Entry &entry = entries_[size_t(i)];
            visibleArtwork[size_t(i - first)] = context_.images.requestAsyncCandidates(
                entry.game.boxartCandidates, artworkWidth, artworkHeight, i == cursor_);
        }
    }

    bool revealActive = false;
    for (int i = first; i < last; ++i) {
        Entry &entry = entries_[size_t(i)];
        const ImagePtr &art = visibleArtwork[size_t(i - first)];
        const Image *previousArt = entry.boxartFadeIn.image();
        if (entry.boxartFadeIn.observe(art, i == cursor_)) {
            if (art || previousArt) {
                char message[128];
                std::snprintf(message, sizeof(message),
                              "games grid: cover cache identity changed at entry %d (%s -> %s)",
                              i, previousArt ? "image" : "empty",
                              art ? "image" : "empty");
                DebugLog::info(message);
            }
        }
        revealActive = revealActive || entry.boxartFadeIn.active();
    }

    const bool layoutChanged = fullRedraw || needsPaint_ || scrollRow_ != paintedScrollRow_ ||
                               entries_.size() != paintedCount_ ||
                               paintedFocus_.size() != focus_.size() ||
                               paintedArtwork_.size() != entries_.size() ||
                               grid_.columns() != paintedColumns_ ||
                               grid_.tileWidth() != paintedTileWidth_ ||
                               grid_.tileHeight() != paintedTileHeight_;

    Rect repaint{};
    if (!layoutChanged) {
        for (int i = first; i < last; ++i) {
            const bool focusChanged =
                std::fabs(focus_[size_t(i)] - paintedFocus_[size_t(i)]) >= 0.002f;
            const bool artworkChanged =
                paintedArtwork_[size_t(i)] != visibleArtwork[size_t(i - first)].get();
            const bool revealChanged = i != cursor_ &&
                                       entries_[size_t(i)].boxartFadeIn.active();
            if (!focusChanged && !artworkChanged && !revealChanged) continue;
            const Rect footprint = Tile::footprint(theme, grid_.cellFrame(i, scrollRow_));
            const int x = std::min(repaint.x, footprint.x);
            const int y = std::min(repaint.y, footprint.y);
            const int right = std::max(repaint.right(), footprint.right());
            const int bottom = std::max(repaint.bottom(), footprint.bottom());
            repaint = repaint.empty() ? footprint : Rect{x, y, right - x, bottom - y};
        }
        if (repaint.empty()) {
            canvas.popClip();
            return;
        }
        repaint = repaint.intersect(area);
        if (repaint.empty()) {
            canvas.popClip();
            return;
        }
    } else {
        repaint = area;
    }

    // GamesScreen used to clear and repaint every visible cover every frame. The image bytes
    // were cached, but this rebuilt all of their pixels and copied the full grid to the
    // framebuffer continuously. Restore only the area affected by focus animation; new
    // artwork, scrolling and layout changes repaint the complete grid once. An arriving cover
    // only dirties that tile and its overlapping shadow.
    if (!fullRedraw && context_.background) canvas.restoreFrom(*context_.background, repaint);

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = first; i < last; ++i) {
            const bool isCursor = (i == cursor_);
            if ((pass == 0) == isCursor) continue;

            Entry &entry = entries_[size_t(i)];
            const Rect frame = grid_.cellFrame(i, scrollRow_);
            if (!layoutChanged && Tile::footprint(theme, frame).intersect(repaint).empty())
                continue;

            Tile::Content content;
            content.label = entry.game.name;
            content.showLabel = spec.showLabel;
            content.coverArt = spec.coverArt;
            content.nativeImageSize = useSmallGridArt;
            content.compactPlaceholder = useSmallGridArt;
            content.placeholderWidth = artworkWidth;
            content.placeholderHeight = artworkHeight;
            content.favorite = context_.favorites.contains(entry.game.path);
            content.image = visibleArtwork[size_t(i - first)];
            content.imageRevealPercent = entry.boxartFadeIn.revealPercent();

            Tile::draw(canvas, theme, frame, content, focus_[size_t(i)]);
            entry.boxartFadeIn.advance();
        }
    }
    canvas.popClip();

    paintedFocus_ = focus_;
    if (layoutChanged) paintedArtwork_.assign(entries_.size(), nullptr);
    for (int i = first; i < last; ++i)
        paintedArtwork_[size_t(i)] = visibleArtwork[size_t(i - first)].get();
    paintedScrollRow_ = scrollRow_;
    paintedCount_ = entries_.size();
    paintedColumns_ = grid_.columns();
    paintedTileWidth_ = grid_.tileWidth();
    paintedTileHeight_ = grid_.tileHeight();
    needsPaint_ = false;
}

void GamesScreen::render(Canvas &canvas, const Rect &area, bool fullRedraw) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    const Rect header{area.x, area.y, area.w, headerHeight};
    const int searchHeight = theme.px(54);
    const int searchGap = theme.px(20);
    const Rect searchField{area.x, area.y + headerHeight + theme.px(2), area.w,
                           searchHeight - theme.px(4)};
    const Rect body{area.x, searchField.bottom() + searchGap, area.w,
                    area.bottom() - searchField.bottom() - searchGap};

    // Incremental grid mode leaves the static screen area in place, so restore its header
    // before drawing text with antialiased edges again.
    if (incremental() && context_.background)
        canvas.restoreFrom(*context_.background,
                           Rect{area.x, area.y, area.w, searchField.bottom() - area.y});
    renderHeader(canvas, header);
    search_.renderField(canvas, theme, searchField);

    if (search_.keyboardVisible()) {
        search_.renderKeyboard(canvas, theme, body);
        keyboardWasVisible_ = true;
        return;
    }

    const bool keyboardClosed = keyboardWasVisible_;
    if ((keyboardClosed || needsPaint_) && context_.background)
        canvas.restoreFrom(*context_.background, body);
    keyboardWasVisible_ = false;
    renderBody(canvas, body, fullRedraw || keyboardClosed);
}

void GamesScreen::renderBody(Canvas &canvas, const Rect &body, bool fullRedraw) {
    Theme &theme = context_.theme;
    if (entries_.empty()) {
        if (searchResultsMode_) {
            const int maxWidth = std::max(theme.px(80), body.w - theme.px(100));
            std::vector<std::string> lines;
            std::string line;
            std::istringstream words(searchEmptyMessage_);
            for (std::string word; words >> word;) {
                const std::string candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && theme.regular().measure(candidate, theme.sizeHeading()) > maxWidth) {
                    lines.push_back(line);
                    line = word;
                } else {
                    line = candidate;
                }
            }
            if (!line.empty()) lines.push_back(line);
            const int lineHeight = theme.regular().lineHeight(theme.sizeHeading());
            const int blockHeight = lineHeight * int(lines.size());
            int y = body.y + (body.h - blockHeight) / 2;
            for (const std::string &part : lines) {
                const Rect lineArea{body.x, y, body.w, lineHeight};
                theme.regular().drawCentered(canvas, lineArea, part, theme.sizeHeading(),
                                             theme.textMuted);
                y += lineHeight;
            }
            return;
        }
        const std::string text = favoritesMode_
                                     ? formatButtonLabels("No favorites yet. Hold X for 2 seconds "
                                                          "on a game to add it here.")
                                 : allMode_
                                     ? "No games are indexed yet. Open Settings → Library and "
                                       "choose Build game list."
                                     : "No games were found for this system.";
        theme.regular().drawCentered(canvas, body, text, theme.sizeHeading(), theme.textMuted);
        return;
    }

    if (view_ == GameView::List) renderList(canvas, body, fullRedraw);
    else renderGrid(canvas, body, fullRedraw);
}

std::string GamesScreen::hints() const {
    return search_.hints("A Play game   Hold X 2s to favorite   Y Change game view   L2/R2 Jump to letter   {DPAD} Move");
}
