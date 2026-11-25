#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/Library.h"
#include "app/Notice.h"
#include "ui-framework/kit/Components.h"

namespace patsgui {

enum class Tab { Disc, Home, Favorites, Systems, Arcade, Games, Settings, Count };

// What the host maps its controller input onto. X, Y and Start are the face buttons by position
// (hints use ui::HintKey::X / Y / Start and follow the user's layout).
//   X, Back        short presses (Back is sent at once when B goes down, even if B is then held)
//   XHold          X held for 2 s, only while favoriteKey() of the active tab is unchanged and not empty
//   BHold          B held for 2 s, only while removeKey() of the active tab is unchanged and not empty
//   LetterPrev/Next  the trigger buttons: jump to the previous / next first letter
//   Select         the Select button: opens the Game options of the focused game (Classic does the same).
//                  A tab with a keyboard open ignores it; Start is its own event.
enum class Event { TabPrev, TabNext, Up, Down, Left, Right, Confirm, Back, X, Y, Start,
                   XHold, BHold, LetterPrev, LetterNext, Select };

// What a tab gets to work with. Lives as long as the GUI; tabs keep the reference.
struct SettingsServices;   // src/app/settings/SettingsServices.h

struct Services {
    Library &library;
    // The Settings tab's services seam (one interface per category). Null: every Settings function is
    // unavailable and the tab behaves as the plain first level. Set by the host after construction.
    SettingsServices *settings = nullptr;
    // Where a page says "this happened" (a toast above the bottom bar, src/app/Notice.h). Set by attach(); null in a
    // host that has no shell, so call it through notice(services.notice, ...).
    Notice *notice = nullptr;
    // Settings > Library > Arcade game status: switches to the Arcade tab and opens its status screen. Set by attach();
    // empty in a host that has no shell.
    std::function<void()> openArcadeStatus = nullptr;
};

// One tab's content. A tab owns its own state (cursor, scroll, open dialogs) and draws inside the area
// the shell leaves for it. It uses src/ui-framework/kit components, ui::Theme for every measure and color, and
// ui::Text keys for every word; it never blocks (see src/app/README.md).
class TabView {
public:
    virtual ~TabView() = default;

    // Once per frame while the GUI is open, for every tab (cheap when idle). Returns true when the
    // tab changed what it shows on its own (an animation, a timer) and needs a redraw.
    virtual bool update(float deltaSeconds) { (void)deltaSeconds; return false; }
    // The tab became the active one (refresh lists, reset transient state).
    virtual void onShow() {}
    // The tab is being left (another tab was chosen, or the GUI closes): stop what runs for it (the video
    // preview, a pending request) and close its overlays.
    virtual void onHide() {}
    virtual void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) = 0;
    // Returns true when the event was used. Unused Back goes to Home (nothing on Home); unused TabPrev/TabNext switch
    // tabs. A tab consumes those only while it has something open (a dialog, the keyboard).
    virtual bool handle(Event event) = 0;
    // The tab's own hints, added to the shell's "change tab" and "leave" hints.
    virtual std::vector<ui::Hint> hints() const { return {}; }
    // Repaints only the video picture after the host drew a whole frame, because only the video frame
    // changed (Library::previewFrameSerial()). Draws that one picture into its slot (the same code as the
    // full draw, so the pixels are identical), sets `damage` to the rectangle it changed and returns true.
    // Returns false when no video is showing in what the last draw() drew (or anything else makes a partial
    // draw wrong, e.g. an overlay on top): the host then does a full redraw. Forward to GameOptionsView /
    // PreviewSlot::redrawVideo() with a few lines; see src/app/README.md.
    virtual bool redrawPreview(ui::Painter &p, const ui::Theme &t, ui::Rect &damage) {
        (void)p; (void)t; (void)damage; return false;
    }
    // True while the tab has an overlay open (a dialog, the keyboard): the shell then shows only the tab's
    // hints and locks the tabs, as the framework demo does.
    virtual bool locked() const { return false; }
    // True while a dialog covers the page: the shell then neither draws nor counts down the toast. Defaults to
    // locked(); the Settings tab says no for a Content page (a progress view is not a dialog, toasts show over it).
    virtual bool dialogOpen() const { return locked(); }
    // The tab needs the user now and asks the shell to show it (Classic: the update restart prompt switches to Settings
    // wherever the user is). Checked every frame for every tab, shown or not.
    virtual bool wantsAttention() const { return false; }
    // A short note for the shell's footer, from any tab (empty = none): "v0.9.0 available" while an update is waiting.
    virtual ui::Text footerNote() const { return ui::Text(); }

    // Opaque key of the focused game when X may be held to toggle its favorite; empty when the focus is
    // not on such a game or an overlay is open. The host starts a hold only when this is non-empty and
    // cancels it when the key changes. List the hold in hints() with holdFavoriteHint().
    virtual std::string favoriteKey() const { return std::string(); }
    // The same for B: the focused game when it sits in a list it can be removed from (Recently played).
    // The host sends Back at once when B goes down, so a tab that returns a key here must consume Back
    // (return true) while it does, or the GUI closes before the hold completes. List the hold with
    // holdRemoveHint().
    virtual std::string removeKey() const { return std::string(); }
};

// The two hold hints: "Hold X Favorite" and "Hold B Remove". While the hold runs the shell replaces the
// help line with the same hint and a bar that fills over the 2 seconds (ui::drawHoldProgress). The tab
// asks for a different wording by giving its own hold hint on the same key (ui::Hint::hold = true); the
// shell uses the active tab's hold hint for the key it finds in hints().
ui::Hint holdFavoriteHint(bool isFavorite = false);   // isFavorite: "Hold X Remove favorite" instead of "Favorite"
ui::Hint holdRemoveHint();

using TabFactory = std::function<std::unique_ptr<TabView>(Services &)>;
using StringsFn = void (*)(ui::Catalog &);

// Registers the tab's factory (and optionally its strings) before main(). Use the macro in the
// tab's .cpp; no shared file needs an edit.
struct TabRegistrar {
    TabRegistrar(Tab tab, TabFactory factory, StringsFn strings = nullptr);
};

#define PATSGUI_REGISTER_TAB(tab, ViewClass, stringsFn) \
    static ::patsgui::TabRegistrar patsguiRegistrar_##ViewClass( \
        tab, [](::patsgui::Services &s) { return std::unique_ptr<::patsgui::TabView>(new ViewClass(s)); }, stringsFn)

// The factory for a tab, or an empty function when none is registered (the placeholder is used).
const TabFactory &tabFactory(Tab tab);
// Calls every registered strings function.
void registerTabStrings();

}  // namespace patsgui
