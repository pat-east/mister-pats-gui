#pragma once

// The base of the tabs that list games and act on them (Home, Games, Arcade's games view, Favorites). It owns what
// those tabs have in common, so each tab writes only what is its own (which games it shows, where its cursor is):
//
//   the overlays   Game options, the removal question, the search keyboard: when one is open the tab is locked, the
//                  shell shows only its hints, and the events belong to it (handleOverlay());
//   the hold keys  X held toggles the favorite, B held removes a recently played game: ONE key format
//                  (`system + '\n' + id`), empty while an overlay is open or no game has the focus;
//   the preview    the video of the focused game in the list view's detail panel, started and stopped with the tab
//                  and the overlays (updateGlue(), redrawPreview());
//   the actions    play (a launch that needs a version choice opens the Game options), toggle a favorite (removing
//                  asks first, then a notice), remove a recently played game, with the same notices everywhere.
//
// It does NOT own a focus model: Home keeps its four zones (search, recent, favorites, results), the others their
// two (search field, games). A tab says only what is focused (focusedGame()) and what changed (libraryChanged()).
//
//   class MyTab : public GameTab {
//       bool focusedGame(GameInfo &out) const override;        // the game under the cursor, false on the search field
//       void libraryChanged(const GameRef &, bool) override;   // re-read the lists / patch the cached entries
//       bool listShowing() const override;                      // the List with details view has the focus
//       bool handle(Event e) override {
//           ConfirmRemove::Result removal;
//           switch (handleOverlay(e, removal)) { case Overlay::None: break; case Overlay::Used: return true;
//               case Overlay::SearchSubmitted: applySearch(search_.query()); return true;
//               case Overlay::RemovalConfirmed: applyRemoval(removal); return true; }
//           ...
//       }
//   };

#include <string>
#include <vector>

#include "app/ConfirmRemove.h"
#include "app/GameOptionsView.h"
#include "app/ListDetail.h"
#include "app/Notice.h"
#include "app/SearchHeader.h"
#include "app/Tab.h"

namespace patsgui {

class GameTab : public TabView {
public:
    // The one key format of the hold keys: the game's system and id. Opaque to the host.
    static std::string gameKey(const GameRef &ref) { return ref.system + '\n' + ref.id; }

    void onShow() override;
    void onHide() override;
    // True while an overlay is open: the shell then shows only the overlay's hints and locks the tabs.
    bool locked() const override { return overlayOpen(); }
    bool redrawPreview(ui::Painter &p, const ui::Theme &t, ui::Rect &damage) override;
    std::string favoriteKey() const override;

protected:
    explicit GameTab(Services &services);

    // ---- What a tab tells the base.
    // The game under the cursor; false when the focus is not on a game (the search field, an empty list).
    virtual bool focusedGame(GameInfo &out) const = 0;
    // A favorite was toggled or a recently played game removed: re-read the lists, patch the cached entries.
    virtual void libraryChanged(const GameRef &ref, bool favorite) { (void)ref; (void)favorite; }
    // The List with details view is showing the focused game (its preview video is wanted, a partial redraw is possible).
    virtual bool listShowing() const { return false; }

    // ---- Overlays.
    bool overlayOpen() const { return options_.isOpen() || search_.typing() || confirm_.open(); }
    enum class Overlay { None, Used, SearchSubmitted, RemovalConfirmed };
    // Routes the event to the open overlay. None: nothing is open, the tab goes on. Used: the overlay took it.
    // SearchSubmitted: the keyboard's Done, the tab runs search_.query(). RemovalConfirmed: the user said yes, `removal`
    // names the game that was asked about (apply it with applyRemoval()).
    Overlay handleOverlay(Event event, ConfirmRemove::Result &removal);
    // The open overlay's hints; false when none is open.
    bool overlayHints(std::vector<ui::Hint> &out) const;
    // Draws the overlays over the tab (the Game options, the removal question, the keyboard).
    void drawOverlays(ui::Painter &p, const ui::Theme &t);

    // ---- Every frame: the Game options, and the list's preview video for `listFocus` (empty: none wanted).
    // True when it needs a redraw by itself.
    bool updateGlue(float deltaSeconds, const GameRef &listFocus);

    // ---- Actions.
    void play(const GameRef &game);                          // a version choice opens the Game options
    void openOptions(const GameRef &game);                   // Select: the Game options (not over another overlay)
    void requestToggleFavorite(const GameInfo &game);        // adds at once (notice), removing asks first
    void askRemoveRecent(const GameInfo &game);
    void applyRemoval(const ConfirmRemove::Result &removal); // after the question was answered yes

    Library &library_;
    Notice *notice_;
    GameOptionsView options_;
    ConfirmRemove confirm_;
    SearchHeader search_;      // the field, the keyboard, the query and the Library search (one flow)
    ListDetail list_;          // List with details: the rows, the detail panel and the preview slot
};

}  // namespace patsgui
