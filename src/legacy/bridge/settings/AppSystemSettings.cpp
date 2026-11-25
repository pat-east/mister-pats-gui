#include "bridge/settings/AppSystemSettings.h"

#include "BackupTask.h"
#include "DiscCoreInstaller.h"
#include "DiscCoreManager.h"
#include "DiscCorePackage.h"
#include "DiscDriveService.h"
#include "GameDatabase.h"
#include "ImageCache.h"

namespace {

using patsgui::DiscDriveInfo;
using patsgui::DiscInstallInfo;

DiscDriveInfo::Media mediaOf(DiscDriveService::MediaState state) {   // by name: the enums are separate types
    switch (state) {
        case DiscDriveService::MediaState::NoDrive: return DiscDriveInfo::Media::NoDrive;
        case DiscDriveService::MediaState::NoDisc: return DiscDriveInfo::Media::NoDisc;
        case DiscDriveService::MediaState::Checking: return DiscDriveInfo::Media::Checking;
        case DiscDriveService::MediaState::Readable: return DiscDriveInfo::Media::Readable;
        case DiscDriveService::MediaState::Unsupported: return DiscDriveInfo::Media::Unsupported;
        case DiscDriveService::MediaState::Unreadable: return DiscDriveInfo::Media::Unreadable;
    }
    return DiscDriveInfo::Media::NoDrive;
}

DiscInstallInfo::State installStateOf(DiscCoreInstaller::State state) {
    switch (state) {
        case DiscCoreInstaller::State::Idle: return DiscInstallInfo::State::Idle;
        case DiscCoreInstaller::State::Downloading: return DiscInstallInfo::State::Downloading;
        case DiscCoreInstaller::State::Verifying: return DiscInstallInfo::State::Verifying;
        case DiscCoreInstaller::State::Installing: return DiscInstallInfo::State::Installing;
        case DiscCoreInstaller::State::Ready: return DiscInstallInfo::State::Ready;
        case DiscCoreInstaller::State::Failed: return DiscInstallInfo::State::Failed;
        case DiscCoreInstaller::State::Cancelled: return DiscInstallInfo::State::Cancelled;
    }
    return DiscInstallInfo::State::Idle;
}

}  // namespace

AppSystemSettings::AppSystemSettings(const AdapterEnv &env) : env_(env) { wifi_.refreshInterface(); }

// SettingsHost entries that may be missing (a host without them): the service is then not available and the category
// behaves as the plain first level.
bool AppSystemSettings::available() const {
    const SettingsHost &h = env_.context.settingsHost;
    return h.backup && h.startMisterCore && h.rebootMister && h.startBackup && h.backupUnreported && h.acknowledgeBackup &&
           h.startAssetDownload && h.assetDownloadStatus && h.assetDownloadRunning && h.assetDownloadFailed &&
           h.assetDownloadError;
}

// Classic: SettingsScreen::update(): system_.update(dt), wifi_.update(dt), wifi_.refreshInterface().
void AppSystemSettings::update(float deltaSeconds) {
    info_.update(deltaSeconds);
    wifi_.update(deltaSeconds);
    wifi_.refreshInterface();
}

patsgui::DiscDriveInfo AppSystemSettings::disc() const {
    const auto drive = env_.context.discDrive.snapshot();
    const auto cores = env_.context.discCores.snapshot();
    DiscDriveInfo out;
    out.media = mediaOf(drive.state);
    out.device = drive.device;
    out.model = drive.model;
    out.stateName = DiscDriveService::stateName(drive.state);
    out.alternateMainInstalled = cores.alternateMainInstalled;
    out.alternateMainVerified = cores.alternateMainVerified;
    out.scopedMainConfigured = cores.scopedMainConfigured;
    out.psxMglInstalled = cores.psxMglInstalled;
    out.psxMglCompatible = cores.psxMglCompatible;
    out.psxMglVerified = cores.psxMglVerified;
    out.psxCoreInstalled = cores.psxCoreInstalled;
    out.saturnMglInstalled = cores.saturnMglInstalled;
    out.saturnMglCompatible = cores.saturnMglCompatible;
    out.saturnMglVerified = cores.saturnMglVerified;
    out.saturnCoreInstalled = cores.saturnCoreInstalled;
    out.packageReady = cores.packageReady();
    out.packageVersion = DiscCorePackage::kVersion;
    out.projectUrl = DiscCoreManager::projectUrl();
    return out;
}

patsgui::DiscInstallInfo AppSystemSettings::discInstall() const {
    const auto snap = env_.context.discInstaller.snapshot();
    DiscInstallInfo out;
    out.state = installStateOf(snap.state);
    out.bytes = snap.bytes;
    out.message = snap.message;
    out.recoveryBlocked = env_.context.discInstaller.recoveryBlocked();
    out.busy = env_.context.discInstaller.busy();
    return out;
}

bool AppSystemSettings::startDiscInstall() { return env_.context.discInstaller.start(); }
void AppSystemSettings::cancelDiscInstall() { env_.context.discInstaller.cancel(); }

// SettingsScreen::facts() (SettingsScreen.cpp 921-999), field for field.
std::vector<patsgui::SystemFact> AppSystemSettings::facts() const {
    Context &context = env_.context;
    std::vector<patsgui::SystemFact> out;
    char buffer[128];

    out.push_back({"Machine", ""});
    std::snprintf(buffer, sizeof(buffer), "%s, %d cores", info_.cpuModel().c_str(), info_.cores());
    out.push_back({"Processor", buffer});
    if (info_.cpuBusy() >= 0.0f) {
        std::snprintf(buffer, sizeof(buffer), "%.0f%%  \xC2\xB7  load %.2f", info_.cpuBusy() * 100.0f, info_.loadAverage());
        out.push_back({"In use", buffer});
    }
    if (info_.memoryTotalBytes()) {
        std::snprintf(buffer, sizeof(buffer), "%s of %s", SystemInfo::formatBytes(info_.memoryUsedBytes()).c_str(),
                      SystemInfo::formatBytes(info_.memoryTotalBytes()).c_str());
        out.push_back({"Memory", buffer});
    }
    out.push_back({"Running for", SystemInfo::formatDuration(info_.uptimeSeconds())});

    out.push_back({"", ""});
    out.push_back({"Networking", ""});
    out.push_back({"Ethernet", info_.ethernetAddress()});
    out.push_back({"Wi-Fi", info_.wifiAddress()});
    out.push_back({"Wi-Fi network", info_.wifiNetwork()});

    out.push_back({"", ""});
    out.push_back({"Storage", ""});
    for (const SystemInfo::Volume &volume : info_.volumes()) {
        std::snprintf(buffer, sizeof(buffer), "%s of %s free", SystemInfo::formatBytes(volume.freeBytes).c_str(),
                      SystemInfo::formatBytes(volume.totalBytes).c_str());
        out.push_back({volume.path, buffer});
    }

    out.push_back({"", ""});
    out.push_back({"Library", ""});
    if (context.library.usingDatabase()) {
        const GameDatabase &db = context.library.database();
        std::snprintf(buffer, sizeof(buffer), "%zu games in %zu systems", db.totalGames(), db.systems().size());
        out.push_back({"Database", buffer});
        for (const std::string &missing : db.missingRoots()) out.push_back({"Drive missing", missing});
    } else {
        out.push_back({"Database", "not built"});
    }
    out.push_back({"Systems listed", std::to_string(context.library.systems().size())});
    out.push_back({"Favorites", std::to_string(context.favorites.size())});
    const size_t indexed = context.library.index().size();
    if (indexed) out.push_back({"Imported game list", std::to_string(indexed) + " games"});

    out.push_back({"", ""});
    out.push_back({"Display", ""});
    const std::string resolution = env_.context.settingsHost.framebufferInfo ? env_.context.settingsHost.framebufferInfo()
                                                                              : std::string();
    out.push_back({"Resolution", resolution.empty() ? "-" : resolution});   // an empty value would read as a heading
    Font &font = context.theme.bold();
    if (!font.usingTrueType()) {
        out.push_back({"Font", "built-in typeface"});
    } else {
        const std::string &path = font.path();
        const size_t slash = path.find_last_of('/');
        out.push_back({"Font", slash == std::string::npos ? path : path.substr(slash + 1)});
    }
    out.push_back({"Images cached", std::to_string(context.images.size())});
    return out;
}

bool AppSystemSettings::use24HourClock() const { return env_.context.preferences.use24HourClock(); }
void AppSystemSettings::setUse24HourClock(bool on) {
    env_.context.preferences.setUse24HourClock(on);
    redrawShell();   // the shell's clock follows the preference
}
std::string AppSystemSettings::timeZone() const { return env_.context.preferences.timeZone(); }
bool AppSystemSettings::setTimeZone(const std::string &zone) {
    const bool ok = env_.context.preferences.setTimeZone(zone);
    if (ok) redrawShell();
    return ok;
}
std::vector<std::string> AppSystemSettings::timeZoneRegions() const { return Preferences::timeZoneRegions(); }
std::vector<std::string> AppSystemSettings::timeZonesIn(const std::string &region) const { return Preferences::timeZonesIn(region); }
int AppSystemSettings::screensaverDelay() const { return env_.context.preferences.screensaverDelay(); }
void AppSystemSettings::setScreensaverDelay(int seconds) { env_.context.preferences.setScreensaverDelay(seconds); }
std::string AppSystemSettings::screensaverStyle() const { return env_.context.preferences.screensaverStyle(); }
void AppSystemSettings::setScreensaverStyle(const std::string &style) { env_.context.preferences.setScreensaverStyle(style); }

// The App lambda of Classic's onBackupSettings, without its notify (the page toasts).
bool AppSystemSettings::startBackup() { return env_.context.settingsHost.startBackup(); }

patsgui::BackupInfo AppSystemSettings::backup() const {
    patsgui::BackupInfo out;
    if (!env_.context.settingsHost.backup) return out;
    const BackupTask::Snapshot snap = env_.context.settingsHost.backup->snapshot();
    switch (snap.state) {
        case BackupTask::State::Idle: out.state = patsgui::BackupInfo::State::Idle; break;
        case BackupTask::State::Running: out.state = patsgui::BackupInfo::State::Running; break;
        case BackupTask::State::Complete: out.state = patsgui::BackupInfo::State::Complete; break;
        case BackupTask::State::Failed: out.state = patsgui::BackupInfo::State::Failed; break;
    }
    out.path = snap.path;
    out.unreported = env_.context.settingsHost.backupUnreported();
    return out;
}

void AppSystemSettings::acknowledgeBackup() { env_.context.settingsHost.acknowledgeBackup(); }

patsgui::IconDownloadInfo AppSystemSettings::icons() const {
    const SettingsHost &h = env_.context.settingsHost;
    patsgui::IconDownloadInfo out;
    out.statusText = h.assetDownloadStatus();   // App::assetDownloadStatus()
    out.running = h.assetDownloadRunning();
    out.failed = h.assetDownloadFailed();
    out.error = out.failed ? h.assetDownloadError() : std::string();
    out.missing = h.assetsMissing ? h.assetsMissing() : -1;
    return out;
}

// App::startAssetDownload() decides between four outcomes and notifies each; its notices go to the Classic bottom bar.
// The outcome is read back from the state it leaves.
patsgui::IconStart AppSystemSettings::startIconDownload() {
    const SettingsHost &h = env_.context.settingsHost;
    patsgui::IconStart out;
    if (h.assetDownloadRunning()) {
        h.startAssetDownload();   // Classic: notifies "An icon download is already running."
        out.kind = patsgui::IconStart::Kind::AlreadyRunning;
        return out;
    }
    h.startAssetDownload();
    if (h.assetDownloadRunning()) out.kind = patsgui::IconStart::Kind::Started;
    else if (h.assetDownloadFailed()) {
        out.kind = patsgui::IconStart::Kind::Failed;
        out.error = h.assetDownloadError();
    } else out.kind = patsgui::IconStart::Kind::AllReady;
    return out;
}

bool AppSystemSettings::wifiAvailable() const { return wifi_.available(); }
bool AppSystemSettings::wifiStartScan(std::string &error) { return wifi_.startScan(error); }
bool AppSystemSettings::wifiScanComplete() const { return wifi_.scanComplete(); }

std::vector<patsgui::WifiNetwork> AppSystemSettings::wifiScanResults(std::string &error) {
    std::vector<patsgui::WifiNetwork> out;
    for (const Wifi::Network &n : wifi_.scanResults(error)) {
        patsgui::WifiNetwork net;
        net.ssid = n.ssid;
        net.security = n.security;
        net.signalDbm = n.signalDbm;
        net.open = n.open;
        net.supported = n.supported;
        out.push_back(net);
    }
    return out;
}

bool AppSystemSettings::wifiBeginTest(const std::string &ssid, const std::string &password, bool open, bool hidden,
                                      std::string &error) {
    return wifi_.beginTest(ssid, password, open, hidden, error);
}

patsgui::WifiTest AppSystemSettings::wifiPollTest(float deltaSeconds) {
    patsgui::WifiTest out;
    const Wifi::TestResult r = wifi_.pollTest(deltaSeconds, out.status);
    out.result = r == Wifi::TestResult::Connected ? patsgui::WifiTest::Result::Connected
                 : r == Wifi::TestResult::Failed ? patsgui::WifiTest::Result::Failed : patsgui::WifiTest::Result::Waiting;
    return out;
}

bool AppSystemSettings::wifiTesting() const { return wifi_.testing(); }
bool AppSystemSettings::wifiNeedsCountryCode() const { return wifi_.needsCountryCode(); }
bool AppSystemSettings::wifiSetCountryCode(const std::string &country, std::string &error) { return wifi_.setCountryCode(country, error); }
bool AppSystemSettings::wifiSaveTestedNetwork(std::string &error) { return wifi_.saveTestedNetwork(error); }
void AppSystemSettings::wifiRollback() { wifi_.rollback(); }

void AppSystemSettings::returnToMisterMenu() { env_.context.settingsHost.startMisterCore(); }

bool AppSystemSettings::reboot(std::string &error) { return env_.context.settingsHost.rebootMister(error); }
