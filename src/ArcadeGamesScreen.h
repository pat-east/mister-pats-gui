#pragma once

#include <functional>
#include <vector>

#include "ArcadeScan.h"
#include "Screen.h"

// Settings -> Manage Arcade -> Arcade Games: a diagnostic table over every .mra this GUI can
// find, one row per game — name, year, manufacturer, category, whether its core and ROM zip
// are actually present, whether the ROM's contents actually match the .mra's own CRCs, and a
// single "might work" verdict combining all of that.
//
// Exists because none of that could be told at a glance the day this feature was designed —
// see ARCADE.md's own Killer Instinct case study, which needed an SSH session and reading
// MiSTer's source directly to explain a game that looked fully installed and still would not
// start. This screen is that same check, done once for the whole library instead of one game
// at a time by hand.
class ArcadeGamesScreen : public Screen {
public:
    ArcadeGamesScreen(Context &context, std::function<void()> onClose);

    // Starts (or restarts) the scan. Called each time the screen is opened, so a library
    // change since the last visit is reflected rather than showing a stale table.
    void refresh();

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    // All the same row, just a different subset of `sorted_` — everything, only what looks
    // playable, or only what does not. Reached with Y (this screen has no view presentations
    // to cycle through, so that action is free for this instead).
    enum class Filter { All, Working, Broken, kCount };
    static const char *filterLabel(Filter filter);

    void ensureSorted();
    void rebuildVisible();
    void jumpLetter(int direction);
    void jumpPage(int direction);
    void launch();
    void renderTable(Canvas &canvas, const Rect &area);

    Context &context_;
    std::function<void()> onClose_;

    ArcadeScan scan_;
    std::vector<ArcadeEntry> sorted_;    // every entry, alphabetical — built once, when done
    bool haveSorted_ = false;

    Filter filter_ = Filter::All;
    std::vector<int> visible_;           // indices into `sorted_` that pass `filter_`

    std::vector<float> focus_;           // one per `visible_` entry
    int cursor_ = 0;                     // index into `visible_`
    int scroll_ = 0;
    int lastVisibleRows_ = 1;            // rows the table last actually fit, for L2/R2 paging
};
