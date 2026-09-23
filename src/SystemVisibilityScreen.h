#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Screen.h"

// A plain scrolling list of every system that has games, each with an on/off state, reached
// from the settings. Off means the Systems tab skips it — for someone with a library that
// spans forty machines but three they actually play, that is most of the point of the tab.
class SystemVisibilityScreen : public Screen {
public:
    SystemVisibilityScreen(Context &context, std::function<void()> onClose);

    // Rebuilds the list from the current library. Called each time the screen is opened, so
    // a database rebuild in between is reflected rather than showing a stale list.
    void refresh();

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    Context &context_;
    std::function<void()> onClose_;

    std::vector<std::string> names_;   // every system with games, sorted
    std::vector<float> focus_;
    int cursor_ = 0;
    int scroll_ = 0;
};
