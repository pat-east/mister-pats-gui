#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/FirstRun.h"
#include "app/Library.h"
#include "app/Tab.h"
#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Toast.h"

namespace patsgui {

class InterfaceService;   // src/app/settings/InterfaceService.h

// MiSTer Pats GUI on the UI framework (parallel to the Classic GUI until 0.9.0). This layer knows the
// application's tabs, texts and screens; everything it draws comes from src/ui-framework/kit. It must not include
// anything from src/legacy/ (the Classic GUI): the host (src/legacy/bridge/PatsGuiScreen) supplies pictures and system values.
// Tab and Event are in src/app/Tab.h. Each tab's content is a TabView (registered in its own file); a tab
// without one shows the placeholder.
// The shell's notice: the one toast above the bottom bar (ui::Toast). Pages call it through Services::notice.
class ShellNotice : public Notice {
public:
    void toast(const ui::Text &text, ui::Tone tone = ui::Tone::Done, float minSeconds = 0.0f) override { toast_.show(text, tone, minSeconds); }
    ui::Toast &get() { return toast_; }
private:
    ui::Toast toast_;
};

struct State {
    Tab tab = Tab::Home;
    // How the tab row shows its tabs (names, icons or both): the user's setting, set by the host.
    ui::TabDisplay tabDisplay = ui::TabDisplay::Names;
    std::vector<std::unique_ptr<TabView>> views;   // one slot per tab; empty slot = placeholder

    // Progress (0..1) of the X and B holds, set by the host every frame (0 = not held). The shell shows
    // it in the bottom bar; tabs do not draw it.
    float favoriteHold = 0.0f;
    float removeHold = 0.0f;

    // The notice sink that attach() hands to the tabs (Services::notice). Do not move a State after attach().
    ShellNotice notice;

    // Where the Games tab was opened from a system (Systems -> A): Back returns there (Tab::Systems). Where the Arcade
    // status screen was opened from Settings: closing it returns there (Tab::Settings). Tab::Count = none.
    Tab backTo = Tab::Count;
    SystemId backSystem;

    // ---- Interface settings (Settings > Interface; Classic: App::visibleTabs(), App::init(), UiSounds)
    // The Interface category's service, set by attach() from Services::settings (null or unavailable: every tab is
    // shown and no sound plays).
    InterfaceService *interface = nullptr;
    Library *library = nullptr;   // set by attach(): the Disc tab follows Library::discTabAvailable()
    // Tabs the tab row leaves out (Classic: Games when "Show Games tab" is off; Arcade when "Show Arcade tab" is off or
    // the library has no Arcade games; Disc without a drive or the Disc package). Refreshed every frame by update();
    // false = shown.
    bool hidden[int(Tab::Count)] = {};
    // attach() opens the user's Start tab (resolveStartTab) instead of `tab`: the hosts that show the GUI to the user set
    // it, the host runner's per-tab renders do not.
    bool useStartTab = false;
    // True when this GUI plays the Navigation, GameLaunch and Start sounds itself. While it is hosted in Classic's App,
    // App already plays them (App::dispatch, the run loop, App::init), so this stays false; the TabChange sound is
    // always ours (the tab switch happens here, not in App::selectTab).
    bool ownsSounds = false;
    bool launchSoundPlayed = false;   // the GameLaunch sound played once for the launch that is handing over to the core

    // The first start (src/app/FirstRun.h): while it is open it is drawn over the shell and owns every event and the hints; the
    // tabs are locked and nothing underneath reacts. Started by startFirstRun(); it ends itself (the guide is complete) or
    // closes with the GUI (the destructor).
    FirstRun firstRun;
    Services *services = nullptr;   // set by attach(): what startFirstRun() hands to the flow
};

// One mounted volume for the Disk usage item: "fat", "usb0", ... (in that order), how full it is and its size.
struct DiskVolume {
    std::string name;        // "fat", "usb0", ...
    int percentUsed = 0;
    std::string size;        // "128GB"
    bool low = false;        // 5 % or less is left (Classic's warning)
};
enum class NetworkLink { Offline, Wifi, Ethernet };

struct SystemValues {
    std::string clock, date, address;
    NetworkLink link = NetworkLink::Offline;   // address is empty when offline
    std::string version;     // the running version for the footer, "0.9.1" (no "v"; digits as released)
    std::vector<DiskVolume> disks;
};

struct Assets {
    ui::ImagePtr logo;                       // the application icon, for the splash
    std::vector<ui::IconPtr> tabIcons;
};

void registerStrings();
Assets makeAssets(const ui::Theme &t, ui::ImagePtr logo);

// The tab Classic opens on at start for a Start tab value (App::init): "favorites" and "systems" always, "games" only
// while the Games tab is shown, "arcade" only while the Arcade tab is shown, anything else (and a hidden tab): Home.
Tab resolveStartTab(const std::string &startTab, bool gamesVisible, bool arcadeVisible);
// The tabs of the tab row now, in order (Classic: App::visibleTabs()), and the one the row highlights: the active tab,
// or, when the active tab is not in the row (the Games list opened from Consoles, the Arcade game status opened from
// Settings), the tab it was opened from.
std::vector<Tab> visibleTabs(const State &state);
Tab highlightedTab(const State &state);
// The start sound (Classic: after the splash); plays only when this GUI owns the sounds.
void playStartSound(State &state);

// Creates the registered tab views on top of the library (call once, after the state is created).
void attach(State &state, Services &services);
// Opens the first-start guide (Welcome, Jump Start, overview) over the shell. The host calls it once after attach() when the guide
// is not complete (Classic: App::init, openScan(true)). False: the services lack what it needs; nothing opens.
bool startFirstRun(State &state);
bool firstRunActive(const State &state);
// Once per frame; true when something changed on its own and needs a redraw.
bool update(State &state, float deltaSeconds);

// The active tab's hold keys (empty = a hold must not start). The host calls these every frame.
std::string favoriteKey(const State &state);
std::string removeKey(const State &state);

// Applies an event; returns true when the application asks to be closed (never from the shell today: Back on Home does nothing).
bool handle(State &state, Event event);

ui::ShellSpec shellFor(const State &state, const SystemValues &values, const Assets &assets);
void drawTab(ui::Painter &p, const ui::Theme &t, State &state, const ui::Rect &content);
// After a whole frame was drawn and only the video frame changed: repaints the active tab's video picture
// alone. True with `damage` set to the changed rectangle; false when a full redraw is needed instead.
bool redrawPreview(State &state, ui::Painter &p, const ui::Theme &t, ui::Rect &damage);
// Call after drawTab with the same shell spec: keeps the help bar readable above a tab's dialog.
void drawOverlays(ui::Painter &p, const ui::Theme &t, State &state, const ui::ShellSpec &spec);

}  // namespace patsgui
