#include "GamesScreen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Alphabet.h"
#include "Canvas.h"
#include "Input.h"   // nowMs
#include "LoadingIndicator.h"
#include "Tile.h"

namespace {

// How long buildStep() is allowed to spend resolving artwork paths each frame. Long enough
// that a big system finishes in a handful of seconds rather than tens of them; short enough
// that the screen stays responsive to input and the loading count keeps visibly moving
// instead of the frame stalling on it.
constexpr int kBuildBudgetMs = 12;

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
    case GameView::BoxartLarge: return {380, 1, 1, true, false, "Boxart large"};
    case GameView::Grid:        return {250, 1, 1, true, true, "Grid"};
    case GameView::BoxartSmall: return {150, 1, 1, true, false, "Boxart small"};
    case GameView::Compact:     return {104, 1, 1, true, false, "Compact"};
    case GameView::List:
    default:                    return {0, 0, 0, false, false, "List"};
    }
}

} // namespace

GamesScreen::GamesScreen(Context &context) : context_(context) {}

void GamesScreen::setView(GameView view) {
    view_ = view;
    scrollRow_ = 0;
}

void GamesScreen::showSystem(const GameSystem &system) {
    system_ = &system;
    favoritesMode_ = false;
    allMode_ = false;
    title_ = system.name;
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
    pending_.clear();
    pendingIndex_ = 0;
    loading_ = false;

    // A handful of entries at most, and resolving each one straight away is what lets the
    // detail panel show something the instant favourites opens — nothing here is worth
    // spreading across frames.
    if (favoritesMode_) {
        for (const FavoriteEntry &favorite : context_.favorites.entries()) {
            const GameSystem *system = context_.library.findSystem(favorite.system);
            if (!system) continue;
            entries_.push_back({system, Library::makeGame(*system, favorite.path)});
        }
        focus_.assign(entries_.size(), 0.0f);
        std::printf("games: %zu entries for %s\n", entries_.size(), title_.c_str());
        return;
    }

    // Only the cheap part happens here — the paths, not the artwork lookups that make each
    // one expensive. buildStep() works through this list a slice at a time from update().
    bool preSorted = false;
    if (allMode_) {
        for (const GameSystem &system : context_.library.systems())
            for (const std::string &path : context_.library.pathsOf(system))
                pending_.push_back({&system, path, std::string()});
    } else if (system_) {
        for (const std::string &path : context_.library.pathsOf(*system_))
            pending_.push_back({system_, path, std::string()});
        preSorted = context_.library.pathsPreSorted(*system_);
    }

    // Merging several systems' worth of paths (or a source that was never sorted by display
    // name to begin with) means whatever order they arrived in is not the display order. The
    // name a path will get costs no I/O — see Library::nameFor — so the whole list can be put
    // in its final order right here, before any of the expensive work starts, and every entry
    // can be shown the moment it resolves instead of waiting for the slowest one to arrive so
    // everything can be sorted and revealed together.
    if (!preSorted) {
        for (PendingItem &item : pending_) item.sortKey = Library::nameFor(*item.system, item.path);
        std::sort(pending_.begin(), pending_.end(), [](const PendingItem &a, const PendingItem &b) {
            return strcasecmp(a.sortKey.c_str(), b.sortKey.c_str()) < 0;
        });
    }

    loading_ = !pending_.empty();
    if (!loading_) std::printf("games: 0 entries for %s\n", title_.c_str());
}

void GamesScreen::buildStep() {
    if (!loading_) return;

    const int64_t deadline = nowMs() + kBuildBudgetMs;
    while (pendingIndex_ < pending_.size() && nowMs() < deadline) {
        const PendingItem &item = pending_[pendingIndex_++];
        entries_.push_back({item.system, Library::makeGame(*item.system, item.path)});
        focus_.push_back(0.0f);
    }

    if (pendingIndex_ < pending_.size()) return;

    pending_.clear();
    loading_ = false;
    std::printf("games: %zu entries for %s\n", entries_.size(), title_.c_str());
}

const GamesScreen::Entry *GamesScreen::current() const {
    if (entries_.empty()) return nullptr;
    const int index = std::min(std::max(cursor_, 0), int(entries_.size()) - 1);
    return &entries_[size_t(index)];
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
        context_.notify("Could not start: " + context_.launcher.lastError(), 6.0f);
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
    buildStep();

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
    // how far along the alphabet the cursor sits. While still loading, pending_ already
    // knows the final count — showing that instead of the still-growing entries_.size()
    // means the total does not visibly climb as more of it resolves.
    const int total = loading_ ? int(pending_.size()) : int(entries_.size());
    char info[96];
    std::snprintf(info, sizeof(info), "%d / %d  ·  %s", entries_.empty() ? 0 : cursor_ + 1,
                  total, specFor(view_).name);
    const int width = theme.regular().measure(info, theme.sizeBody());
    theme.regular().draw(canvas, area.right() - width, area.y + theme.px(10), info,
                         theme.sizeBody(), theme.textMuted.withAlpha(160));
}

void GamesScreen::renderDetail(Canvas &canvas, const Rect &area, const Entry &entry) {
    Theme &theme = context_.theme;

    // Background artwork, heavily dimmed so the foreground stays readable.
    if (ImagePtr background = context_.images.get(entry.game.background, entry.game.backgroundFallback,
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
        const Rect cover{area.x + (area.w - coverW) / 2, area.y + (area.h - coverH) / 2, coverW,
                         coverH};

        canvas.pushClip(area);
        canvas.drawImage(*background, cover, uint8_t(90 * detailFade_), 0);
        canvas.popClip();
    }
    canvas.fillRoundedRect(area, theme.radius(), theme.backgroundLo.withAlpha(150));

    const int pad = theme.px(28);
    const int textBlock = theme.px(110);
    const Rect artArea{area.x + pad, area.y + pad, area.w - 2 * pad,
                       area.h - 2 * pad - textBlock};

    if (ImagePtr art = context_.images.get(entry.game.boxart, entry.game.boxartFallback,
                                             artArea.w, artArea.h)) {
        const Rect target = artArea.fitAspect(art->width(), art->height());
        canvas.dropShadow(target, theme.px(6), theme.px(18), theme.shadow.withAlpha(170));
        canvas.drawImage(*art, target, 255, theme.px(6));
    } else {
        canvas.fillRoundedRect(artArea, theme.radius(), theme.surface.withAlpha(120));
        theme.regular().drawCentered(canvas, artArea, "No boxart", theme.sizeBody(),
                                     theme.textMuted.withAlpha(140));
    }

    const Rect titleArea{area.x + pad, artArea.bottom() + theme.px(14), area.w - 2 * pad,
                         theme.px(44)};
    const std::string name =
        theme.bold().elide(entry.game.name, theme.sizeTitle(), titleArea.w);
    theme.bold().draw(canvas, titleArea.x, titleArea.y, name, theme.sizeTitle(),
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

        const std::string name = theme.regular().elide(entry.game.name, theme.sizeBody(), textW);
        theme.regular().draw(canvas, textX, row.y + (row.h - theme.regular().lineHeight(theme.sizeBody())) / 2,
                             name, theme.sizeBody(),
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

void GamesScreen::renderGrid(Canvas &canvas, const Rect &area) {
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

    // Ask for the focused tile's picture before anything else. The two passes below draw it
    // last on purpose — so its shadow and grown edge sit above its neighbours rather than
    // under them — but that made its own request the last one issued too, and the per-frame
    // decode budget is normally spent by the first cache miss it meets. On a screen full of
    // misses the focused tile lost every single time; asking here first means it never has to
    // wait on whichever neighbour happened to be drawn before it.
    if (cursor_ >= first && cursor_ < last) {
        const Entry &focused = entries_[size_t(cursor_)];
        context_.images.get(focused.game.boxart, focused.game.boxartFallback,
                            grid_.tileWidth(), grid_.tileHeight());
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = first; i < last; ++i) {
            const bool isCursor = (i == cursor_);
            if ((pass == 0) == isCursor) continue;

            const Entry &entry = entries_[size_t(i)];
            const Rect frame = grid_.cellFrame(i, scrollRow_);

            Tile::Content content;
            content.label = entry.game.name;
            content.showLabel = spec.showLabel;
            content.coverArt = spec.coverArt;
            content.favorite = context_.favorites.contains(entry.game.path);
            content.image = context_.images.get(entry.game.boxart, entry.game.boxartFallback,
                                                grid_.tileWidth(), grid_.tileHeight());

            Tile::draw(canvas, theme, frame, content, focus_[size_t(i)]);
        }
    }
    canvas.popClip();
}

void GamesScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    const Rect header{area.x, area.y, area.w, headerHeight};
    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    renderHeader(canvas, header);

    if (entries_.empty() && loading_) {
        // Only reached while merging several systems (or another source that arrives
        // unsorted) — a single database-backed system reveals entries as they resolve and
        // never has an empty, still-loading grid to report on.
        const int panelWidth = std::min(body.w, theme.px(700));
        const Rect panel{body.x + (body.w - panelWidth) / 2, body.y + body.h / 2 - theme.px(70),
                         panelWidth, theme.px(200)};
        const float fraction =
            pending_.empty() ? 0.0f : float(pendingIndex_) / float(pending_.size());
        char counts[64];
        std::snprintf(counts, sizeof(counts), "%zu of %zu games", pendingIndex_, pending_.size());
        LoadingIndicator::draw(canvas, theme, panel, title_, fraction, std::string(), counts);
        return;
    }

    if (entries_.empty()) {
        const char *text = favoritesMode_
                               ? "No favorites yet - press X on a game to mark it"
                           : allMode_
                               ? "No games yet - build the game database in the settings"
                               : "No games in this directory";
        theme.regular().drawCentered(canvas, body, text, theme.sizeHeading(), theme.textMuted);
        return;
    }

    if (view_ == GameView::List) renderList(canvas, body);
    else renderGrid(canvas, body);
}

std::string GamesScreen::hints() const {
    return "A Start   B Back   X Favorite   Y View   L2/R2 Letter";
}
