#pragma once

#include <string>
#include <functional>

#include "Favorites.h"
#include "DiscDriveService.h"
#include "GamePreview.h"
#include "GameScreenshotService.h"
#include "DiscCoreManager.h"
#include "DiscCoreInstaller.h"
#include "DiscArtworkService.h"
#include "TheGamesDBClient.h"
#include "HiddenSystems.h"
#include "History.h"
#include "Icons.h"
#include "ImageCache.h"
#include "Input.h"
#include "Launcher.h"
#include "Library.h"
#include "Preferences.h"
#include "Theme.h"

class Canvas;
class LibraryScan;
class MediaScraper;
class UpdateService;
class BackupTask;

// What the new GUI's Settings adapters (src/legacy/bridge/settings/) need from App beyond the shared objects above: the objects
// App owns that are not in Context, and the App operations that carry App state (screens, tabs, redraws). App
// fills it once, next to `Context::input` (App.cpp); a null pointer or empty function means "not available".
// Added for the Settings port; src/app/settings/PORTING.md lists which adapter uses what.
struct SettingsHost {
    LibraryScan *scan = nullptr;          // Library: Build game list
    MediaScraper *scraper = nullptr;      // Library: Prepare box art
    UpdateService *updates = nullptr;     // Updates; System (commitRunning, changelog)
    BackupTask *backup = nullptr;         // System: Back up GUI settings
    Input *input = nullptr;               // Controllers: the writable input (Context::input is read-only)
    // Controllers: true while the mapping wizard or the input test needs every raw event. App then does not
    // translate the pad into Actions (the same switch as ControllersScreen::wantsRawInput()).
    bool rawInputWanted = false;
    std::function<void()> reloadLibrary;                 // App::reloadLibrary()
    std::function<void()> startMisterCore;               // App::startMisterCore(): leave for the MiSTer menu
    std::function<void()> startUpdateCheck;              // autoCheckDecisionMade_ = true; updateService_.startCheck(false)
    std::function<void()> startAssetDownload;            // App::startAssetDownload()
    std::function<std::string()> assetDownloadStatus;    // App::assetDownloadStatus()
    std::function<void()> activateSpaceGrotesk;          // App::activateSpaceGrotesk()
    std::function<std::string()> spaceGroteskStatus;     // App::spaceGroteskStatus()
    std::function<void()> toggleGamesTab;                // App::toggleGamesTab()
    std::function<void()> toggleArcadeTab;               // App::toggleArcadeTab()
    std::function<void()> cycleDefaultView;              // App::cycleDefaultView()
    std::function<void()> openWireframes;                // App::openWireframes()
    std::function<std::string()> framebufferInfo;        // Framebuffer::describe(): the Resolution fact of System -> Info
    // System (added by the System category). The backup/reboot/icon operations of Classic split into what the new GUI can
    // call without App's notify() and showError(), and the state App keeps privately.
    std::function<bool(std::string &error)> rebootMister;   // App::rebootMister() without showError: sync + reboot; false + strerror(errno)
    std::function<bool()> startBackup;                   // backupTask_.start() and backupStateReported_ = Idle (no notify)
    std::function<bool()> backupUnreported;              // finished and state != backupStateReported_ (what App::update would report)
    std::function<void()> acknowledgeBackup;             // backupStateReported_ = the current state: App::update will not report it
    std::function<bool()> assetDownloadRunning;          // App::assetsDownloadActive_
    std::function<bool()> assetDownloadFailed;           // the system or tab icon download state is Failed
    std::function<std::string()> assetDownloadError;     // the system icon error, else the tab icon error
    std::function<int()> assetsMissing;                  // icons_.missingBitmaps().size() + tabIcons_.missingFiles().size() (optional)
    std::function<bool()> spaceGroteskRunning;           // spaceGroteskDownload_.running() (optional)
    std::function<bool()> spaceGroteskFailed;            // spaceGroteskDownload_.state() == Failed (optional)
    std::function<std::string()> spaceGroteskError;      // spaceGroteskDownload_.error() (optional)
};

// Whether a game counts as a favourite: its own path, or any of its versions. A title with several
// ROM versions (Arcade revisions, regional releases) is one entry whose own path is just the
// representative one, while the favourite may have been marked on another version.
inline bool isFavoriteGame(const Favorites &favorites, const Game &game) {
    if (favorites.contains(game.path)) return true;
    for (const Game::Variant &version : game.versions)
        if (favorites.contains(version.path)) return true;
    return false;
}

// Shared services handed to every screen.
struct Context {
    Context(Theme &t, Library &l, Favorites &f, Launcher &la, ImageCache &i, Icons &ic,
            History &h, HiddenSystems &hs, Preferences &p, DiscDriveService &dd,
            DiscCoreManager &dc, DiscArtworkService &da, TheGamesDBClient &tgdb,
            DiscCoreInstaller &di, GamePreview &gp)
        : theme(t), library(l), favorites(f), launcher(la), images(i), icons(ic), history(h),
          hiddenSystems(hs), preferences(p), discDrive(dd), discCores(dc), discArtwork(da),
          theGamesDB(tgdb), discInstaller(di), preview(gp) {}

    Theme &theme;
    Library &library;
    Favorites &favorites;
    Launcher &launcher;
    ImageCache &images;
    Icons &icons;
    History &history;
    HiddenSystems &hiddenSystems;
    Preferences &preferences;
    DiscDriveService &discDrive;
    DiscCoreManager &discCores;
    DiscArtworkService &discArtwork;
    TheGamesDBClient &theGamesDB;
    DiscCoreInstaller &discInstaller;
    GamePreview &preview;
    std::function<void()> onThemeChanged;
    std::function<void(const GameSystem &, const Game &)> onChooseGameVersion;
    std::function<void()> onGameVersionSettingChanged;

    // The Game options panel is open: it shows the video preview itself, so the list under it
    // must not start one for its own selection.
    bool detailOpen = false;

    // Prepared background, for restoring the area under a changed element.
    const Canvas *background = nullptr;

    std::string message;      // transient status line shown in the bottom bar
    float messageTimer = 0.0f;
    // Counts every notify() call (also a repeat of the same text): the new GUI shows each one as a toast.
    uint64_t messageSerial = 0;

    // Incremented whenever the library was reloaded (App::reloadLibrary); lets adapters drop caches.
    uint64_t libraryRevision = 0;

    // The controller input, read-only: the new GUI shows the progress of the X and B holds.
    const Input *input = nullptr;

    // The screenshot lookup of the Game options (owned by App; one worker for every user of it).
    GameScreenshotService *gameScreenshots = nullptr;

    // The new GUI's Settings adapters (see SettingsHost above).
    SettingsHost settingsHost;

    // Set after starting a game: the core needs the inputs and the screen.
    bool standDown = false;

    void notify(const std::string &text, float seconds = 3.5f) {
        message = text;
        messageTimer = seconds;
        ++messageSerial;
    }

    // A failure the user has to actually notice and dismiss — see ErrorModal.h. App owns
    // drawing it and routing Confirm/Back to close it; this is just the shared state so any
    // screen or service (Launcher, a scan, the scraper) can raise one without depending on App.
    std::string errorTitle;
    std::string errorMessage;
    bool errorActive = false;

    void showError(const std::string &title, const std::string &text) {
        errorTitle = title;
        errorMessage = text;
        errorActive = true;
    }
};

class Screen {
public:
    virtual ~Screen() = default;

    virtual void update(float deltaSeconds) = 0;

    // `fullRedraw` is true when the caller has already wiped the whole area back to the
    // background this frame — a tab switch, returning from a detail view, anything that
    // invalidates whatever a screen thinks is still on screen. A screen that skips its own
    // repaint when nothing it tracks has changed must still repaint when this is true, or it
    // leaves the canvas showing whatever the wipe left behind: nothing.
    virtual void render(Canvas &canvas, const Rect &area, bool fullRedraw) = 0;

    virtual void handle(Action action) = 0;

    // A screen may reserve Start for a field action or Back for closing an overlay.
    virtual bool usesStartAction() const { return false; }
    virtual bool hasBackOverlay() const { return false; }

    // Button legend for the bottom bar.
    virtual std::string hints() const = 0;

    // The selected game's path, if this screen can mark it as a favorite. App uses it
    // to keep a three-second hold tied to the same game from press to completion.
    virtual std::string favoritePath() const { return {}; }

    // The selected game's path when it sits in a list the user may remove it from by holding
    // B (Home: Recently played); empty otherwise.
    virtual std::string removablePath() const { return {}; }

    // The selected game, when the screen's cursor is on a game rather than a group/title.
    virtual bool selectedGame(const GameSystem *&system, Game &game) const {
        (void)system;
        (void)game;
        return false;
    }

    // True when the screen restores the background under whatever it redraws.
    // Screens that return false get their whole area reset before rendering.
    virtual bool incremental() const { return false; }
};
