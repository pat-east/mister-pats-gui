#include "SystemsScreen.h"

#include <cmath>
#include <cstdio>

#include "Canvas.h"
#include "LoadingIndicator.h"

namespace {

// Let the drive and the FPGA settle after boot before touching the library.
constexpr float kSettleSeconds = 3.0f;

} // namespace

#include "Tile.h"

SystemsScreen::SystemsScreen(Context &context, OpenHandler onOpen)
    : context_(context), onOpen_(std::move(onOpen)) {
    refresh();
}

void SystemsScreen::refresh() {
    systems_.clear();
    focus_.clear();
    cursor_ = 0;
    scrollRow_ = 0;
    scanned_ = 0;
    total_ = context_.library.systems().size();
    settleDelay_ = kSettleSeconds;

    paintedFocus_.clear();
    paintedScrollRow_ = -1;
    paintedCount_ = size_t(-1);
    paintedImages_ = uint64_t(-1);
}

void SystemsScreen::scanStep(float deltaSeconds) {
    if (scanned_ >= total_) return;

    if (settleDelay_ > 0.0f) {
        settleDelay_ -= deltaSeconds;
        return;
    }

    const GameSystem &system = context_.library.systems()[scanned_++];
    if (context_.library.hasGames(system) && !context_.hiddenSystems.contains(system.name)) {
        systems_.push_back(&system);
        focus_.push_back(0.0f);
    }

    if (scanned_ >= total_)
        std::printf("systems: %zu with games\n", systems_.size());
}

void SystemsScreen::requestImages(int first, int last) {
    for (int i = first; i < last; ++i) {
        const GameSystem *system = systems_[size_t(i)];
        context_.images.get(context_.icons.pathFor(system->name), grid_.tileWidth(),
                            grid_.tileHeight());
    }
}

const GameSystem *SystemsScreen::current() const {
    if (systems_.empty()) return nullptr;
    return systems_[size_t(std::min(std::max(cursor_, 0), int(systems_.size()) - 1))];
}

void SystemsScreen::update(float deltaSeconds) {
    scanStep(deltaSeconds);

    const float speed = std::min(1.0f, deltaSeconds * 9.0f);
    for (size_t i = 0; i < focus_.size(); ++i) {
        const float target = (int(i) == cursor_) ? 1.0f : 0.0f;
        focus_[i] += (target - focus_[i]) * speed;
    }
}

void SystemsScreen::handle(Action action) {
    if (systems_.empty()) return;

    switch (action) {
    case Action::Up:    cursor_ = grid_.move(cursor_, int(systems_.size()), 0, -1); break;
    case Action::Down:  cursor_ = grid_.move(cursor_, int(systems_.size()), 0, 1); break;
    case Action::Left:  cursor_ = grid_.move(cursor_, int(systems_.size()), -1, 0); break;
    case Action::Right: cursor_ = grid_.move(cursor_, int(systems_.size()), 1, 0); break;
    case Action::Confirm:
        if (const GameSystem *system = current()) onOpen_(*system);
        break;
    default:
        break;
    }
}

void SystemsScreen::render(Canvas &canvas, const Rect &area, bool fullRedraw) {
    Theme &theme = context_.theme;
    Font &bold = theme.bold();

    const int headerHeight = theme.px(58);
    const Rect header{area.x, area.y, area.w, headerHeight};
    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    if (systems_.empty()) {
        // The dot animates every frame regardless of anything this screen itself tracks, so
        // — unlike the grid below, which restores only the region it is about to repaint —
        // this whole area has to be wiped first every time or each frame's text and dot paint
        // over the last one instead of replacing it.
        if (context_.background) canvas.restoreFrom(*context_.background, area);

        if (scanning()) {
            const int panelWidth = std::min(area.w, theme.px(700));
            const Rect panel{area.x + (area.w - panelWidth) / 2,
                             area.y + area.h / 2 - theme.px(70), panelWidth, theme.px(200)};
            const float fraction = total_ ? float(scanned_) / float(total_) : 0.0f;
            char counts[64];
            std::snprintf(counts, sizeof(counts), "%zu of %zu systems", scanned_, total_);
            LoadingIndicator::draw(canvas, theme, panel, "Reading your library", fraction,
                                   std::string(), counts);
        } else {
            bold.drawCentered(canvas, area, "No systems with games found", theme.sizeHeading(),
                              theme.textMuted);
        }
        return;
    }

    // The focused tile grows beyond its cell, so the layout keeps a margin inside the clip.
    const int growth = theme.px(18);
    grid_.configure(body.inset(growth), theme, 250, 4, 3, 0);
    grid_.centerContent(int(systems_.size()));
    scrollRow_ = grid_.clampScroll(cursor_, int(systems_.size()), scrollRow_);

    if (context_.background) canvas.restoreFrom(*context_.background, header);
    bold.draw(canvas, header.x, header.y, "Systems", theme.sizeHeading(), theme.textPrimary);

    const GameSystem *active = current();
    if (active) {
        char counter[96];
        std::snprintf(counter, sizeof(counter), "%s  ·  %d systems", active->group.c_str(),
                      int(systems_.size()));
        const int width = theme.regular().measure(counter, theme.sizeBody());
        theme.regular().draw(canvas, header.right() - width, header.y + theme.px(10), counter,
                             theme.sizeBody(), theme.textMuted.withAlpha(160));
    }

    const int first = scrollRow_ * grid_.columns();
    const int last = std::min(int(systems_.size()),
                              first + grid_.columns() * (grid_.visibleRows() + 1));

    // A focused tile grows and casts an offset shadow, so it paints beyond its cell.
    auto footprint = [&](int index) {
        return Tile::footprint(theme, grid_.cellFrame(index, scrollRow_));
    };

    // Asking here, before anything below decides whether to repaint, is what keeps icons
    // loading on a screen that has otherwise gone still: this request used to live only
    // inside the draw passes further down, which are skipped once nothing is left
    // animating — so once the focus settled, nothing ever asked the cache again, and
    // any icon that missed that frame's decode budget stayed missing forever.
    requestImages(first, last);

    // Taken after that request, so a decode it just triggered still counts as a change
    // this frame rather than waiting until the next one.
    const uint64_t imagesBefore = context_.images.generation();

    // Work out what actually changed since the last frame. `fullRedraw` covers a wipe the
    // caller already did for a reason of its own — a tab switch, returning from a detail
    // view — which our own tracking below has no way to see for itself.
    const bool layoutChanged = fullRedraw || scrollRow_ != paintedScrollRow_ ||
                               systems_.size() != paintedCount_ ||
                               paintedFocus_.size() != focus_.size() ||
                               imagesBefore != paintedImages_;

    Rect dirty{};
    if (!layoutChanged) {
        for (int i = first; i < last; ++i) {
            if (std::fabs(focus_[size_t(i)] - paintedFocus_[size_t(i)]) < 0.002f) continue;
            const Rect f = footprint(i);
            dirty = dirty.empty() ? f : Rect{std::min(dirty.x, f.x), std::min(dirty.y, f.y),
                                            std::max(dirty.right(), f.right()) - std::min(dirty.x, f.x),
                                            std::max(dirty.bottom(), f.bottom()) - std::min(dirty.y, f.y)};
        }
        if (dirty.empty()) {
            paintedScrollRow_ = scrollRow_;
            return;   // nothing moved; leave the canvas as it is
        }
    }

    const Rect repaint = layoutChanged ? body : dirty.intersect(body);
    if (context_.background) canvas.restoreFrom(*context_.background, repaint);

    canvas.pushClip(body);

    // See the identical note in GamesScreen::renderGrid: ask for the focused tile's icon
    // before the two passes below, which draw it last so its shadow and border sit above its
    // neighbours — a draw order that used to cost it the frame's decode budget too.
    if (cursor_ >= first && cursor_ < last) {
        const GameSystem *focused = systems_[size_t(cursor_)];
        context_.images.get(context_.icons.pathFor(focused->name), grid_.tileWidth(),
                            grid_.tileHeight());
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = first; i < last; ++i) {
            const bool isCursor = (i == cursor_);
            if ((pass == 0) == isCursor) continue;
            if (!layoutChanged && footprint(i).intersect(repaint).empty()) continue;

            const GameSystem *system = systems_[size_t(i)];
            Tile::Content content;
            content.label = system->name;
            content.sublabel = system->launchable() ? std::string() : std::string("no core");
            content.coverArt = true;

            const Rect frame = grid_.cellFrame(i, scrollRow_);
            const std::string icon = context_.icons.pathFor(system->name);
            content.image = context_.images.get(icon, grid_.tileWidth(), grid_.tileHeight());

            Tile::draw(canvas, theme, frame, content, focus_[size_t(i)]);
        }
    }

    canvas.popClip();

    paintedFocus_ = focus_;
    paintedScrollRow_ = scrollRow_;
    paintedCount_ = systems_.size();
    paintedImages_ = imagesBefore;
}

std::string SystemsScreen::hints() const {
    return "A Open   Y View   LB/RB Tabs";
}
