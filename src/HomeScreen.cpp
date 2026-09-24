#include "HomeScreen.h"

#include <cmath>
#include <cstdio>

#include "Alphabet.h"
#include "Canvas.h"
#include "Tile.h"

namespace {

// Design pixels; everything else follows from the tile width.
constexpr int kTileWidth = 190;
// Square: see the note in GamesScreen. Box art comes both ways round, and a square tile is
// the shape that suits both equally.
constexpr int kTileAspectW = 1;
constexpr int kTileAspectH = 1;

} // namespace

HomeScreen::HomeScreen(Context &context) : context_(context) { refresh(); }

void HomeScreen::refresh() {
    rows_.clear();

    Row recent;
    recent.title = "Recently played";
    recent.emptyHint = "Nothing started yet";
    for (const HistoryEntry &entry : context_.history.entries()) {
        const GameSystem *system = entry.system.empty()
                                       ? context_.library.systemForPath(entry.path)
                                       : context_.library.findSystem(entry.system);
        if (!system) continue;
        recent.items.push_back({system, Library::makeGame(*system, entry.path)});
    }

    Row favorites;
    favorites.title = "Favorites";
    favorites.emptyHint = "Press X on a game to mark it";
    for (const FavoriteEntry &entry : context_.favorites.entries()) {
        const GameSystem *system = context_.library.findSystem(entry.system);
        if (!system) continue;
        favorites.items.push_back({system, Library::makeGame(*system, entry.path)});
    }

    for (Row *row : {&recent, &favorites}) {
        row->focus.assign(row->items.size(), 0.0f);
        rows_.push_back(std::move(*row));
    }

    activeRow_ = 0;
    pageScroll_ = 0;
    paintedPageScroll_ = -1;
    paintedImages_ = uint64_t(-1);

    std::printf("home: %zu recent, %zu favorites\n", rows_[0].items.size(),
                rows_[1].items.size());
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

void HomeScreen::launch() {
    const Item *item = current();
    if (!item) return;

    if (context_.launcher.launchGame(*item->system, item->game)) {
        context_.history.remember(item->system->name, item->game.path, item->game.name);
        context_.notify("Starting " + item->game.name);
        context_.standDown = true;
    } else {
        context_.showError("Could not start " + item->game.name, context_.launcher.lastError());
    }
}

void HomeScreen::toggleFavorite() {
    const Item *item = current();
    if (!item) return;

    const bool was = context_.favorites.contains(item->game.path);
    context_.favorites.toggle(item->system->name, item->game.path, item->game.name);
    context_.notify(was ? item->game.name + " removed from favorites"
                        : item->game.name + " added to favorites");

    // The favourites row is one of the rows on screen, so rebuild it in place.
    Row &favorites = rows_[1];
    const int previous = favorites.cursor;
    favorites.items.clear();
    for (const FavoriteEntry &entry : context_.favorites.entries()) {
        const GameSystem *system = context_.library.findSystem(entry.system);
        if (!system) continue;
        favorites.items.push_back({system, Library::makeGame(*system, entry.path)});
    }
    favorites.focus.assign(favorites.items.size(), 0.0f);
    favorites.cursor = std::min(previous, std::max(0, int(favorites.items.size()) - 1));
}

void HomeScreen::handle(Action action) {
    if (rows_.empty()) return;
    Row &row = rows_[size_t(activeRow_)];

    switch (action) {
    case Action::Left:
        if (row.cursor > 0) --row.cursor;
        break;
    case Action::Right:
        if (row.cursor + 1 < int(row.items.size())) ++row.cursor;
        break;
    case Action::Up:
        if (activeRow_ > 0) --activeRow_;
        break;
    case Action::Down:
        if (activeRow_ + 1 < int(rows_.size())) ++activeRow_;
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
    context_.notify(letter == '#' ? std::string("0–9") : std::string(1, letter), 1.2f);
}

void HomeScreen::update(float deltaSeconds) {
    const float speed = std::min(1.0f, deltaSeconds * 9.0f);

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
        theme.regular().draw(canvas, strip.x, strip.y + theme.px(10), row.emptyHint,
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
    if (active && row.cursor >= row.scroll && row.cursor < last) {
        const Item &focused = row.items[size_t(row.cursor)];
        context_.images.get(focused.game.boxart, focused.game.boxartFallback, tileWidth,
                            tileHeight);
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = row.scroll; i < last; ++i) {
            const bool isCursor = active && i == row.cursor;
            if ((pass == 0) == isCursor) continue;

            const Item &item = row.items[size_t(i)];
            const Rect frame{lane.x + (i - row.scroll) * (tileWidth + gap), lane.y, tileWidth,
                             tileHeight + labelHeight};

            Tile::Content content;
            content.label = item.game.name;
            content.sublabel = item.system->name;
            content.coverArt = true;
            content.favorite = context_.favorites.contains(item.game.path);
            content.image = context_.images.get(item.game.boxart, item.game.boxartFallback,
                                                tileWidth, tileHeight);

            Tile::draw(canvas, theme, frame, content, row.focus[size_t(i)]);
        }
    }

    canvas.popClip();
}

void HomeScreen::requestImages(Row &row, int laneWidth) {
    if (row.items.empty()) return;

    const Metrics m = metrics();
    const int gap = context_.theme.gap();
    const int lane = laneWidth - 2 * m.growX;
    const int perPage = std::max(1, (lane + gap) / (m.tileWidth + gap));

    if (row.cursor < row.scroll) row.scroll = row.cursor;
    if (row.cursor >= row.scroll + perPage) row.scroll = row.cursor - perPage + 1;
    row.scroll = std::min(std::max(0, row.scroll),
                          std::max(0, int(row.items.size()) - perPage));

    const int last = std::min(int(row.items.size()), row.scroll + perPage + 1);
    for (int i = row.scroll; i < last; ++i) {
        const Item &item = row.items[size_t(i)];
        context_.images.get(item.game.boxart, item.game.boxartFallback, m.tileWidth,
                            m.tileHeight);
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
    const Rect header{area.x, area.y, area.w, headerHeight};
    if (context_.background) canvas.restoreFrom(*context_.background, header);
    theme.bold().draw(canvas, area.x, area.y, "Home", theme.sizeHeading(), theme.textPrimary);

    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    rowHeight_ = metrics().rowHeight;

    // Keep the active row fully visible; the page slides by whole rows.
    const int visibleRows = std::max(1, body.h / rowHeight_);
    if (activeRow_ < pageScroll_) pageScroll_ = activeRow_;
    if (activeRow_ >= pageScroll_ + visibleRows) pageScroll_ = activeRow_ - visibleRows + 1;
    pageScroll_ = std::min(std::max(0, pageScroll_),
                           std::max(0, int(rows_.size()) - visibleRows));

    // Asking here, before anything decides whether to repaint, is what keeps artwork
    // loading on a screen that has otherwise gone still: the request (and any decode it
    // triggers) used to live inside renderRow(), which only ran when something had
    // already changed — so once the focus animation settled, nothing ever asked again,
    // and any picture that missed the decode budget by then stayed missing forever.
    for (Row &row : rows_) requestImages(row, body.w);

    // Taken after that request, so a decode it just triggered still counts as a change
    // this frame rather than waiting until the next one.
    const uint64_t imagesBefore = context_.images.generation();

    // The caller may have already wiped the canvas this frame for a reason of its own — a
    // tab switch, returning from a detail view — in which case skipping our own repaint
    // because nothing *we* track has changed would leave that wipe on screen.
    bool anyChange = fullRedraw || pageScroll_ != paintedPageScroll_ ||
                     imagesBefore != paintedImages_;
    for (size_t r = 0; r < rows_.size() && !anyChange; ++r)
        anyChange = rowChanged(rows_[r], int(r) == activeRow_);

    if (!anyChange) return;

    // A focused tile grows and casts a shadow beyond its own row, so restoring one strip
    // would leave debris in the neighbour. With three rows, repainting the content area is
    // simpler and provably correct.
    if (context_.background) canvas.restoreFrom(*context_.background, body);

    canvas.pushClip(body);

    int y = body.y - pageScroll_ * rowHeight_;
    for (size_t r = 0; r < rows_.size(); ++r) {
        // An empty row shrinks to its heading, so it does not waste a screenful.
        const int height = rows_[r].items.empty() ? theme.px(66) : rowHeight_;
        const Rect rowArea{body.x, y, body.w, height - theme.gap()};
        y += height;

        if (rowArea.bottom() >= body.y && rowArea.y <= body.bottom())
            renderRow(canvas, rowArea, rows_[r], int(r) == activeRow_);

        Row &row = rows_[r];
        row.paintedFocus = row.focus;
        row.paintedScroll = row.scroll;
        row.paintedCount = row.items.size();
        row.paintedActive = int(r) == activeRow_;
    }

    canvas.popClip();
    paintedPageScroll_ = pageScroll_;
    paintedImages_ = imagesBefore;
}

std::string HomeScreen::hints() const {
    return "A Start   X Favorite   L2/R2 Letter   D-pad Navigate";
}
