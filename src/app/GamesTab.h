#pragma once

// The Games tab of the new GUI: the games of ONE system in Grid, Grid-Small or Grid-List, with search,
// jump to letter, favorites and launching (including the version choice).
//
// Opening a system: the Systems tab (or the shell) calls openSystem() on the tab instance, then switches
// to the Games tab (onShow() keeps a system that was opened this way):
//
//     if (auto *games = dynamic_cast<patsgui::GamesTab *>(state.views[size_t(Tab::Games)].get()))
//         games->openSystem(systemId);
//
// openSystem(SystemId()) (also the default) shows all games of all systems, sorted by title.

#include <string>
#include <vector>

#include "app/GameTab.h"
#include "app/GridPage.h"
#include "ui-framework/kit/Grid.h"
#include "ui-framework/kit/Input.h"
#include "ui-framework/kit/PageHeader.h"

namespace patsgui {

class GamesTab : public GameTab {
public:
    explicit GamesTab(Services &services);

    // Shows the games of this system (keeps the cursor when it is the system already shown; clears a
    // running search when it is another one).
    void openSystem(const SystemId &system);
    const SystemId &system() const { return system_; }

    bool update(float deltaSeconds) override;
    void onShow() override;
    void onHide() override;
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event) override;
    std::vector<ui::Hint> hints() const override;
    // Test hook for the host renderer (src/app/host): set the view (0 Grid, 1 Grid-Small, 2 Grid-List).
    void setViewForTest(int view) { view_ = viewFromInt(view % 3); }
    // The Game options (versions, screenshots, video, Start game). Opens by itself when launching needs a
    // version choice; the shell can open it for the focused game (the Select button in the Classic GUI).
    void openGameOptions();
    // This is the Arcade tab's list: its view comes from Library::arcadeView() (Classic: the global Game view, Grid in
    // Individual mode) and a view change is not remembered (Classic gives the Arcade list no view of its own).
    void setArcade(bool arcade) { arcade_ = arcade; }

private:
    enum class Zone { Search, Games };

    bool focusedGame(GameInfo &out) const override;
    void libraryChanged(const GameRef &ref, bool favorite) override;
    bool listShowing() const override { return view_ == ViewMode::List && count() > 0; }

    bool searching() const { return search_.hasQuery(); }
    int count() const { return searching() ? int(results_.size()) : count_; }
    bool infoAt(int index, GameInfo &out) const;
    void ensureWindow(int first, int n);
    void invalidateWindow() { windowValid_ = false; list_.invalidateDetail(); }

    void setSystem(const SystemId &id);
    void readSystem();
    void applySearch(const std::string &query);
    void refreshResults();
    void clampCursor();
    void ensureZone();
    void jumpTo(int index);

    void handleCursorMove(int dx, int dy);

    ui::PageHeaderSpec headerSpec();
    void drawEmpty(ui::Painter &p, const ui::Theme &t, const ui::Rect &area);
    void drawGrid(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, bool small);
    void drawListView(ui::Painter &p, const ui::Theme &t, const ui::Rect &area);

    SystemId system_;
    std::string systemName_;
    ViewMode view_ = ViewMode::Grid;
    bool arcade_ = false;
    int startView(bool oneSystem) { return arcade_ ? library_.arcadeView() : library_.gamesView(oneSystem); }
    Zone zone_ = Zone::Games;
    int cursor_ = 0;
    int count_ = 0;
    uint64_t revision_ = 0;

    // Search.
    std::vector<GameInfo> results_;
    bool searchDone_ = true, searchMore_ = false;   // all matches found / more than the shown 500 exist

    // The window of games around the visible range.
    std::vector<GameInfo> window_;
    int windowFirst_ = 0;
    bool windowValid_ = false;

    GridPage grid_;

    bool focusResults_ = false;      // after a search: move to the games when the first results arrive
};

}  // namespace patsgui
