#include "ArcadeSettingsScreen.h"

#include <algorithm>

#include "Canvas.h"

ArcadeSettingsScreen::ArcadeSettingsScreen(Context &context,
                                          std::function<void()> onOpenArcadeGames,
                                          std::function<void()> onClose)
    : context_(context), onClose_(std::move(onClose)) {
    rows_.push_back({"Arcade Games", std::move(onOpenArcadeGames)});
}

void ArcadeSettingsScreen::update(float deltaSeconds) {
    focus_ += (1.0f - focus_) * std::min(1.0f, deltaSeconds * 9.0f);
}

void ArcadeSettingsScreen::handle(Action action) {
    switch (action) {
    case Action::Up:
        if (cursor_ > 0) --cursor_;
        break;
    case Action::Down:
        if (cursor_ + 1 < int(rows_.size())) ++cursor_;
        break;
    case Action::Confirm:
        if (rows_[size_t(cursor_)].activate) rows_[size_t(cursor_)].activate();
        break;
    case Action::Back:
        onClose_();
        break;
    default:
        break;
    }
}

void ArcadeSettingsScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    theme.bold().draw(canvas, area.x, area.y, "Manage Arcade", theme.sizeHeading(),
                      theme.textPrimary);

    const Rect body{area.x, area.y + headerHeight, std::min(area.w, theme.px(620)),
                    area.h - headerHeight};

    const int rowHeight = theme.px(64);
    for (size_t i = 0; i < rows_.size(); ++i) {
        const Row &row = rows_[i];
        const Rect frame{body.x, body.y + int(i) * rowHeight, body.w, rowHeight - theme.px(8)};
        if (frame.bottom() > body.bottom()) break;

        const bool active = int(i) == cursor_;
        canvas.fillRoundedRect(frame, theme.px(8),
                               active ? theme.surfaceHi : theme.surface.withAlpha(150));
        if (active) {
            canvas.fillRoundedRect({frame.x, frame.y, theme.px(4), frame.h}, theme.px(2),
                                   theme.accent);
        }

        const int textY = frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
        theme.regular().draw(canvas, frame.x + theme.px(22), textY, row.label, theme.sizeBody(),
                             active ? theme.textPrimary : theme.textMuted);
    }
}

std::string ArcadeSettingsScreen::hints() const { return "A Select   B Back"; }
