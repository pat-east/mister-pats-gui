#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Screen.h"
#include "app/Library.h"
#include "app/Look.h"
#include "app/PatsGui.h"
#include "bridge/settings/SettingsAdapters.h"
#include "ui-framework/render/soft/Canvas.h"
#include "ui-framework/render/soft/Font.h"
#include "ui-framework/render/soft/RetainedFrame.h"

// Development tool (Settings -> Developer -> MiSTer Pats GUI (UI-Framework)): the new GUI on the UI
// framework (src/app/), hosted inside this GUI like WireframesScreen. It draws its own complete screen
// (splash with the real icon, then the main-screen layout) into the framework's canvas and Back leaves it.
class PatsGuiScreen : public Screen {
public:
    PatsGuiScreen(Context &context, std::function<void()> onClose);

    // Starts over: shows the splash, then the first tab.
    // `atStart`: the GUI is opened as the program starts (the start splash was already shown): no second splash.
    void open(bool atStart = false);
    // Opens the first-start guide (Welcome, Jump Start, overview: src/app/FirstRun.h) over the shell, in place of Classic's ScanScreen
    // first-run pages. App::init calls it right after open(true) when the guide is not complete. When it ends the screen opens again
    // (the Jump Start has just built the library the first open did not have).
    void startFirstRun();

    void update(float deltaSeconds) override;
    // Not used: the demo owns the whole screen, so App calls renderFull() instead.
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    // Draws the screen into `canvas`. Only what changed since the last frame is repainted and copied (ui::RetainedFrame: a
    // display list compared per 32 px tile; src/app/README.md "Rendering and damage"), so the canvas's damage is just those
    // rectangles, which App presents alone. `fullRedraw`: App needs the whole screen (something else drew on its canvas).
    void renderFull(Canvas &canvas, bool fullRedraw);
    // App reports how long presenting the frame took (for the preview timing log).
    void notePresent(double milliseconds);
    void handle(Action action) override;
    std::string hints() const override { return std::string(); }
    // The hold machinery of App (X 2 s, B 2 s) works on these: the focused game's key of the active tab.
    std::string favoritePath() const override { return splash_ ? std::string() : patsgui::favoriteKey(state_); }
    std::string removablePath() const override { return splash_ ? std::string() : patsgui::removeKey(state_); }
    bool incremental() const override { return true; }
    // The new GUI decides what Start means (Done on a keyboard, the search, else the same as A: patsgui::handle), so
    // App must not turn it into Confirm first.
    bool usesStartAction() const override { return true; }

private:
    void ensureResources(int width, int height);
    void refreshSystemValues();
    void logPreviewStats();
    void logInputStats(bool force);
    // Draws the whole screen description into the display list and paints the changed tiles on canvas_; returns the
    // repainted rectangles (all of the screen when `everything`).
    const std::vector<ui::Rect> &paintFrame(bool everything);
    // Applies the Classic settings that decide the look (text and hint size, high contrast and its color, color
    // theme, dark mode, font files) to the framework theme and fonts when they differ from what was applied.
    // Cheap (a few ints and two strings compared); redraws only on a change.
    void syncLook();

    Context &context_;
    std::function<void()> onClose_;

    std::unique_ptr<ui::Font> regular_;
    std::unique_ptr<ui::Font> bold_;
    std::unique_ptr<ui::Canvas> canvas_;
    std::unique_ptr<ui::RetainedFrame> frame_;   // the display list and tile hashes of what canvas_ shows
    std::unique_ptr<ui::Theme> theme_;
    uint64_t noticeSerial_ = 0;           // Context::messageSerial already shown as a toast
    patsgui::LookSettings appliedLook_;   // what theme_ and the fonts show now
    patsgui::Assets assets_;

    // The real library when the Classic library has systems, otherwise the sample data.
    std::unique_ptr<patsgui::Library> library_;
    std::unique_ptr<patsgui::Services> services_;
    // The Settings tab's services: one adapter per category on the Classic objects (src/legacy/bridge/settings/).
    std::unique_ptr<SettingsAdapters> settingsAdapters_;
    uint64_t drawnRevision_ = 0;
    patsgui::State state_;
    patsgui::SystemValues values_;
    float splashSeconds_ = 0.0f;
    bool splash_ = true;
    int lastMinute_ = -1;
    bool lastUse24_ = true;       // the clock was last built with this Time format and time zone
    std::string lastZone_;
    bool dirty_ = true;
    bool firstRunSeen_ = false;   // the first-start guide was open: when it ends the screen reopens on the library it built

    // Video preview: a new frame (Library::previewFrameSerial) without any other change repaints only the
    // video picture. The numbers go to the debug log about every 5 s while a video plays.
    uint64_t drawnSerial_ = 0;
    bool previewDirty_ = false;
    struct PreviewStats {
        double start = 0;                  // seconds, steady clock; 0 = no window open
        int partial = 0, full = 0, presents = 0;
        double partialMs = 0, fullMs = 0, presentMs = 0;
    } stats_;

    // Input latency and frame cost, one debug-log line per ~5 s window while keys are pressed (never per frame).
    struct InputStats {
        double start = 0;                       // the window opened at the first key; 0 = none open
        int keys = 0, frames = 0, fullFrames = 0, presents = 0;
        double recordMs = 0, recordMax = 0;     // building the display list: shell + tab + overlays
        double paintMs = 0, paintMax = 0;       // rasterising the changed tiles
        double copyMs = 0, copyMax = 0;         // copying them onto App's canvas
        double presentMs = 0, presentMax = 0;   // App's present to the framebuffer
        double dirtyPixels = 0;
        int latencies = 0;
        double latencySum = 0, latencyMax = 0;  // key handled -> frame with it presented
    } input_;
    std::vector<double> keyStamps_;             // handled keys whose frame has not been drawn yet
    std::vector<double> inFlight_;              // drawn, waiting for App's present
};
