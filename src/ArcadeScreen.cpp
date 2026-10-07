#include "ArcadeScreen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Alphabet.h"
#include "Canvas.h"
#include "Tile.h"

namespace {

// Design pixels. Smaller than Home's 190 so that all three rows fit on a 1080p screen at once,
// which is the point of a tab that is three previews of the same library.
constexpr int kTileWidth = 160;

std::string countText(size_t count) {
    return std::to_string(count) + (count == 1 ? " game" : " games");
}

} // namespace

ArcadeScreen::ArcadeScreen(Context &context, OpenFullHandler onOpenFull,
                           OpenGroupHandler onOpenGroup)
    : context_(context), onOpenFull_(std::move(onOpenFull)),
      onOpenGroup_(std::move(onOpenGroup)) {}

void ArcadeScreen::refresh() {
    rows_.clear();
    activeRow_ = 0;
    pageScroll_ = 0;

    system_ = context_.library.arcadeSystem();
    if (!system_) return;

    Row games;
    games.dimension = Dimension::Games;
    games.title = "Games";
    for (const std::string &path : context_.library.pathsOf(*system_)) {
        Item item;
        item.game = Library::makeStub(*system_, path);
        games.items.push_back(std::move(item));
    }

    const GameDatabase &database = context_.library.database();
    const auto groupRow = [&](Dimension dimension, const char *title, const char *file) {
        Row row;
        row.dimension = dimension;
        row.title = title;
        for (const DatabaseGroup &group : database.groupsFor(file)) {
            Item item;
            item.group = group;
            row.items.push_back(std::move(item));
        }
        // The biggest groups first, because that is what a preview row is for. The full view
        // is alphabetical instead, where a letter jump has something to work with.
        std::stable_sort(row.items.begin(), row.items.end(), [](const Item &a, const Item &b) {
            if (a.group.count != b.group.count) return a.group.count > b.group.count;
            return strcasecmp(a.group.name.c_str(), b.group.name.c_str()) < 0;
        });
        return row;
    };

    rows_.push_back(std::move(games));
    rows_.push_back(groupRow(Dimension::Manufacturers, "Manufacturers",
                             GameDatabase::kArcadeManufacturers));
    rows_.push_back(groupRow(Dimension::Categories, "Categories",
                             GameDatabase::kArcadeCategories));

    for (Row &row : rows_) row.focus.assign(row.items.size(), 0.0f);
}

bool ArcadeScreen::wantsBack() const {
    return !rows_.empty() && active().cursor >= 0;
}

std::string ArcadeScreen::favoritePath() const {
    if (rows_.empty()) return {};
    const Row &row = active();
    if (!system_ || row.dimension != Dimension::Games || row.cursor < 0 ||
        row.cursor >= int(row.items.size())) return {};
    return row.items[size_t(row.cursor)].game.path;
}

ArcadeScreen::Metrics ArcadeScreen::metrics() const {
    Theme &theme = context_.theme;

    Metrics m;
    m.tileWidth = theme.px(kTileWidth);
    m.tileHeight = m.tileWidth;
    m.captionGame = Tile::captionHeight(theme, true, false);
    m.captionGroup = Tile::captionHeight(theme, false, true);

    const Rect probe{0, 0, m.tileWidth, m.tileHeight + std::max(m.captionGame, m.captionGroup)};
    m.growX = Tile::focusMarginX(probe);
    m.growY = Tile::focusMarginY(probe);

    m.rowHeight = theme.px(40) + m.tileHeight + std::max(m.captionGame, m.captionGroup) +
                  2 * m.growY + theme.gap();
    return m;
}

void ArcadeScreen::launch() {
    Row &row = active();
    if (row.cursor < 0 || row.dimension != Dimension::Games || !system_) return;

    const Game &game = row.items[size_t(row.cursor)].game;
    if (context_.launcher.launchGame(*system_, game)) {
        context_.history.remember(system_->name, game.path, game.name);
        context_.notify("Starting " + game.name);
        context_.standDown = true;
    } else {
        context_.showError("Could not start " + game.name, context_.launcher.lastError());
    }
}

void ArcadeScreen::toggleFavorite() {
    Row &row = active();
    if (row.cursor < 0 || row.dimension != Dimension::Games || !system_) return;

    const Game &game = row.items[size_t(row.cursor)].game;
    const bool was = context_.favorites.contains(game.path);
    context_.favorites.toggle(system_->name, game.path, game.name);
    context_.notify(was ? game.name + " removed from favorites"
                        : game.name + " added to favorites");
}

void ArcadeScreen::jumpLetter(int direction) {
    Row &row = active();
    if (row.dimension != Dimension::Games || row.items.empty()) return;

    const int from = std::max(row.cursor, 0);
    const int target = Alphabet::jump(from, int(row.items.size()), direction,
                                      [&row](int i) -> const std::string & {
                                          return row.items[size_t(i)].game.name;
                                      });
    if (target == from && row.cursor >= 0) return;

    row.cursor = target;
    const char letter = Alphabet::initial(row.items[size_t(row.cursor)].game.name);
    context_.notify(letter == '#' ? std::string("0-9") : std::string(1, letter), 1.2f);
}

void ArcadeScreen::handle(Action action) {
    if (rows_.empty()) return;
    Row &row = active();

    switch (action) {
    case Action::Left:
        if (row.cursor >= 0) --row.cursor;
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
    case Action::Back:
        // From a tile, back to its row's title; leaving the tab is App's business, and it
        // only asks this screen when wantsBack() says there is somewhere nearer to go.
        if (row.cursor >= 0) row.cursor = -1;
        break;
    case Action::Confirm:
        if (row.cursor < 0) {
            onOpenFull_(row.dimension);
        } else if (row.dimension == Dimension::Games) {
            launch();
        } else {
            onOpenGroup_(row.dimension, row.items[size_t(row.cursor)].group);
        }
        break;
    case Action::ToggleFavorite:
        toggleFavorite();
        break;
    case Action::JumpPrev:
        jumpLetter(-1);
        break;
    case Action::JumpNext:
        jumpLetter(1);
        break;
    default:
        break;
    }
}

void ArcadeScreen::update(float deltaSeconds) {
    const float speed = std::min(1.0f, deltaSeconds * 9.0f);

    for (size_t r = 0; r < rows_.size(); ++r) {
        Row &row = rows_[r];
        const bool isActive = int(r) == activeRow_;

        const float titleTarget = (isActive && row.cursor < 0) ? 1.0f : 0.0f;
        row.titleFocus += (titleTarget - row.titleFocus) * speed;

        for (size_t i = 0; i < row.focus.size(); ++i) {
            const float target = (isActive && int(i) == row.cursor) ? 1.0f : 0.0f;
            row.focus[i] += (target - row.focus[i]) * speed;
        }
    }
}

int ArcadeScreen::perPage(const Row &, int laneWidth) const {
    const Metrics m = metrics();
    const int gap = context_.theme.gap();
    const int lane = laneWidth - 2 * m.growX;
    return std::max(1, (lane + gap) / (m.tileWidth + gap));
}

void ArcadeScreen::settleScroll(Row &row, int laneWidth) {
    const int page = perPage(row, laneWidth);
    const int cursor = std::max(row.cursor, 0);

    if (cursor < row.scroll) row.scroll = cursor;
    if (cursor >= row.scroll + page) row.scroll = cursor - page + 1;
    row.scroll = std::min(std::max(0, row.scroll), std::max(0, int(row.items.size()) - page));
}

void ArcadeScreen::renderRow(Canvas &canvas, const Rect &area, Row &row, bool isActive) {
    Theme &theme = context_.theme;

    const int titleHeight = theme.px(40);
    Font &bold = theme.bold();

    // The title is a stop of its own. Focused, it sits in a pill in the accent colour, which
    // is also what marks "A opens the whole list" — nothing else on screen looks like it.
    // Inside the row's own area rather than around it, so the clip the rows are drawn under
    // never cuts the pill's edge off.
    const int titleWidth = bold.measure(row.title, theme.sizeBody());
    const int padX = theme.px(14);
    const Rect pill{area.x, area.y, titleWidth + 2 * padX,
                    bold.lineHeight(theme.sizeBody()) + theme.px(8)};
    if (row.titleFocus > 0.01f) {
        canvas.fillRoundedRect(pill, pill.h / 2,
                               theme.accent.withAlpha(uint8_t(70 * row.titleFocus)));
        canvas.strokeRoundedRect(pill, pill.h / 2, theme.px(2),
                                 theme.accent.withAlpha(uint8_t(255 * row.titleFocus)));
    }
    bold.draw(canvas, area.x + padX, area.y + theme.px(4), row.title, theme.sizeBody(),
              Color::lerp(isActive ? theme.textPrimary : theme.textMuted.withAlpha(150),
                          theme.accent, row.titleFocus));

    char count[32];
    std::snprintf(count, sizeof(count), "%d", int(row.items.size()));
    const int countWidth = theme.regular().measure(count, theme.sizeSmall());
    theme.regular().draw(canvas, area.right() - countWidth, area.y + theme.px(8), count,
                         theme.sizeSmall(), theme.textMuted.withAlpha(120));

    const Rect strip{area.x, area.y + titleHeight, area.w, area.h - titleHeight};
    if (strip.empty() || row.items.empty()) return;

    const Metrics m = metrics();
    const int gap = theme.gap();
    const bool games = row.dimension == Dimension::Games;
    const int caption = games ? m.captionGame : m.captionGroup;

    const Rect lane{strip.x + m.growX, strip.y + m.growY, strip.w - 2 * m.growX,
                    strip.h - m.growY};
    const int page = perPage(row, strip.w);
    settleScroll(row, strip.w);

    canvas.pushClip(strip);
    const int last = std::min(int(row.items.size()), row.scroll + page + 1);

    // Artwork for what is about to be drawn, at most once per game; see GamesScreen, which
    // resolves lazily for the same reason.
    if (games && system_) {
        for (int i = row.scroll; i < last; ++i) {
            Item &item = row.items[size_t(i)];
            if (item.artworkResolved) continue;
            Library::resolveArtwork(*system_, item.game, true);
            item.artworkResolved = true;
        }
        if (isActive && row.cursor >= row.scroll && row.cursor < last) {
            const Game &focused = row.items[size_t(row.cursor)].game;
            context_.images.get(focused.boxart, focused.boxartFallback, m.tileWidth,
                                m.tileHeight);
        }
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = row.scroll; i < last; ++i) {
            const bool isCursor = isActive && i == row.cursor;
            if ((pass == 0) == isCursor) continue;

            const Item &item = row.items[size_t(i)];
            const Rect frame{lane.x + (i - row.scroll) * (m.tileWidth + gap), lane.y,
                             m.tileWidth, m.tileHeight + caption};

            if (games) {
                Tile::Content content;
                content.label = item.game.name;
                content.coverArt = true;
                content.favorite = context_.favorites.contains(item.game.path);
                content.image = context_.images.get(item.game.boxart, item.game.boxartFallback,
                                                    m.tileWidth, m.tileHeight);
                Tile::draw(canvas, theme, frame, content, row.focus[size_t(i)]);
            } else {
                // Manufacturers and categories are navigation entries, not games. Keep them
                // as plain text with a small count and the same blue underline used for tabs.
                const float focus = row.focus[size_t(i)];
                const Rect nameArea{frame.x + theme.px(6), frame.y + theme.px(48),
                                    frame.w - theme.px(12), theme.px(30)};
                const std::string name = theme.bold().elide(
                    item.group.name, theme.sizeBody(), nameArea.w);
                theme.bold().drawCentered(canvas, nameArea, name, theme.sizeBody(),
                    Color::lerp(theme.textMuted, theme.textPrimary, focus));

                const Rect countArea{frame.x, nameArea.bottom() + theme.px(4), frame.w,
                                     theme.px(20)};
                const std::string count = countText(item.group.count);
                theme.regular().drawCentered(canvas, countArea, count, theme.sizeSmall(),
                                             theme.textMuted.withAlpha(190));

                const int lineWidth = theme.px(48);
                const int lineHeight = std::max(1, theme.px(3));
                const Rect focusLine{frame.x + (frame.w - lineWidth) / 2,
                                     countArea.bottom() + theme.px(7), lineWidth, lineHeight};
                if (focus > 0.01f)
                    canvas.fillRect(focusLine,
                                    theme.accent.withAlpha(uint8_t(255 * focus)));
            }
        }
    }
    canvas.popClip();
}

void ArcadeScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    theme.bold().draw(canvas, area.x, area.y, "Arcade", theme.sizeHeading(), theme.textPrimary);

    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    if (rows_.empty() || rows_[0].items.empty()) {
        theme.regular().drawCentered(canvas, body,
                                     "No Arcade games - build the game database in the settings",
                                     theme.sizeHeading(), theme.textMuted);
        return;
    }

    char info[64];
    std::snprintf(info, sizeof(info), "%d games", int(rows_[0].items.size()));
    const int infoWidth = theme.regular().measure(info, theme.sizeBody());
    theme.regular().draw(canvas, area.right() - infoWidth, area.y + theme.px(10), info,
                         theme.sizeBody(), theme.textMuted.withAlpha(160));

    const int rowHeight = metrics().rowHeight;
    const int visibleRows = std::max(1, body.h / rowHeight);
    if (activeRow_ < pageScroll_) pageScroll_ = activeRow_;
    if (activeRow_ >= pageScroll_ + visibleRows) pageScroll_ = activeRow_ - visibleRows + 1;
    pageScroll_ = std::min(std::max(0, pageScroll_),
                           std::max(0, int(rows_.size()) - visibleRows));

    canvas.pushClip(body);
    int y = body.y - pageScroll_ * rowHeight;
    for (size_t r = 0; r < rows_.size(); ++r) {
        // A row with nothing in it shrinks to its heading, like Home's does.
        const int height = rows_[r].items.empty() ? theme.px(66) : rowHeight;
        const Rect rowArea{body.x, y, body.w, height - theme.gap()};
        y += height;

        if (rowArea.bottom() >= body.y && rowArea.y <= body.bottom())
            renderRow(canvas, rowArea, rows_[r], int(r) == activeRow_);
    }
    canvas.popClip();
}

std::string ArcadeScreen::hints() const {
    if (rows_.empty()) return "LB/RB Tabs";

    const Row &row = active();
    if (row.cursor < 0)
        return "A Open all " + row.title + "   Right Browse   Up/Down Row   LB/RB Tabs";

    if (row.dimension == Dimension::Games)
        return "A Start   Hold X 2s Favorite   B Back to title   L2/R2 Letter";

    return "A Open " + row.items[size_t(row.cursor)].group.name + "   B Back to title";
}
