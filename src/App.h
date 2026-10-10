#pragma once

#include <csignal>
#include <memory>
#include <string>
#include <vector>

#include "ArcadeGamesScreen.h"
#include "ArcadeGroupsScreen.h"
#include "ArcadeScreen.h"
#include "ArcadeSettingsScreen.h"
#include "Canvas.h"
#include "Console.h"
#include "ControllersScreen.h"
#include "Framebuffer.h"
#include "GamesScreen.h"
#include "HiddenSystems.h"
#include "History.h"
#include "HomeScreen.h"
#include "Input.h"
#include "LibraryScan.h"
#include "MediaScraper.h"
#include "Preferences.h"
#include "ScanScreen.h"
#include "Screen.h"
#include "SettingsScreen.h"
#include "SystemVisibilityScreen.h"
#include "SystemIconDownload.h"
#include "SystemsScreen.h"
#include "TopBar.h"
#include "UpdateService.h"

// Owns the services, the screens and the frame loop.
class App {
public:
    // Lets a screen be driven straight from the command line, which makes it possible to
    // capture and review each view without a controller in hand.
    struct Options {
        Tab tab = Tab::Home;
        GameView view = GameView::Grid;
        bool viewExplicit = false;   // --view was passed; overrides the saved default view
        std::string system;      // empty picks the first one with games
        int exitAfterFrames = 0; // 0 runs until stopped
        bool readInput = true;   // open the input devices at all
        bool exclusive = false;  // keep other readers out (blocks the MiSTer OSD)
        std::string dumpPath;    // writes the finished canvas here before exiting
        bool launchNow = false;  // starts the preselected game right away
        bool dryRun = false;     // builds the MGL and prints it instead of loading the core
        bool stats = false;      // reports where frame time is spent
        bool incremental = true; // redraw and transfer only what changed
        bool wizard = true;      // offer to build the database when there is none
        bool scanOnly = false;   // build the database, report, and exit

        // Button presses fed to the interface before anything else, so a screen that takes a
        // few presses to reach can be captured without a controller in hand. Comma separated:
        // up down left right confirm back fav view prev next jumpprev jumpnext, and wait:N for
        // N frames. See --press.
        std::string script;

        // Diagnostic: an artificial pause on the splash screen before anything else runs, to
        // test whether boxart missing right after boot is a drive-not-ready-yet problem. 0
        // skips it. Off by default under --frames/--no-input, so headless tests stay fast.
        int splashMs = 0;
    };

    bool initialize(const Options &options);
    int run();
    void stop() { running_ = false; }
    void requestStopFromSignal() { signalStopRequested_ = 1; }

    // Asks for the next finished frame to be written out. Called from a signal
    // handler, so it only sets a flag; the work happens in the frame loop.
    void requestScreenshot() { screenshotRequested_ = 1; }

    static constexpr const char *kScreenshotPath = "/tmp/mister-gui-shot.raw";

    // Tells the patched Main_MiSTer not to relaunch this GUI, so the stock menu underneath
    // stays on screen instead of being covered again a few seconds later. Lives in /tmp, so
    // a reboot is what brings the GUI back.
    static constexpr const char *kSuspendMarkerPath = "/tmp/mister-pat-suspend-gui";

private:
    Screen *activeScreen();
    void dispatch(Action action);
    void selectTab(Tab tab);
    void renderBackground(Canvas &target);
    void renderBottomBar(const Rect &area);
    void reloadLibrary();
    void openScan(bool firstRun);
    void openArtwork();
    void startSystemIconDownload();
    void closeScan();
    void openVisibility();
    void closeVisibility();
    void openControllers();
    void closeControllers();
    void openArcadeSettings();
    void closeArcadeSettings();
    void openArcadeGames();
    void closeArcadeGames();
    void toggleGamesTab();
    void toggleArcadeTab();
    bool arcadeTabShown() const;
    bool tabNavigationAvailable() const;
    std::vector<Tab> visibleTabs() const;
    void openArcadeFull(ArcadeScreen::Dimension dimension);
    void openArcadeGroup(ArcadeScreen::Dimension dimension, const DatabaseGroup &group);
    void cycleDefaultView();
    void startMisterCore();
    void removePinnedVersion();
    void restartMisterAfterUpdate(bool removePin);
    void writeCanvas(const std::string &path);
    void runScript();

    std::string version;     // Version string to display in bottom bar

    ConsoleGuard console_;
    Framebuffer framebuffer_;
    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<Canvas> background_;   // prepared once, restored from per frame
    std::unique_ptr<Theme> theme_;

    Library library_;
    Favorites favorites_;
    Launcher launcher_;
    ImageCache images_;
    Icons icons_;
    std::unique_ptr<SystemIconDownload> systemIconDownload_;
    History history_;
    HiddenSystems hiddenSystems_;
    Preferences preferences_;
    std::unique_ptr<Context> context_;

    Input input_;
    Screen *favoriteHoldScreen_ = nullptr;
    std::string favoriteHoldPath_;
    TopBar topBar_;
    Tab tab_ = Tab::Home;

    std::unique_ptr<SystemsScreen> systemsScreen_;
    std::unique_ptr<HomeScreen> homeScreen_;
    std::unique_ptr<GamesScreen> gamesScreen_;   // per-system detail view
    std::unique_ptr<GamesScreen> allGamesScreen_;// the Games tab: the whole library at once
    std::unique_ptr<GamesScreen> favoritesScreen_;
    std::unique_ptr<SettingsScreen> settingsScreen_;
    std::unique_ptr<ScanScreen> scanScreen_;
    std::unique_ptr<SystemVisibilityScreen> visibilityScreen_;
    std::unique_ptr<ControllersScreen> controllersScreen_;
    std::unique_ptr<ArcadeSettingsScreen> arcadeSettingsScreen_;
    std::unique_ptr<ArcadeGamesScreen> arcadeGamesScreen_;
    std::unique_ptr<ArcadeScreen> arcadeScreen_;              // the Arcade tab
    std::unique_ptr<ArcadeGroupsScreen> arcadeGroupsScreen_;  // ...its Manufacturers/Categories list
    bool arcadeGroupsActive_ = false;   // that list is showing, between the tab and a group's games

    // One group's games are an ordinary GamesScreen over a copy of the Arcade system whose
    // database key names the group's list. Kept here so the pointer GamesScreen holds stays
    // valid for as long as it is showing; it is a copy of Arcade, not a system of its own,
    // so a favourite or history entry made from it still files under "Arcade".
    GameSystem arcadeGroupSystem_;

    LibraryScan scan_;
    MediaScraper scraper_;
    UpdateService updateService_;
    UpdateSnapshot updateSnapshot_;
    float updateCheckDelay_ = 8.0f;
    bool autoCheckDecisionMade_ = false;
    bool scanActive_ = false;     // the database wizard owns the screen
    bool visibilityActive_ = false; // the show/hide list owns the screen
    bool controllersActive_ = false; // Settings -> Controllers owns the screen
    bool arcadeSettingsActive_ = false; // Settings -> Manage Arcade owns the screen
    bool arcadeGamesActive_ = false;    // ...-> Arcade Games owns the screen, on top of that
    bool detailActive_ = false;   // a system was opened from the systems tab
    bool needsFullRedraw_ = true; // set on start and whenever the screen changes

    Options options_;
    bool running_ = false;
    volatile sig_atomic_t signalStopRequested_ = 0;
    volatile sig_atomic_t screenshotRequested_ = 0;

    std::vector<std::string> script_;   // Options::script, split; consumed from the front
    size_t scriptPosition_ = 0;
    int scriptWait_ = 0;

    int64_t backgroundMs_ = 0;
    int64_t chromeMs_ = 0;
    int64_t screenMs_ = 0;
    int64_t presentMs_ = 0;
};
