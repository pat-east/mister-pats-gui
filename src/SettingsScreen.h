#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Screen.h"
#include "SystemInfo.h"

// Two panes: what you can do on the left, what the machine is doing on the right.
//
// The split exists because the two kinds of line behave differently. An action responds to
// the D-pad; a reading does not, and mixing them into one list meant stepping the cursor
// through rows that never did anything.
class SettingsScreen : public Screen {
public:
    SettingsScreen(Context &context, std::function<void()> onReload,
                   std::function<void()> onQuit, std::function<void()> onBuildDatabase,
                   std::function<void()> onFetchArtwork,
                   std::function<void()> onManageSystems,
                   std::function<void()> onToggleGamesTab,
                   std::function<void()> onCycleDefaultView,
                   std::function<void()> onStartMisterCore);

    void setFramebufferInfo(const std::string &info) { framebufferInfo_ = info; }

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    struct Row {
        std::string label;
        std::function<std::string()> value;
        std::function<void()> activate;
    };

    // One line of the information pane. An empty label starts a new group.
    struct Fact {
        std::string label;
        std::string value;
    };

    void buildRows();
    std::vector<Fact> facts() const;
    void renderActions(Canvas &canvas, const Rect &area);
    void renderInfo(Canvas &canvas, const Rect &area);

    Context &context_;
    std::function<void()> onReload_;
    std::function<void()> onQuit_;
    std::function<void()> onBuildDatabase_;
    std::function<void()> onFetchArtwork_;
    std::function<void()> onManageSystems_;
    std::function<void()> onToggleGamesTab_;
    std::function<void()> onCycleDefaultView_;
    std::function<void()> onStartMisterCore_;
    std::vector<Row> rows_;
    SystemInfo system_;
    std::string framebufferInfo_;
    int cursor_ = 0;
    float focus_ = 0.0f;
};
