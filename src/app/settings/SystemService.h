#pragma once

// What the System category needs from the host (Classic: DiscDriveService, DiscCoreManager, DiscCoreInstaller,
// DiscCorePackage, Preferences (clock, time zone, screensaver), BackupTask, SystemInfo, Wifi, Icons/TabIcons
// downloads, App::rebootMister(), App::startMisterCore()). Owned and extended by the System agent.
// The changelog row uses UpdatesService.
//
// Every method is a thin wrapper of the Classic call named beside it; texts that Classic computes in the
// service layer (installer messages, Wi-Fi status lines, errors, the icon status) come back as data strings,
// exactly as Classic shows them.

#include <cstdint>
#include <string>
#include <vector>

namespace patsgui {

// System -> Disc drive (Classic: DiscDriveService::Snapshot, DiscCoreManager::Status).
struct DiscDriveInfo {
    enum class Media { NoDrive, NoDisc, Checking, Readable, Unsupported, Unreadable };   // DiscDriveService::MediaState
    Media media = Media::NoDrive;
    std::string device, model, stateName;     // stateName: DiscDriveService::stateName(state)
    bool alternateMainInstalled = false, alternateMainVerified = false, scopedMainConfigured = false;
    bool psxMglInstalled = false, psxMglCompatible = false, psxMglVerified = false, psxCoreInstalled = false;
    bool saturnMglInstalled = false, saturnMglCompatible = false, saturnMglVerified = false, saturnCoreInstalled = false;
    bool packageReady = false;                // DiscCoreManager::Status::packageReady()
    std::string packageVersion, projectUrl;   // DiscCorePackage::kVersion, DiscCoreManager::projectUrl()
};

// System -> Install/update Disc cores (Classic: DiscCoreInstaller::Snapshot, busy(), recoveryBlocked()).
struct DiscInstallInfo {
    enum class State { Idle, Downloading, Verifying, Installing, Ready, Failed, Cancelled };   // DiscCoreInstaller::State
    State state = State::Idle;
    uint64_t bytes = 0;
    std::string message;                      // the installer's own message (it is never cleared after a run)
    bool recoveryBlocked = false;             // DiscCoreInstaller::recoveryBlocked()
    bool busy = false;                        // DiscCoreInstaller::busy(): Downloading, Verifying or Installing
};

// System -> Info (Classic: SettingsScreen::facts()). Groups of label/value lines: a label with an empty value is a
// group heading, an empty label and value is a spacer.
struct SystemFact { std::string label, value; };

// System -> Connect to Wi-Fi (Classic: Wifi).
struct WifiNetwork { std::string ssid, security; int signalDbm = 0; bool open = false, supported = false; };
struct WifiTest {                              // Classic: Wifi::pollTest(dt, status)
    enum class Result { Waiting, Connected, Failed };
    Result result = Result::Waiting;
    std::string status;                        // Wifi's own line: "Joining X…", "Connected to X — IP 1.2.3.4", ...
                                               // Empty = unchanged since the last poll (Classic keeps its string between polls).
};

// System -> Back up GUI settings (Classic: BackupTask::Snapshot).
struct BackupInfo {
    enum class State { Idle, Running, Complete, Failed };
    State state = State::Idle;
    std::string path;                          // the archive, when Complete
    bool unreported = false;                   // finished (Complete or Failed) and App has not reported it yet (state != backupStateReported_)
};

// System -> Download missing icons (Classic: App::startAssetDownload(), assetDownloadStatus()).
struct IconDownloadInfo {
    std::string statusText;                    // App::assetDownloadStatus(): "Downloading icons…", "Try download again", "<n> icons missing", "All icons ready"
    bool running = false;                      // App::assetsDownloadActive_
    bool failed = false;                       // the system or tab download state is Failed
    std::string error;                         // system icon error, else the tab icon error ("" when none)
    int missing = -1;                          // icons missing (Icons::missingBitmaps + TabIcons::missingFiles); -1: not known. Reads the folders: ask once, not every frame
};
struct IconStart {                             // what App::startAssetDownload() decided (its notify texts)
    enum class Kind { Started, AlreadyRunning, AllReady, Failed };
    Kind kind = Kind::Failed;
    std::string error;                         // Failed: the error Classic notifies (8 s)
};

class SystemService {
public:
    virtual ~SystemService() = default;
    virtual bool available() const = 0;
    // Once per frame while the Settings tab is shown: SystemInfo::update, Wifi::update/refreshInterface, the
    // disc snapshots. Cheap when idle (Classic: SettingsScreen::update()).
    virtual void update(float deltaSeconds) = 0;

    // ---- Disc
    virtual DiscDriveInfo disc() const = 0;
    virtual DiscInstallInfo discInstall() const = 0;
    virtual bool startDiscInstall() = 0;                      // DiscCoreInstaller::start(); false: refused, message in discInstall()
    virtual void cancelDiscInstall() = 0;                     // DiscCoreInstaller::cancel()

    // ---- Info
    virtual std::vector<SystemFact> facts() const = 0;

    // ---- Clock and screensaver (Classic: Preferences)
    virtual bool use24HourClock() const = 0;
    virtual void setUse24HourClock(bool on) = 0;
    virtual std::string timeZone() const = 0;                 // "" follows MiSTer's system time zone
    virtual bool setTimeZone(const std::string &zone) = 0;    // "Region/City"; false: could not be saved
    virtual std::vector<std::string> timeZoneRegions() const = 0;          // Preferences::timeZoneRegions()
    virtual std::vector<std::string> timeZonesIn(const std::string &region) const = 0;
    virtual int screensaverDelay() const = 0;                 // seconds: 0 30 300 600 1200 1800 3600
    virtual void setScreensaverDelay(int seconds) = 0;
    virtual std::string screensaverStyle() const = 0;         // "dimmed" "boxart" "boxart-collage"
    virtual void setScreensaverStyle(const std::string &style) = 0;

    // ---- Backup (Classic: BackupTask)
    virtual bool startBackup() = 0;                           // false: already running (Classic is silent then)
    virtual BackupInfo backup() const = 0;
    virtual void acknowledgeBackup() = 0;                     // the result was shown: App must not report it again

    // ---- Icons (Classic: App::startAssetDownload() / assetDownloadStatus())
    virtual IconDownloadInfo icons() const = 0;
    virtual IconStart startIconDownload() = 0;

    // ---- Wi-Fi
    virtual bool wifiAvailable() const = 0;                   // Wifi::available() after refreshInterface()
    virtual bool wifiStartScan(std::string &error) = 0;
    virtual bool wifiScanComplete() const = 0;
    virtual std::vector<WifiNetwork> wifiScanResults(std::string &error) = 0;
    virtual bool wifiBeginTest(const std::string &ssid, const std::string &password, bool open, bool hidden,
                               std::string &error) = 0;
    virtual WifiTest wifiPollTest(float deltaSeconds) = 0;
    virtual bool wifiTesting() const = 0;
    virtual bool wifiNeedsCountryCode() const = 0;
    virtual bool wifiSetCountryCode(const std::string &country, std::string &error) = 0;
    virtual bool wifiSaveTestedNetwork(std::string &error) = 0;
    virtual void wifiRollback() = 0;

    // ---- Leaving (Classic: App::startMisterCore(), App::rebootMister())
    virtual void returnToMisterMenu() = 0;                    // leaves the GUI
    virtual bool reboot(std::string &error) = 0;              // sync + reboot; false with the reason
};

}  // namespace patsgui
