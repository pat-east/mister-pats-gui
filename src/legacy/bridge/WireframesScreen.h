#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Screen.h"
#include "app/Look.h"
#include "ui-framework/demo/Demo.h"
#include "ui-framework/render/soft/Canvas.h"
#include "ui-framework/render/soft/Font.h"

// Development tool (Settings -> Developer -> UI-Framework Wireframes): hosts the UI framework's demo
// inside this GUI. The demo draws its own complete screen (splash, then the main-screen layout with
// tabs, nav items, help and footer) into the framework's canvas; this screen copies it to the GUI's
// canvas when something changed. It holds the display and the controller while it is open, like the
// other full-screen views, and Back leaves it.
class WireframesScreen : public Screen {
public:
    WireframesScreen(Context &context, std::function<void()> onClose);

    // Starts over: shows the splash, then the first tab.
    void open();

    void update(float deltaSeconds) override;
    // Not used: the demo owns the whole screen, so App calls renderFull() instead.
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void renderFull(Canvas &canvas, bool fullRedraw);
    void handle(Action action) override;
    std::string hints() const override { return std::string(); }
    bool incremental() const override { return true; }

private:
    void ensureResources(int width, int height);
    void refreshSystemValues();
    void syncFonts();   // reloads the fonts in place when the GUI's font files changed (Space Grotesk on/off)

    Context &context_;
    std::function<void()> onClose_;

    std::unique_ptr<ui::Font> regular_;
    std::unique_ptr<ui::Font> bold_;
    std::unique_ptr<ui::Canvas> canvas_;
    std::unique_ptr<ui::Theme> theme_;
    patsgui::LookSettings appliedLook_;   // the font files the fonts show now
    ui::demo::Assets assets_;

    ui::demo::State state_;
    ui::demo::SystemValues values_;
    float splashSeconds_ = 0.0f;
    bool splash_ = true;
    int lastMinute_ = -1;
    bool dirty_ = true;
};
