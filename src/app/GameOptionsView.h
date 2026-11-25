#pragma once

// Game options: the panel the Classic GUI opens with the Select button on a game (and when a launch needs a
// version choice). One shared view for every tab that lists games: the picture (box art, or the video
// preview in its place) with the preview's status line, the title, the available versions ("Selected" marks
// the one Start launches), Start game, four screenshots and the picture viewer.
//
// How a tab uses it (a handful of lines; see src/app/README.md):
//
//     GameOptionsView options_{library_};
//     bool update(float dt) override { return options_.update(dt) || ...; }
//     void onShow() override { options_.show(); }  void onHide() override { options_.hide(); }
//     bool handle(Event e) override {
//         if (options_.isOpen()) return options_.handle(e);          // it takes every event while open
//         if (e == Event::Select && haveFocusedGame) { options_.open(focusedRef); return true; }
//         ...
//     }
//     std::vector<ui::Hint> hints() const override { if (options_.isOpen()) return options_.hints(); ... }
//     bool locked() const override { return options_.isOpen() || ...; }
//     std::string favoriteKey() const override { if (options_.isOpen()) return {}; ... }
//     void draw(...) { ...; options_.draw(p, t, p.bounds()); }       // last, over the tab
//     // after launch(ref) returned LaunchStatus::NeedsVersion:  options_.open(ref);
//
// The view does not draw the bottom bar: the shell draws the view's hints over the scrim (the tab reports
// locked()).

#include <string>
#include <vector>

#include "app/Library.h"
#include "app/DetailView.h"
#include "app/PreviewSlot.h"
#include "app/Tab.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/Modal.h"

namespace patsgui {

class GameOptionsView {
public:
    explicit GameOptionsView(Library &library) : library_(library), slot_(library) {}

    void open(const GameRef &game);
    void close();
    bool isOpen() const { return open_; }

    // The tab is the active one / was left: the preview runs only while shown.
    void show() { slot_.show(); }
    void hide() { slot_.hide(); close(); }

    // Every frame; true when it needs a redraw by itself (never today: the library's revision covers it).
    bool update(float deltaSeconds);
    // True when the event was used; while open that is every event.
    bool handle(Event event);
    std::vector<ui::Hint> hints() const;
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &canvas);
    // Only the video picture of the open panel, after a full draw (TabView::redrawPreview). The picture
    // lies on the panel's own surface, so nothing but the picture changes. False when the panel is closed,
    // the viewer is open, or no video was in the last draw.
    bool redrawPreview(ui::Painter &p, const ui::Theme &t, ui::Rect &damage) {
        return open_ && viewer_.empty() && slot_.redrawVideo(p, t, damage);
    }

    static void registerStrings(ui::Catalog &catalog);

private:
    void start();
    void openViewer();
    DetailSubject subject();   // what the shared DetailView shows for this game

    Library &library_;
    PreviewSlot slot_;
    DetailView view_;
    bool open_ = false;
    GameRef game_;
    GameDetail detail_;
    ui::ModalStack viewer_;  // the screenshot viewer
};

}  // namespace patsgui
