#pragma once

#include <csignal>
#include <memory>
#include <string>

#include "Canvas.h"
#include "Console.h"
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
#include "SystemsScreen.h"
#include "TopBar.h"

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

        // Diagnostic: an artificial pause on the splash screen before anything else runs, to
        // test whether boxart missing right after boot is a drive-not-ready-yet problem. 0
        // skips it. Off by default under --frames/--no-input, so headless tests stay fast.
        int splashMs = 0;
    };

    bool initialize(const Options &options);
    int run();
    void stop() { running_ = false; }

    // Asks for the next finished frame to be written out. Called from a signal
    // handler, so it only sets a flag; the work happens in the frame loop.
    void requestScreenshot() { screenshotRequested_ = 1; }

    static constexpr const char *kScreenshotPath = "/tmp/mister-gui-shot.raw";

private:
    Screen *activeScreen();
    void dispatch(Action action);
    void selectTab(Tab tab);
    void renderBackground(Canvas &target);
    void renderBottomBar(const Rect &area);
    void reloadLibrary();
    void openScan(bool firstRun);
    void openArtwork();
    void closeScan();
    void openVisibility();
    void closeVisibility();
    void toggleGamesTab();
    void cycleDefaultView();
    void writeCanvas(const std::string &path);

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
    History history_;
    HiddenSystems hiddenSystems_;
    Preferences preferences_;
    std::unique_ptr<Context> context_;

    Input input_;
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

    LibraryScan scan_;
    MediaScraper scraper_;
    bool scanActive_ = false;     // the database wizard owns the screen
    bool visibilityActive_ = false; // the show/hide list owns the screen
    bool detailActive_ = false;   // a system was opened from the systems tab
    bool needsFullRedraw_ = true; // set on start and whenever the screen changes

    Options options_;
    bool running_ = false;
    volatile sig_atomic_t screenshotRequested_ = 0;

    int64_t backgroundMs_ = 0;
    int64_t chromeMs_ = 0;
    int64_t screenMs_ = 0;
    int64_t presentMs_ = 0;
};
