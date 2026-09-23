#include "SystemVisibilityScreen.h"

#include <algorithm>
#include <cstring>

#include "Canvas.h"

SystemVisibilityScreen::SystemVisibilityScreen(Context &context, std::function<void()> onClose)
    : context_(context), onClose_(std::move(onClose)) {}

void SystemVisibilityScreen::refresh() {
    names_.clear();
    cursor_ = 0;
    scroll_ = 0;

    // The same set the Systems tab would show if nothing were hidden — a system nobody could
    // ever see there is not worth a row here either.
    for (const GameSystem &system : context_.library.systems())
        if (context_.library.hasGames(system)) names_.push_back(system.name);

    std::sort(names_.begin(), names_.end(), [](const std::string &a, const std::string &b) {
        return strcasecmp(a.c_str(), b.c_str()) < 0;
    });

    focus_.assign(names_.size(), 0.0f);
}

void SystemVisibilityScreen::update(float deltaSeconds) {
    const float speed = std::min(1.0f, deltaSeconds * 9.0f);
    for (size_t i = 0; i < focus_.size(); ++i) {
        const float target = (int(i) == cursor_) ? 1.0f : 0.0f;
        focus_[i] += (target - focus_[i]) * speed;
    }
}

void SystemVisibilityScreen::handle(Action action) {
    switch (action) {
    case Action::Up:
        if (cursor_ > 0) --cursor_;
        break;
    case Action::Down:
        if (cursor_ + 1 < int(names_.size())) ++cursor_;
        break;
    case Action::Confirm:
        if (cursor_ >= 0 && cursor_ < int(names_.size()))
            context_.hiddenSystems.toggle(names_[size_t(cursor_)]);
        break;
    case Action::Back:
        onClose_();
        break;
    default:
        break;
    }
}

void SystemVisibilityScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    theme.bold().draw(canvas, area.x, area.y, "Systems", theme.sizeHeading(), theme.textPrimary);

    size_t hidden = 0;
    for (const std::string &name : names_)
        if (context_.hiddenSystems.contains(name)) ++hidden;

    char count[64];
    std::snprintf(count, sizeof(count), "%zu of %zu shown", names_.size() - hidden,
                  names_.size());
    const int width = theme.regular().measure(count, theme.sizeBody());
    theme.regular().draw(canvas, area.right() - width, area.y + theme.px(10), count,
                         theme.sizeBody(), theme.textMuted.withAlpha(160));

    const Rect body{area.x, area.y + headerHeight, std::min(area.w, theme.px(700)),
                    area.h - headerHeight};

    if (names_.empty()) {
        theme.regular().drawCentered(canvas, body, "No systems yet", theme.sizeHeading(),
                                     theme.textMuted);
        return;
    }

    const int rowHeight = theme.px(52);
    const int visible = std::max(1, body.h / rowHeight);

    if (cursor_ < scroll_) scroll_ = cursor_;
    if (cursor_ >= scroll_ + visible) scroll_ = cursor_ - visible + 1;
    const int maxScroll = std::max(0, int(names_.size()) - visible);
    scroll_ = std::min(std::max(0, scroll_), maxScroll);

    canvas.pushClip(body);
    for (int i = scroll_; i < std::min(int(names_.size()), scroll_ + visible); ++i) {
        const std::string &name = names_[size_t(i)];
        const Rect row{body.x, body.y + (i - scroll_) * rowHeight, body.w, rowHeight - theme.px(6)};
        const float focus = focus_[size_t(i)];
        const bool shown = !context_.hiddenSystems.contains(name);

        if (focus > 0.01f) {
            canvas.fillRoundedRect(row, theme.px(8),
                                   Color::lerp(theme.surface, theme.surfaceHi, focus)
                                       .withAlpha(uint8_t(220 * focus)));
            canvas.fillRoundedRect({row.x, row.y, theme.px(4), row.h}, theme.px(2),
                                   theme.accent.withAlpha(uint8_t(255 * focus)));
        }

        const int textX = row.x + theme.px(20);
        theme.regular().draw(canvas, textX, row.y + (row.h - theme.regular().lineHeight(theme.sizeBody())) / 2,
                             name, theme.sizeBody(),
                             shown ? Color::lerp(theme.textMuted, theme.textPrimary, focus)
                                   : theme.textMuted.withAlpha(120));

        // A small pill instead of plain text: readable at a glance down a long list, and it
        // does not depend on colour alone (green/red would fail for anyone colour-blind, so
        // the words themselves carry the meaning).
        const char *state = shown ? "Shown" : "Hidden";
        const int stateWidth = theme.bold().measure(state, theme.sizeSmall());
        const int pillW = stateWidth + theme.px(24);
        const int pillH = theme.px(28);
        const Rect pill{row.right() - pillW - theme.px(16), row.y + (row.h - pillH) / 2, pillW,
                        pillH};
        canvas.fillRoundedRect(pill, pillH / 2,
                               shown ? theme.accent.withAlpha(uint8_t(60 + 40 * focus))
                                     : theme.surfaceHi.withAlpha(200));
        theme.bold().drawCentered(canvas, pill, state, theme.sizeSmall(),
                                  shown ? theme.accent : theme.textMuted);
    }
    canvas.popClip();
}

std::string SystemVisibilityScreen::hints() const {
    return "A Show/Hide   B Back";
}
