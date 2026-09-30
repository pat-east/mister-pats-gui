#include "ArcadeGroupsScreen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Alphabet.h"
#include "Canvas.h"
#include "Tile.h"

namespace {

constexpr int kTileWidth = 250;   // design pixels, the same as GamesScreen's Grid

} // namespace

ArcadeGroupsScreen::ArcadeGroupsScreen(Context &context, OpenHandler onOpen)
    : context_(context), onOpen_(std::move(onOpen)) {}

void ArcadeGroupsScreen::show(bool manufacturers) {
    manufacturers_ = manufacturers;
    groups_ = context_.library.database().groupsFor(
        manufacturers ? GameDatabase::kArcadeManufacturers : GameDatabase::kArcadeCategories);

    // Alphabetical here, unlike the preview row's biggest-first: a full list is for finding
    // one by name, and that is what the letter jump needs.
    std::sort(groups_.begin(), groups_.end(), [](const DatabaseGroup &a, const DatabaseGroup &b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    focus_.assign(groups_.size(), 0.0f);
    cursor_ = 0;
    scrollRow_ = 0;
}

void ArcadeGroupsScreen::jumpLetter(int direction) {
    const int target = Alphabet::jump(cursor_, int(groups_.size()), direction,
                                      [this](int i) -> const std::string & {
                                          return groups_[size_t(i)].name;
                                      });
    if (target == cursor_) return;

    cursor_ = target;
    scrollRow_ = cursor_ / std::max(1, grid_.columns());

    const char letter = Alphabet::initial(groups_[size_t(cursor_)].name);
    context_.notify(letter == '#' ? std::string("0-9") : std::string(1, letter), 1.2f);
}

void ArcadeGroupsScreen::handle(Action action) {
    if (groups_.empty()) return;

    switch (action) {
    case Action::Up:    cursor_ = grid_.move(cursor_, int(groups_.size()), 0, -1); break;
    case Action::Down:  cursor_ = grid_.move(cursor_, int(groups_.size()), 0, 1); break;
    case Action::Left:  cursor_ = grid_.move(cursor_, int(groups_.size()), -1, 0); break;
    case Action::Right: cursor_ = grid_.move(cursor_, int(groups_.size()), 1, 0); break;
    case Action::Confirm: onOpen_(groups_[size_t(cursor_)]); break;
    case Action::JumpPrev: jumpLetter(-1); break;
    case Action::JumpNext: jumpLetter(1); break;
    default: break;
    }
}

void ArcadeGroupsScreen::update(float deltaSeconds) {
    const float speed = std::min(1.0f, deltaSeconds * 9.0f);
    for (size_t i = 0; i < focus_.size(); ++i) {
        const float target = (int(i) == cursor_) ? 1.0f : 0.0f;
        focus_[i] += (target - focus_[i]) * speed;
    }
}

void ArcadeGroupsScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    const std::string title = std::string("Arcade > ") +
                              (manufacturers_ ? "Manufacturers" : "Categories");
    theme.bold().draw(canvas, area.x, area.y, title, theme.sizeHeading(), theme.textPrimary);

    char info[96];
    std::snprintf(info, sizeof(info), "%d / %d  \xC2\xB7  %d %s", groups_.empty() ? 0 : cursor_ + 1,
                  int(groups_.size()), int(groups_.size()),
                  manufacturers_ ? "manufacturers" : "categories");
    const int width = theme.regular().measure(info, theme.sizeBody());
    theme.regular().draw(canvas, area.right() - width, area.y + theme.px(10), info,
                         theme.sizeBody(), theme.textMuted.withAlpha(160));

    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    if (groups_.empty()) {
        theme.regular().drawCentered(canvas, body,
                                     "Nothing here - rebuild the game database in the settings",
                                     theme.sizeHeading(), theme.textMuted);
        return;
    }

    const int labelHeight = Tile::captionHeight(theme, false, true);
    const Rect probe{0, 0, theme.px(kTileWidth), theme.px(kTileWidth)};
    const int growth = std::max(Tile::focusMarginX(probe), Tile::focusMarginY(probe)) + theme.px(3);

    grid_.configure(body.inset(growth), theme, kTileWidth, 1, 1, labelHeight);
    grid_.centerContent(int(groups_.size()));
    scrollRow_ = grid_.clampScroll(cursor_, int(groups_.size()), scrollRow_);

    const int first = scrollRow_ * grid_.columns();
    const int last = std::min(int(groups_.size()),
                              first + grid_.columns() * (grid_.visibleRows() + 2));

    canvas.pushClip(body);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = first; i < last; ++i) {
            const bool isCursor = (i == cursor_);
            if ((pass == 0) == isCursor) continue;

            const DatabaseGroup &group = groups_[size_t(i)];
            Tile::Content content;
            content.label = group.name;
            content.sublabel = std::to_string(group.count) + (group.count == 1 ? " game" : " games");
            content.showLabel = false;

            Tile::draw(canvas, theme, grid_.cellFrame(i, scrollRow_), content,
                       focus_[size_t(i)]);
        }
    }
    canvas.popClip();
}

std::string ArcadeGroupsScreen::hints() const {
    return "A Open   B Back   L2/R2 Letter";
}
