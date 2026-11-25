#pragma once

// The Arcade tab of the new GUI. Like Classic, the tab is the Games view (Grid, Small grid, List with
// details; search, jump to letter, favorites by holding X, version choice) over the arcade system, plus
// the "Arcade game status" screen (the diagnostic table and the check of the games).
//
// The status screen is reached with X while the focus is not on a game (the search field, or an empty
// tab), or from Settings > Library > Arcade game status: the shell (PatsGui.cpp, `openArcadeStatusOf`) switches to
// this tab and calls openStatus(); when the screen is closed there the shell returns to Settings (State::backTo).

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/GameOptionsView.h"
#include "app/GamesTab.h"
#include "app/Tab.h"
#include "ui-framework/kit/List.h"

namespace patsgui {

class ArcadeTab : public TabView {
public:
    explicit ArcadeTab(Services &services);

    // Opens the status screen and starts the check of the games (the user asked for it).
    void openStatus();
    // True while the status screen is showing (the shell returns to Settings when it closes there, PatsGui.cpp).
    bool statusOpen() const { return status_; }

    bool update(float deltaSeconds) override;
    void onShow() override;
    void onHide() override;
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event) override;
    std::vector<ui::Hint> hints() const override;
    bool locked() const override;
    bool redrawPreview(ui::Painter &p, const ui::Theme &t, ui::Rect &damage) override {
        if (status_) return options_.redrawPreview(p, t, damage);   // the Game options' video, over the status table
        return hasArcade_ && games_.redrawPreview(p, t, damage);
    }
    std::string favoriteKey() const override;

private:
    void readArcadeSystem();
    void rebuildStatus();
    void closeStatus();
    void ensureWindow(int first, int count);
    const GameInfo *gameAt(int index) const;
    bool fetchDetails();
    void drawStatus(ui::Painter &p, const ui::Theme &t, const ui::Rect &content);
    bool handleStatus(Event event);
    void play();
    void moveCursor(int delta);

    Library &library_;
    Notice *notice_;
    GamesTab games_;
    SystemId arcadeId_;
    bool hasArcade_ = false;
    uint64_t revision_ = 0;

    // The status screen.
    bool status_ = false;
    Works works_ = Works::All;
    int total_ = 0, allTotal_ = 0, cursor_ = 0;
    ui::ListLayout list_;                   // the rows' scroll and page step: the same layout every list uses
    std::vector<GameInfo> window_;
    int windowFirst_ = 0;
    bool windowValid_ = false;
    struct Files { std::string core, rom; };
    std::map<std::string, Files> files_;    // detail() answers by game key
    GameOptionsView options_{library_};     // the version choice: the same Game options panel as every tab
    int textScale_ = 100;
    bool glyphs_ = true;
};

}  // namespace patsgui
