#pragma once

// The Systems tab: the consoles and computers that have games, as a grid of tiles, plus a list to show or
// hide systems. Arcade systems are listed in the visibility list; the grid shows them only when the "Show Arcade in
// Consoles" preference is on (the Arcade tab has them otherwise).
//
// Opening a system (the seam to the Games tab). The tab never knows the Games tab. Pressing A on a tile
// records the request; the application shell fetches it with takeOpenRequest() right after handle() (empty id
// = none; reading it clears it).
// After the shell has opened the Games tab for that system it may call focusSystem() when the user comes
// back, so the cursor is on the system they left.

#include <string>
#include <vector>

#include "app/GridPage.h"
#include "app/Library.h"
#include "app/Tab.h"
#include "ui-framework/kit/Grid.h"
#include "ui-framework/kit/List.h"

namespace patsgui {

class SystemsTab : public TabView {
public:
    explicit SystemsTab(Services &services);

    SystemId takeOpenRequest();
    void focusSystem(const SystemId &system);

    bool update(float deltaSeconds) override;
    void onShow() override;
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event) override;
    std::vector<ui::Hint> hints() const override;

private:
    void reload();
    int visibleIndex(const SystemId &id) const;
    void drawGrid(ui::Painter &p, const ui::Theme &t, const ui::Rect &content);
    void drawVisibility(ui::Painter &p, const ui::Theme &t, const ui::Rect &content);

    Library &library_;
    uint64_t revision_ = 0;
    std::vector<SystemInfo> all_;      // every system with games (hidden or not, arcade too), in the catalog order
    std::vector<SystemInfo> shown_;    // the ones the grid shows
    int cursor_ = 0;                   // in shown_
    int manageCursor_ = 0;             // in all_
    bool showArcade_ = false;          // the "Show Arcade in Consoles" preference as last read
    bool managing_ = false;            // the visibility list is open
    GridPage grid_;
    ui::ListLayout list_;
    SystemId request_;
};

}  // namespace patsgui
