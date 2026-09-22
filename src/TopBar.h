#pragma once

#include <string>
#include <vector>

#include "Geometry.h"
#include "Theme.h"

class Canvas;

enum class Tab { Home, Favorites, Systems, Games, Settings };

// Clock and date on the left, tabs in the middle, network on the right — all deliberately
// quiet so the content carries the screen.
class TopBar {
public:
    void update(float deltaSeconds);
    void render(Canvas &canvas, Theme &theme, const Rect &area, Tab active, bool showGames);

    // The tabs actually reachable right now, in display order — Games drops out when its
    // Settings toggle is off, everything else always shows.
    static std::vector<Tab> visibleTabs(bool showGames);
    static std::string label(Tab tab);

private:
    void refreshClock();
    void refreshNetwork();

    std::string time_ = "--:--";
    std::string date_;
    std::string link_ = "Offline";
    std::string address_;

    float clockTimer_ = 0.0f;
    float networkTimer_ = 0.0f;
    float indicatorX_ = -1.0f;
    float indicatorW_ = 0.0f;
};
