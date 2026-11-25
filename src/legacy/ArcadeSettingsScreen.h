#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Screen.h"

// Settings -> Manage Arcade. A short menu of its own, the same way "Manage systems" and
// "Controllers" get one, reached from Settings — room to grow into DIP switches, rescanning,
// and whatever else docs/ARCADE.md's own roadmap ends up needing, without the main Settings list
// itself growing a second, unrelated section for just this.
class ArcadeSettingsScreen : public Screen {
public:
    ArcadeSettingsScreen(Context &context, std::function<void()> onOpenArcadeGames,
                        std::function<void()> onClose);

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    struct Row {
        std::string label;
        std::function<void()> activate;
    };

    Context &context_;
    std::function<void()> onClose_;
    std::vector<Row> rows_;
    int cursor_ = 0;
    float focus_ = 0.0f;
};
