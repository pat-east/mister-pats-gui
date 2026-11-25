#include "app/PatsGui.h"

#include <algorithm>

#include "app/ArcadeTab.h"
#include "app/DetailView.h"
#include "app/EmptyPages.h"
#include "app/GameOptionsView.h"
#include "app/GamesTab.h"
#include "app/PreviewSlot.h"
#include "app/Strings.h"
#include "app/SystemsTab.h"
#include "app/settings/SettingsServices.h"
#include "ui-framework/kit/Primitives.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

namespace {

const char *kTabKeys[int(Tab::Count)] = {"tab.disc", "tab.home", "tab.favorites", "tab.systems",
                                         "tab.arcade", "tab.games", "tab.settings"};
const char *kTabIcons[int(Tab::Count)] = {"disc", "house", "star", "controller", "joystick", "grid-3x3-gap", "gear"};

}  // namespace

void registerStrings() {
    ui::Catalog &c = ui::Catalog::get();
    registerSharedStrings(c);
    c.set("nav.disk", "Disk usage");
    c.set("nav.disk.of", "of {1}");
    c.set("nav.disk.ofLow", "of {1} (!)");
    c.set("nav.ethernet", "Ethernet");
    c.set("nav.wifi", "Wi-Fi");
    c.set("nav.offline", "Offline");
    c.set("footer.version", "Version {1}");
    c.set("shell.noTab.title", "Nothing to show here");
    c.set("shell.noTab.why", "This tab has no content in this build.");
    registerFirstRunStrings(c);
    DetailView::registerStrings(c);
    GameOptionsView::registerStrings(c);
    registerTabStrings();
}

Assets makeAssets(const ui::Theme &t, ui::ImagePtr logo) {
    Assets a;
    a.logo = std::move(logo);
    for (int i = 0; i < int(Tab::Count); ++i) a.tabIcons.push_back(ui::makeBootstrapIcon(kTabIcons[i], t.px(26), t.px(26)));
    return a;
}

namespace {
void openArcadeStatusOf(State &state);   // below, next to switchTo()
bool refreshTabRow(State &state);        // below, next to switchTo()

InterfaceService *interfaceOf(const State &state) { return usable(state.interface); }

void sound(const State &state, UiSound which) {
    if (InterfaceService *i = interfaceOf(state)) i->playSound(which);
}
}

Tab resolveStartTab(const std::string &startTab, bool gamesVisible, bool arcadeVisible) {
    if (startTab == "favorites") return Tab::Favorites;
    if (startTab == "systems") return Tab::Systems;
    if (startTab == "games" && gamesVisible) return Tab::Games;
    if (startTab == "arcade" && arcadeVisible) return Tab::Arcade;
    return Tab::Home;
}

void attach(State &state, Services &services) {
    state.views.clear();
    state.interface = services.settings ? services.settings->interfaceService : nullptr;
    state.library = &services.library;
    state.services = &services;
    refreshTabRow(state);
    if (state.useStartTab)
        if (InterfaceService *i = interfaceOf(state))
            state.tab = resolveStartTab(i->startTab(), !state.hidden[int(Tab::Games)], !state.hidden[int(Tab::Arcade)]);
    services.notice = &state.notice;
    services.openArcadeStatus = [&state] { openArcadeStatusOf(state); };
    for (int i = 0; i < int(Tab::Count); ++i) {
        const TabFactory &factory = tabFactory(Tab(i));
        state.views.push_back(factory ? factory(services) : nullptr);
    }
    if (TabView *view = state.views[size_t(state.tab)].get()) view->onShow();
}

namespace {
TabView *activeView(const State &state) {
    return size_t(state.tab) < state.views.size() ? state.views[size_t(state.tab)].get() : nullptr;
}

// The toast is not shown, and its time does not run, while a dialog is open or a hold hint takes the help line.
bool toastSuppressed(const State &state) {
    const TabView *view = activeView(state);
    return state.firstRun.active() || (view && view->dialogOpen()) || state.favoriteHold > 0.0f || state.removeHold > 0.0f;
}
}  // namespace

namespace {
// The launch sound (Classic: App::run, when a core was started): once, when this GUI owns the sounds. Hosted by
// Classic's App it is App that plays it, so nothing plays twice.
void playLaunchSound(State &state) {
    if (!state.ownsSounds || state.launchSoundPlayed || !state.library || !state.library->standingDown()) return;
    state.launchSoundPlayed = true;
    sound(state, UiSound::GameLaunch);
}
}  // namespace

bool startFirstRun(State &state) {
    return state.services && state.firstRun.start(*state.services);
}

bool firstRunActive(const State &state) { return state.firstRun.active(); }

bool update(State &state, float deltaSeconds) {
    playLaunchSound(state);   // a disc start finishes in the library's update, not in an event
    bool redraw = refreshTabRow(state);
    for (auto &view : state.views)
        if (view && view->update(deltaSeconds)) redraw = true;
    const bool firstRun = state.firstRun.active();
    if (firstRun && state.firstRun.update(deltaSeconds)) redraw = true;
    if (firstRun && !state.firstRun.active()) redraw = true;   // it ended in this frame: the shell is back
    if (state.notice.get().update(deltaSeconds, toastSuppressed(state))) redraw = true;
    // A tab that needs the user (the update restart prompt) is shown wherever the user is (Classic: App::update selectTab);
    // not while the first start is open (the tabs are locked).
    for (size_t i = 0; i < state.views.size() && !state.firstRun.active(); ++i) {
        if (!state.views[i] || int(i) == int(state.tab) || !state.views[i]->wantsAttention()) continue;
        if (size_t(state.tab) < state.views.size() && state.views[size_t(state.tab)]) state.views[size_t(state.tab)]->onHide();
        state.tab = Tab(i);
        state.views[i]->onShow();
        state.backTo = Tab::Count;
        sound(state, UiSound::TabChange);
        redraw = true;
        break;
    }
    return redraw;
}

std::string favoriteKey(const State &state) {
    if (state.firstRun.active()) return std::string();   // nothing underneath reacts
    const TabView *view = activeView(state);
    return view ? view->favoriteKey() : std::string();
}

std::string removeKey(const State &state) {
    if (state.firstRun.active()) return std::string();
    const TabView *view = activeView(state);
    return view ? view->removeKey() : std::string();
}

namespace {

// The Games tab was opened from Systems and the user leaves it in another way than Back: it shows all
// games again the next time.
void forgetBackTarget(State &state) {
    if (state.backTo == Tab::Count) return;
    const bool fromSystems = state.backTo == Tab::Systems;
    state.backTo = Tab::Count;
    state.backSystem.clear();
    if (!fromSystems) return;
    if (auto *games = dynamic_cast<GamesTab *>(state.views[size_t(Tab::Games)].get())) games->openSystem(SystemId());
}

}  // namespace

namespace {

// Changes the active tab: the one left is told (onHide), the one reached too (onShow).
void switchTo(State &state, Tab to) {
    if (to == state.tab) return;
    if (TabView *old = activeView(state)) old->onHide();
    state.tab = to;
    if (TabView *now = activeView(state)) now->onShow();
    sound(state, UiSound::TabChange);   // Classic: App::selectTab, gated by Menu sounds; not played at start (attach)
}

// Classic's App::visibleTabs() and arcadeTabShown(), read from the Interface service and the Library every frame (the
// Disc tab only while a drive and the Disc package are there). A tab that just vanished while it was the current one
// gives way to Home (App::toggleGamesTab/toggleArcadeTab, reloadLibrary, the Disc check in App::run); a hidden tab that
// is shown anyway (the Arcade status opened from Settings) stays until the user leaves it.
bool refreshTabRow(State &state) {
    InterfaceService *i = interfaceOf(state);
    const Tab tabs[3] = {Tab::Games, Tab::Arcade, Tab::Disc};
    bool nowHidden[3];
    nowHidden[0] = i && !i->gamesTabVisible();
    nowHidden[1] = i && !i->arcadeTabVisible();
    nowHidden[2] = state.library && !state.library->discTabAvailable();
    bool changed = false;
    for (int k = 0; k < 3; ++k) {
        const bool wasHidden = state.hidden[int(tabs[k])];
        state.hidden[int(tabs[k])] = nowHidden[k];
        if (wasHidden == nowHidden[k]) continue;
        changed = true;
        if (nowHidden[k] && state.tab == tabs[k] && state.backTo == Tab::Count && !state.views.empty()) switchTo(state, Tab::Home);
    }
    return changed;
}

// Settings > Library > Arcade game status (Classic: App::openArcadeGames()): the Arcade tab's status screen, which
// starts the check of the games (the user asked for it).
void openArcadeStatusOf(State &state) {
    auto *arcade = dynamic_cast<ArcadeTab *>(state.views[size_t(Tab::Arcade)].get());
    if (!arcade) return;
    switchTo(state, Tab::Arcade);
    state.backTo = Tab::Settings;   // closing the status screen returns to Settings (offerToView, highlightedTab)
    arcade->openStatus();
}

}  // namespace

namespace {

// Offers the event to the active tab; true when the tab used it. After a used event the shell does what the tab
// asked of it (Systems asked to open a system: the Games tab shows it; the Arcade status screen opened from Settings
// was closed: back to Settings, as in Classic where it is a Settings sub-screen).
bool offerToView(State &state, Event event) {
    TabView *view = activeView(state);
    if (!view || !view->handle(event)) return false;
    if (state.tab == Tab::Systems) {
        auto *systems = dynamic_cast<SystemsTab *>(view);
        auto *games = dynamic_cast<GamesTab *>(state.views[size_t(Tab::Games)].get());
        const SystemId wanted = systems ? systems->takeOpenRequest() : SystemId();
        if (games && !wanted.empty()) {
            games->openSystem(wanted);
            state.backTo = Tab::Systems;
            state.backSystem = wanted;
            switchTo(state, Tab::Games);
        }
    }
    if (state.tab == Tab::Arcade && state.backTo == Tab::Settings) {
        auto *arcade = dynamic_cast<ArcadeTab *>(view);
        if (arcade && !arcade->statusOpen()) {
            state.backTo = Tab::Count;
            switchTo(state, Tab::Settings);
        }
    }
    return true;
}

}  // namespace

std::vector<Tab> visibleTabs(const State &state) {
    std::vector<Tab> out;
    for (int i = 0; i < int(Tab::Count); ++i)
        if (!state.hidden[i]) out.push_back(Tab(i));
    return out;
}

Tab highlightedTab(const State &state) {
    if (state.tab == Tab::Arcade && state.backTo == Tab::Settings) return Tab::Settings;   // the status opened from Settings
    // The list opened from Consoles is a screen of the Systems tab in Classic: Systems stays highlighted (also when the Games
    // tab is shown in the row), so LB/RB go on from Systems.
    if (state.tab == Tab::Games && state.backTo != Tab::Count) return state.backTo;
    if (!state.hidden[int(state.tab)]) return state.tab;
    return state.tab == Tab::Arcade ? Tab::Settings : Tab::Home;                        // the status opened from Settings
}

void playStartSound(State &state) {
    if (state.ownsSounds) sound(state, UiSound::Start);
}

bool handle(State &state, Event event) {
    // The first start owns every event (it is a modal flow over the shell); the shell never closes from it.
    if (state.firstRun.active()) {
        state.firstRun.handle(event);
        return false;
    }
    // Classic (App::dispatch): the navigation sound comes first, before anything is routed, for the six movement actions.
    if (state.ownsSounds && (event == Event::Up || event == Event::Down || event == Event::Left || event == Event::Right ||
                             event == Event::LetterPrev || event == Event::LetterNext))
        sound(state, UiSound::Navigation);
    // Classic (App::dispatch): Start acts as A everywhere except where a screen keeps it for itself (a keyboard, the
    // search). So the tab is offered Start first; when it does not use it, Start is a Confirm.
    if (event == Event::Start) {
        if (offerToView(state, Event::Start)) return false;
        event = Event::Confirm;
    }
    if (offerToView(state, event)) return false;
    switch (event) {
        case Event::TabPrev:
        case Event::TabNext: {   // Classic: over the tabs of the row, with wrap, from the highlighted one
            const std::vector<Tab> row = visibleTabs(state);
            const int count = int(row.size());
            const int at = int(std::find(row.begin(), row.end(), highlightedTab(state)) - row.begin());
            forgetBackTarget(state);
            switchTo(state, row[size_t((at + (event == Event::TabNext ? 1 : count - 1)) % count)]);
            break;
        }
        case Event::Back:
            // Back in the Games tab, opened from a system: return to the Systems tab on that system.
            if (state.tab == Tab::Games && state.backTo == Tab::Systems) {
                const SystemId system = state.backSystem;
                forgetBackTarget(state);
                if (auto *systems = dynamic_cast<SystemsTab *>(state.views[size_t(Tab::Systems)].get()))
                    systems->focusSystem(system);
                switchTo(state, Tab::Systems);
                break;
            }
            // The Arcade game status opened from Settings while the Arcade tab is hidden: Back returns to Settings.
            if (state.tab == Tab::Arcade && state.hidden[int(Tab::Arcade)]) {
                forgetBackTarget(state);
                switchTo(state, Tab::Settings);
                break;
            }
            // Classic (App.cpp, Back): on any other tab Back selects Home. On Home it does nothing: the new GUI is
            // the standard interface and is not left with B (decision of the owner; Classic is `--classic`).
            if (state.tab != Tab::Home) {
                forgetBackTarget(state);
                switchTo(state, Tab::Home);
            }
            break;
        default: break;
    }
    return false;
}

ui::ShellSpec shellFor(const State &state, const SystemValues &values, const Assets &assets) {
    ui::ShellSpec spec;
    const Tab highlighted = highlightedTab(state);
    for (int i = 0; i < int(Tab::Count); ++i) {
        if (state.hidden[i]) continue;   // Classic: the tab row has only the visible tabs
        ui::TabSpec ts;
        ts.label = ui::Text::key(kTabKeys[i]);
        if (size_t(i) < assets.tabIcons.size()) ts.icon = assets.tabIcons[size_t(i)];
        if (Tab(i) == highlighted) spec.activeTab = int(spec.tabs.size());
        spec.tabs.push_back(ts);
    }
    spec.tabDisplay = state.tabDisplay;

    ui::NavItemSpec clock;
    clock.primary = ui::Text::data(values.clock);
    clock.secondary = ui::Text::data(values.date);
    clock.isClock = true;
    clock.priority = 10;
    spec.left.push_back(clock);

    ui::NavItemSpec disk;
    disk.primary = ui::Text::key("nav.disk");
    // "fat:    7% of 128GB" / "usb0:  80% of 512GB": the item lines the columns up. A nearly full volume says so in
    // words as well as in colour (Classic: "(!)").
    for (const DiskVolume &v : values.disks) {
        ui::NavRow row;
        row.label = ui::Text::data(v.name + ":");
        row.value = ui::Text::data(std::to_string(v.percentUsed) + "%");
        row.note = ui::Text::data(ui::format(v.low ? "nav.disk.ofLow" : "nav.disk.of", {v.size}));
        row.tone = v.low ? ui::Tone::Warning : ui::Tone::Neutral;
        disk.rows.push_back(row);
    }
    disk.priority = 5;
    spec.right.push_back(disk);
    ui::NavItemSpec net;
    // Classic (TopBar): the link type over the address; "Offline" alone, in the warning colour, without a link.
    switch (values.link) {
        case NetworkLink::Ethernet:
            net.primary = ui::Text::key("nav.ethernet");
            net.icon = ui::IconId::Network;
            net.tone = ui::Tone::Done;
            break;
        case NetworkLink::Wifi:
            net.primary = ui::Text::key("nav.wifi");
            net.icon = ui::IconId::Wifi;
            net.tone = ui::Tone::Done;
            break;
        case NetworkLink::Offline:
            net.primary = ui::Text::key("nav.offline");
            net.icon = ui::IconId::Alert;
            net.tone = ui::Tone::Warning;
            break;
    }
    if (values.link != NetworkLink::Offline) net.secondary = ui::Text::data(values.address);
    net.priority = 4;
    spec.right.push_back(net);

    const TabView *view = activeView(state);
    if (state.firstRun.active()) {
        for (const ui::Hint &h : state.firstRun.hints()) spec.help.push_back(h);   // the flow's own hints are the whole help
        spec.tabsLocked = true;
        view = nullptr;
    }
    if (view)
        for (const ui::Hint &h : view->hints()) spec.help.push_back(h);
    if (state.firstRun.active()) {
        // no shell hints: see above
    } else if (view && view->locked()) {
        spec.tabsLocked = true;   // an overlay is open: its own hints are the whole help
    } else {
        spec.help.push_back(ui::hint(ui::HintKey::Shoulders, "hint.changeTab", true));
        // A tab that names its own B action (e.g. "Back to Systems") replaces the shell's "Back".
        bool tabNamesB = false;
        if (view)
            for (const ui::Hint &h : view->hints())
                if (h.key == ui::HintKey::B && !h.hold) tabNamesB = true;
        // B goes back to Home from every other tab (Classic); on Home it does nothing, so no hint is shown there.
        if (!tabNamesB && state.tab != Tab::Home) spec.help.push_back(ui::hint(ui::HintKey::B, "hint.back", true));
    }
    // A hold in progress takes over the help line (the active tab's own hold hint for that button, else
    // the generic one) so the bar is visible whatever else the help contained.
    auto holdHint = [&](ui::HintKey key, const ui::Hint &fallback) {
        if (view)
            for (const ui::Hint &h : view->hints())
                if (h.hold && h.key == key) return h;
        return fallback;
    };
    if (state.favoriteHold > 0.0f) {
        spec.hold.active = true;
        spec.hold.hint = holdHint(ui::HintKey::X, holdFavoriteHint());
        spec.hold.progress = state.favoriteHold;
    } else if (state.removeHold > 0.0f) {
        spec.hold.active = true;
        spec.hold.hint = holdHint(ui::HintKey::B, holdRemoveHint());
        spec.hold.progress = state.removeHold;
    }
    ui::FooterTextSpec version;
    version.text = ui::Text::data(ui::format("footer.version", {values.version}));
    spec.footer.push_back(version);
    for (const auto &v : state.views) {   // a note from a tab, shown on every tab (an update is available)
        if (!v) continue;
        const ui::Text note = v->footerNote();
        if (note.empty()) continue;
        ui::FooterTextSpec n;
        n.text = note;
        n.tone = ui::Tone::Action;
        spec.footer.push_back(n);
    }
    return spec;
}

void drawTab(ui::Painter &p, const ui::Theme &t, State &state, const ui::Rect &content) {
    if (TabView *view = activeView(state)) view->draw(p, t, content);
    else ui::drawEmptyPage(p, t, content, emptyPage("shell.noTab.title", "shell.noTab.why"));   // a tab nobody registered
    state.firstRun.draw(p, t, content);   // the first start, over the tab (it dims the whole screen itself)
}

bool redrawPreview(State &state, ui::Painter &p, const ui::Theme &t, ui::Rect &damage) {
    TabView *view = activeView(state);
    if (state.firstRun.active() || !view || !view->redrawPreview(p, t, damage)) return false;
    // A toast over the repainted picture would have to be drawn again on top of it: a full redraw does that.
    const ui::Toast &toast = state.notice.get();
    return !(toast.visible() && !toast.rect(p, t, p.bounds()).intersect(damage).empty());
}

void drawOverlays(ui::Painter &p, const ui::Theme &t, State &state, const ui::ShellSpec &spec) {
    const TabView *view = activeView(state);
    if (!toastSuppressed(state)) state.notice.get().draw(p, t, p.bounds());   // over the page, never over a dialog
    if (!state.firstRun.active() && (!view || !view->locked())) return;
    // A dialog dims the screen; the help bar stays readable above it and shows the dialog's own help.
    const ui::Rect all = p.bounds();
    p.pushClip({0, all.h - t.bottomBarHeight(), all.w, t.bottomBarHeight()});
    ui::drawPage(p, t, all);
    ui::drawShellBottomBar(p, t, spec);
    p.popClip();
}

}  // namespace patsgui
