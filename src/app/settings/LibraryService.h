#pragma once

// What the Library category needs from the host (Classic: Preferences, LibraryScan, MediaScraper, TheGamesDBClient,
// ScreenScraperService / ScreenScraperSettings, CoreMraUpdater, App::reloadLibrary). Owned and extended by the
// Library category agent: add what the rows and pages need, nothing else. Every method mirrors one Classic call;
// the comment names it. All calls are cheap and never block: a long task is a state machine that the host
// advances in update(). available() == false means the host has no implementation yet: the category then
// behaves as the plain first level does today (values in memory, nothing happens on A).

#include <cstdint>
#include <string>
#include <vector>

namespace patsgui {

// Library -> TheGamesDB (Classic: TheGamesDBClient::CheckState / checkStatus() / configured()).
struct TheGamesDbStatus {
    enum class State { NotChecked, Checking, Success, InvalidOrLimited, NetworkError, InvalidResponse };
    State state = State::NotChecked;
    bool configured = false;      // an API key is in the settings file
    std::string text;             // the row's value: checkStatus()
    std::string settingsPath;     // TheGamesDBClient::settingsPath(), named in the "add your key" notice
};

// Library -> ScreenScraper.fr -> Account (Classic: ScreenScraperService::Account).
struct ScreenScraperAccount {
    enum class State { Unknown, Checking, Ok, NoDeveloperLogin, NoUserLogin, Rejected, DeveloperRejected, Unavailable,
                       Blocked, Other };
    State state = State::Unknown;
    std::string detail;           // ScreenScraperService::Account::detail, unchanged (plain words for the settings page)
};

// The game-list scan (Classic: LibraryScan, driven by ScanScreen). A state machine the host steps.
struct ScanProgress {
    enum class State { Idle, Discovering, Scanning, Writing, Done, Failed };
    struct Item { std::string name, status; bool active = false, done = false; };
    State state = State::Idle;
    std::string statusLine;       // LibraryScan::statusLine()
    uint64_t systemsFound = 0, gamesFound = 0;
    float progress = 0.0f;        // LibraryScan::progress(), 0..1
    std::vector<Item> items;      // LibraryScan::progressItems() (only the first-run wizard shows them; Settings does not)
    std::string error;            // also the text of a cancelled scan: "Scan cancelled. The game list was not replaced."
};

// What the "Drives" step shows of a scan root: its size and free space (statvfs). Zero total: unknown.
struct DriveSpace {
    uint64_t totalBytes = 0, freeBytes = 0;
};

// The box-art job (Classic: MediaScraper, driven by ScanScreen's artwork pages).
struct ArtworkProgress {
    enum class State { Idle, Preparing, Working, Done, Failed };
    State state = State::Idle;
    std::string statusLine;       // MediaScraper::statusLine()
    uint64_t fetched = 0, prepared = 0, skipped = 0, missing = 0;
    float progress = 0.0f;        // MediaScraper::progress(), 0..1
    int64_t etaSeconds = -1;      // MediaScraper::estimatedRemainingSeconds(); -1 before the first game is done
    uint64_t totalGames = 0;      // Library::database().totalGames()
    std::string error;
    std::string missesFile;       // MediaScraper::kMissesFile, named on the Done page when games had no cover
};

// A system the cover search can be limited to (Classic: ScanScreen::refreshArtworkSystems()).
struct ArtworkSystem {
    std::string key, name;
    uint64_t games = 0;
};

// The Core and MRA updater (Classic: CoreMraUpdater). Snapshot mirrors CoreMraSnapshot.
struct CoreMraFile {
    enum class Type { ArcadeCore, ConsoleCore, OtherCore, ArcadeMra };
    enum class Status { UpToDate, Missing, UpdateAvailable, Modified, Error };
    std::string path, name;
    uint64_t size = 0;
    Type type = Type::ArcadeCore;
    Status status = Status::Missing;
    bool selected = false;
};
struct CoreMraProgress {
    enum class Phase { Idle, Checking, Ready, Installing, Complete, Failed };
    Phase phase = Phase::Idle;
    std::vector<CoreMraFile> entries;
    uint64_t completed = 0, totalFiles = 0, current = 0, currentBytes = 0, currentTotal = 0, installed = 0, failed = 0;
    std::string message, detail;
    std::vector<std::pair<std::string, CoreMraFile::Status>> recent;   // last files checked, oldest first
    int64_t checkedAt = 0;        // epoch seconds
    bool fromCache = false;
};

class LibraryService {
public:
    virtual ~LibraryService() = default;
    virtual bool available() const = 0;
    // The host calls this once per frame while the Settings tab is shown: step the scan / artwork job /
    // updater, refresh the account state. Cheap when idle.
    virtual void update(float deltaSeconds) = 0;

    // ---- Preferences (Classic: Preferences, SettingsScreen::buildRows)
    virtual bool organizeGameVersions() const = 0;                         // organizeGameVersions()
    virtual void setOrganizeGameVersions(bool on) = 0;                     // + Context::onGameVersionSettingChanged
    virtual bool screenScraperEnabled() const = 0;
    virtual void setScreenScraperEnabled(bool on) = 0;                     // on: forgetAccount + checkAccount; off: preview.stop()
    virtual int screenScraperVolume() const = 0;                           // percent, 0 = Off
    virtual void setScreenScraperVolume(int percent) = 0;
    virtual std::string screenScraperRegion() const = 0;                   // "eu" "us" "jp"
    virtual void setScreenScraperRegion(const std::string &region) = 0;
    virtual int screenScraperDelay() const = 0;                            // seconds
    virtual void setScreenScraperDelay(int seconds) = 0;
    virtual int screenScraperStorageLimitMb() const = 0;                   // 0 = Unlimited
    // The videos downloaded so far (Classic: ScreenScraperCache::videos()): one entry per file, sorted by title.
    // `id` names the file for deleteScrapedVideo(). `system` is the system's name, empty when not known.
    struct ScrapedVideo {
        std::string id, title, system;
        long long bytes = 0;
    };
    virtual std::vector<ScrapedVideo> scrapedVideos() = 0;
    virtual bool deleteScrapedVideo(const std::string &id) = 0;
    virtual size_t deleteAllScrapedVideos() = 0;
    virtual void setScreenScraperStorageLimitMb(int megabytes) = 0;

    // ---- Reload saved game list (Classic: setScanningAllowed(true); App::reloadLibrary(); setScanningAllowed(false))
    virtual void reloadSavedGameList() = 0;

    // ---- TheGamesDB
    virtual TheGamesDbStatus theGamesDb() const = 0;
    virtual bool startTheGamesDbCheck() = 0;                               // startApiKeyCheck()

    // ---- ScreenScraper account
    virtual ScreenScraperAccount account() const = 0;
    virtual void checkAccount() = 0;                                       // forgetAccount() + checkAccount()
    virtual std::string screenScraperUser() const = 0;                     // ScreenScraperSettings::load().userId
    virtual bool screenScraperPasswordSet() const = 0;
    // ScreenScraperSettings::save("SCREENSCRAPER_SSID" | "SCREENSCRAPER_SSPASSWORD", value); on success the host
    // also forgets the account and re-checks it when video previews are on. False: the file could not be written.
    virtual bool saveScreenScraperLogin(bool user, const std::string &value) = 0;

    // ---- Build game list / Prepare box art (Classic: ScanScreen pages Intro, Working, ArtworkOffer, ...)
    virtual std::vector<std::string> scanRoots() const = 0;                // LibraryScan::detectRoots()
    virtual DriveSpace driveSpace(const std::string &root) const = 0;      // statvfs(root); {0, 0} when it cannot be read
    virtual void startScan(const std::vector<std::string> &roots) = 0;
    virtual void cancelScan() = 0;
    virtual ScanProgress scan() const = 0;
    // The systems with games and a cover set online, sorted by name (case-insensitive); and the game count of the
    // whole library ("All systems"). Classic reads them once when the artwork page opens.
    virtual std::vector<ArtworkSystem> artworkSystems() const = 0;
    virtual uint64_t libraryGameCount() const = 0;
    // Classic App::closeScan(): the scan / box-art pages ended; reloads the library when the box-art job of this
    // visit fetched or prepared anything (new covers must reach the image cache).
    virtual void libraryTaskClosed() = 0;
    // The system keys to prepare; empty = all systems (MediaScraper::start takes the list).
    virtual void startArtwork(const std::vector<std::string> &systemKeys) = 0;
    virtual void cancelArtwork() = 0;
    virtual ArtworkProgress artwork() const = 0;

    // ---- First-run Jump Start (Classic: ScanScreen::handle() page JumpStart, update() after the scan; src/app/FirstRun.cpp)
    // The quick scan of up to 20 games per system (and 20 working Arcade games) on every detected drive:
    // LibraryScan::startJumpStart(detectRoots(), 20, 20). Progress and end are scan(), like startScan().
    virtual void startJumpStart() = 0;
    // The cover step after it: MediaScraper::start with boxart only (no backgrounds), all systems. Progress is artwork().
    virtual void startJumpStartArtwork() = 0;

    // ---- Core and MRA updates (Classic: CoreMraUpdater, internetRouteAvailable())
    virtual bool internetRouteAvailable() const = 0;
    virtual bool startCoreCheck(bool fresh) = 0;
    virtual void cancelCoreCheck() = 0;
    virtual bool startCoreInstall() = 0;
    virtual void selectAllMissingOrOutdated() = 0;
    virtual bool toggleCoreSelected(size_t index) = 0;
    // The latest snapshot. The host refreshes it in update() (about 10 Hz while checking or installing, at once after
    // every call above): the real snapshot copies up to 3000 entries, so it is never taken per call.
    virtual const CoreMraProgress &cores() const = 0;
};

}  // namespace patsgui
