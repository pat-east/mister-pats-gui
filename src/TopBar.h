#pragma once

#include <string>
#include <vector>

#include "Geometry.h"
#include "Preferences.h"
#include "Theme.h"

class TabIcons;

class Canvas;

enum class Tab { Disc, Home, Favorites, Systems, Arcade, Games, Settings };

// Clock and date on the left, tabs in the middle, network on the right — all deliberately
// quiet so the content carries the screen.
class TopBar {
public:
    void update(float deltaSeconds, bool use24HourClock);
    void render(Canvas &canvas, Theme &theme, const Rect &area, Tab active, bool showGames,
                bool showArcade, bool showDisc, bool tabsEnabled, TabDisplay display,
                const TabIcons &icons);

    // The tabs actually reachable right now, in display order — Games drops out when its
    // Settings toggle is off, Arcade when that one is off or there are no Arcade games to
    // show; the other tabs always show.
    static std::vector<Tab> visibleTabs(bool showGames, bool showArcade, bool showDisc = false);
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
    float indicatorOpacity_ = 1.0f;
    Tab indicatorTab_ = Tab::Home;
    int indicatorFade_ = 0; // -1 fades out; 1 fades in
    bool indicatorTabSet_ = false;
    bool use24HourClock_ = true;
};
