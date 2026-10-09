#include "GamesScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Alphabet.h"
#include "Canvas.h"
#include "DebugLog.h"
#include "Input.h"   // nowMs
#include "Tile.h"

namespace {

// How long a single frame may spend resolving artwork for newly-visible entries. Scrolling
// normally only reveals a handful of new tiles a frame and finishes well inside this; it
// exists for the rare jump that reveals a whole screenful at once — a letter jump across a
// library that has never scrolled there before — so that does not become a stall either.
constexpr int kResolveBudgetMs = 8;

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

ArtworkVariant artworkVariantFor(GameView view) {
    switch (view) {
    case GameView::List: return ArtworkVariant::Detail;
    case GameView::BoxartSmall: return ArtworkVariant::Small;
    case GameView::Grid:
    default: return ArtworkVariant::Grid;
    }
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
    title_ = title.empty() ? system.name : title;
    reload();
}

void GamesScreen::showFavorites() {
    system_ = nullptr;
    favoritesMode_ = true;
    allMode_ = false;
    title_ = "Favorites";
    reload();
}

void GamesScreen::showAllGames() {
    system_ = nullptr;
    favoritesMode_ = false;
    allMode_ = true;
    title_ = "All games";
    reload();
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
        for (const FavoriteEntry &favorite : context_.favorites.entries()) {
            const GameSystem *system = context_.library.findSystem(favorite.system);
            if (!system || Library::isSystemFile(*system, favorite.path)) continue;
            entries_.push_back({system, Library::makeStub(*system, favorite.path), false,
                                ArtworkVariant::Full});
        }
    } else if (allMode_) {
        for (const GameSystem &system : context_.library.systems())
            for (const std::string &path : context_.library.pathsOf(system))
                entries_.push_back({&system, Library::makeStub(system, path), false,
                                    ArtworkVariant::Full});
    } else if (system_) {
        for (const std::string &path : context_.library.pathsOf(*system_))
            entries_.push_back({system_, Library::makeStub(*system_, path), false,
                                ArtworkVariant::Full});
        preSorted = context_.library.pathsPreSorted(*system_);
    }

    // Merging several systems' worth of paths (or a source that was never sorted by display
    // name to begin with) means whatever order they arrived in is not the display order.
    if (!preSorted) {
        std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) {
            return strcasecmp(a.game.name.c_str(), b.game.name.c_str()) < 0;
        });
    }

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
    context_.notify(letter == '#' ? std::string("0–9") : std::string(1, letter), 1.2f);
}

void GamesScreen::toggleFavorite() {
    const Entry *entry = current();
    if (!entry) return;

    const bool wasFavorite = context_.favorites.contains(entry->game.path);
    context_.favorites.toggle(entry->system->name, entry->game.path, entry->game.name);
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

    if (context_.launcher.launchGame(*entry->system, entry->game)) {
        context_.history.remember(entry->system->name, entry->game.path, entry->game.name);
        context_.notify("Starting " + entry->game.name);
        context_.standDown = true;
    }
    else {
        context_.showError("Could not start " + entry->game.name, context_.launcher.lastError());
    }
}

void GamesScreen::handle(Action action) {
    switch (action) {
    case Action::Up:    moveCursor(0, -1); break;
    case Action::Down:  moveCursor(0, 1); break;
    case Action::Left:  moveCursor(-1, 0); break;
    case Action::Right: moveCursor(1, 0); break;
    case Action::Confirm: launch(); break;
    case Action::ToggleFavorite: toggleFavorite(); break;
    case Action::JumpPrev: jumpLetter(-1); break;
    case Action::JumpNext: jumpLetter(1); break;
    case Action::CycleView:
        setView(static_cast<GameView>((int(view_) + 1) % int(GameView::kCount)));
        context_.notify(std::string("View: ") + specFor(view_).name, 2.0f);
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
        ensureArtwork(entries_[size_t(index)], artworkVariantFor(view_));
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

void GamesScreen::renderDetail(Canvas &canvas, const Rect &area, const Entry &entry) {
    Theme &theme = context_.theme;

    // Background artwork, heavily dimmed so the foreground stays readable.
    if (context_.preferences.showBoxArt()) {
        if (ImagePtr background = context_.images.get(entry.game.background,
                                                       entry.game.backgroundFallback,
                                                       area.w, area.h)) {
            // Cover the panel instead of fitting into it: scale up until both sides are filled
            // and let the clip crop the overhang, so no bars appear beside the artwork.
            const long scaleW = long(area.w) * background->height();
            const long scaleH = long(area.h) * background->width();
            const int coverW = (scaleW > scaleH) ? area.w
                                                 : int(long(background->width()) * area.h /
                                                       background->height());
            const int coverH = (scaleW > scaleH) ? int(long(background->height()) * area.w /
                                                      background->width())
                                                 : area.h;
            const Rect cover{area.x + (area.w - coverW) / 2, area.y + (area.h - coverH) / 2,
                             coverW, coverH};

            canvas.pushClip(area);
            canvas.drawImage(*background, cover, uint8_t(90 * detailFade_), 0);
            canvas.popClip();
        }
    }
    canvas.fillRoundedRect(area, theme.radius(), theme.backgroundLo.withAlpha(150));

    const int pad = theme.px(28);
    const int textBlock = theme.px(110);
    const Rect artArea{area.x + pad, area.y + pad, area.w - 2 * pad,
                       area.h - 2 * pad - textBlock};

    ImagePtr art;
    if (context_.preferences.showBoxArt())
        art = context_.images.requestAsync(entry.game.boxart, entry.game.boxartFallback,
                                  artArea.w, artArea.h, true);
    if (art) {
        const Rect target = artArea.fitAspect(art->width(), art->height());
        canvas.dropShadow(target, theme.px(6), theme.px(18), theme.shadow.withAlpha(170));
        canvas.drawImage(*art, target, 255, theme.px(6));
    } else {
        canvas.fillRoundedRect(artArea, theme.radius(), theme.surface.withAlpha(120));
        const char *placeholder =
            context_.preferences.showBoxArt() ? "No boxart" : "Box art off";
        theme.regular().drawCentered(canvas, artArea, placeholder, theme.sizeBody(),
                                     theme.textMuted.withAlpha(140));
    }

    const Rect titleArea{area.x + pad, artArea.bottom() + theme.px(14), area.w - 2 * pad,
                         theme.px(44)};
    const int titleSize = theme.sizeGameDetail();
    const std::string name = theme.bold().elide(entry.game.name, titleSize, titleArea.w);
    theme.bold().draw(canvas, titleArea.x, titleArea.y, name, titleSize,
                      theme.textPrimary);

    std::string meta = entry.system->name;
    if (entry.game.isArchive()) meta += "  ·  Archive";
    if (context_.favorites.contains(entry.game.path)) meta += "  ·  Favorite";
    theme.regular().draw(canvas, titleArea.x, titleArea.bottom() + theme.px(2), meta,
                         theme.sizeBody(), theme.textMuted);
}

void GamesScreen::renderList(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;

    const int detailWidth = area.w * 42 / 100;
    const Rect listArea{area.x, area.y, area.w - detailWidth - theme.gap(), area.h};
    const Rect detailArea{listArea.right() + theme.gap(), area.y, detailWidth, area.h};

    const int rowHeight = theme.px(52);
    const int visible = std::max(1, listArea.h / rowHeight);

    if (cursor_ < scrollRow_) scrollRow_ = cursor_;
    if (cursor_ >= scrollRow_ + visible) scrollRow_ = cursor_ - visible + 1;
    const int maxScroll = std::max(0, int(entries_.size()) - visible);
    scrollRow_ = std::min(std::max(0, scrollRow_), maxScroll);

    canvas.pushClip(listArea);
    for (int i = scrollRow_; i < std::min(int(entries_.size()), scrollRow_ + visible); ++i) {
        const Entry &entry = entries_[size_t(i)];
        const Rect row{listArea.x, listArea.y + (i - scrollRow_) * rowHeight, listArea.w,
                       rowHeight - theme.px(6)};
        const float focus = focus_[size_t(i)];

        if (focus > 0.01f) {
            canvas.fillRoundedRect(row, theme.px(8),
                                   Color::lerp(theme.surface, theme.surfaceHi, focus)
                                       .withAlpha(uint8_t(220 * focus)));
            canvas.fillRoundedRect({row.x, row.y, theme.px(4), row.h}, theme.px(2),
                                   theme.accent.withAlpha(uint8_t(255 * focus)));
        }

        const bool isFavorite = context_.favorites.contains(entry.game.path);
        const int textX = row.x + theme.px(20);
        const int textW = row.w - theme.px(40) - (isFavorite ? theme.px(28) : 0);

        const int nameSize = theme.sizeGameLabel();
        const std::string name = theme.regular().elide(entry.game.name, nameSize, textW);
        theme.regular().draw(canvas, textX, row.y + (row.h - theme.regular().lineHeight(nameSize)) / 2,
                             name, nameSize,
                             Color::lerp(theme.textMuted, theme.textPrimary, focus));

        if (isFavorite) {
            theme.bold().draw(canvas, row.right() - theme.px(26),
                              row.y + (row.h - theme.bold().lineHeight(theme.sizeBody())) / 2,
                              "*", theme.sizeBody(), theme.favorite);
        }
    }
    canvas.popClip();

    if (const Entry *entry = current()) renderDetail(canvas, detailArea, *entry);
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

    // Resolve artwork paths for whatever is newly visible, within a small per-frame budget.
    // Once resolved an entry stays that way (for this view's size) until it changes, so a
    // normal scroll only ever touches a handful of never-seen entries; the budget exists for
    // the rare jump that reveals a whole screenful never visited before — a letter jump deep
    // into a library that was never scrolled there.
    const ArtworkVariant variant = artworkVariantFor(view_);
    const int64_t resolveDeadline = nowMs() + kResolveBudgetMs;
    if (context_.preferences.showBoxArt()) {
        for (int i = first; i < last && nowMs() < resolveDeadline; ++i)
            ensureArtwork(entries_[size_t(i)], variant);
    }

    // Ask for the focused tile's picture before anything else. The two passes below draw it
    // last on purpose — so its shadow and grown edge sit above its neighbours rather than
    // under them — but that made its own request the last one issued too, and the per-frame
    // decode budget is normally spent by the first cache miss it meets. On a screen full of
    // misses the focused tile lost every single time; asking here first means it never has to
    // wait on whichever neighbour happened to be drawn before it.
    if (context_.preferences.showBoxArt() && cursor_ >= first && cursor_ < last) {
        const Entry &focused = entries_[size_t(cursor_)];
        context_.images.requestAsync(focused.game.boxart, focused.game.boxartFallback,
                            grid_.tileWidth(), grid_.tileHeight(), true);
    }

    // Keep the current image per visible tile. A cache-wide generation counter makes one
    // arriving cover repaint every tile; comparing these pointers limits that repaint to the
    // cover that actually changed.
    std::vector<ImagePtr> visibleArtwork(size_t(last - first));
    if (context_.preferences.showBoxArt()) {
        for (int i = first; i < last; ++i) {
            const Entry &entry = entries_[size_t(i)];
            visibleArtwork[size_t(i - first)] = context_.images.requestAsync(
                entry.game.boxart, entry.game.boxartFallback, grid_.tileWidth(),
                grid_.tileHeight(), i == cursor_);
        }
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
            if (!focusChanged && !artworkChanged) continue;
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

            const Entry &entry = entries_[size_t(i)];
            const Rect frame = grid_.cellFrame(i, scrollRow_);
            if (!layoutChanged && Tile::footprint(theme, frame).intersect(repaint).empty())
                continue;

            Tile::Content content;
            content.label = entry.game.name;
            content.showLabel = spec.showLabel;
            content.coverArt = spec.coverArt;
            content.favorite = context_.favorites.contains(entry.game.path);
            content.image = visibleArtwork[size_t(i - first)];

            Tile::draw(canvas, theme, frame, content, focus_[size_t(i)]);
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
    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    // Incremental grid mode leaves the static screen area in place, so restore its header
    // before drawing text with antialiased edges again.
    if (incremental() && context_.background) canvas.restoreFrom(*context_.background, header);
    renderHeader(canvas, header);

    if (entries_.empty()) {
        const char *text = favoritesMode_
                               ? "No favorites yet - hold X for 2s on a game to mark it"
                           : allMode_
                               ? "No games yet - build the game database in the settings"
                               : "No games in this directory";
        theme.regular().drawCentered(canvas, body, text, theme.sizeHeading(), theme.textMuted);
        return;
    }

    if (view_ == GameView::List) renderList(canvas, body);
    else renderGrid(canvas, body, fullRedraw);
}

std::string GamesScreen::hints() const {
    return "A Start   B Back   Hold X 2s Favorite   Y View   L2/R2 Letter";
}
