#include "bridge/settings/AppLibrarySettings.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <ifaddrs.h>
#include <net/if.h>
#include <strings.h>
#include <sys/statvfs.h>

#include "LibraryScan.h"
#include "LibretroIndex.h"
#include "MediaScraper.h"
#include "ScreenScraperCache.h"
#include "SystemCatalog.h"

namespace {

using patsgui::ArtworkProgress;
using patsgui::CoreMraFile;
using patsgui::CoreMraProgress;
using patsgui::ScanProgress;
using patsgui::ScreenScraperAccount;
using patsgui::TheGamesDbStatus;

// Whether this MiSTer can plausibly reach the internet: an interface with an IPv4 address and a default route. It
// sends no traffic and touches no storage (/proc is kernel memory). A copy of internetRouteAvailable() in
// src/legacy/SettingsScreen.cpp:36-59, which is private to that file.
bool routeAvailable() {
    bool address = false;
    ifaddrs *list = nullptr;
    if (getifaddrs(&list) == 0) {
        for (ifaddrs *it = list; it && !address; it = it->ifa_next) {
            address = it->ifa_addr && it->ifa_addr->sa_family == AF_INET &&
                      !(it->ifa_flags & IFF_LOOPBACK) && (it->ifa_flags & IFF_UP);
        }
        freeifaddrs(list);
    }
    if (!address) return false;

    std::ifstream routes("/proc/net/route");
    std::string line;
    std::getline(routes, line);   // header
    while (std::getline(routes, line)) {
        char name[32] = {};
        unsigned destination = 1, flags = 0;
        if (std::sscanf(line.c_str(), "%31s %x %*x %x", name, &destination, &flags) == 3 &&
            destination == 0 && (flags & 0x1))   // RTF_UP
            return true;
    }
    return false;
}

ScreenScraperAccount::State accountStateOf(ScreenScraperService::AccountState s) {   // by name: separate enums
    switch (s) {
        case ScreenScraperService::AccountState::Unknown: return ScreenScraperAccount::State::Unknown;
        case ScreenScraperService::AccountState::Checking: return ScreenScraperAccount::State::Checking;
        case ScreenScraperService::AccountState::Ok: return ScreenScraperAccount::State::Ok;
        case ScreenScraperService::AccountState::NoDeveloperLogin: return ScreenScraperAccount::State::NoDeveloperLogin;
        case ScreenScraperService::AccountState::NoUserLogin: return ScreenScraperAccount::State::NoUserLogin;
        case ScreenScraperService::AccountState::Rejected: return ScreenScraperAccount::State::Rejected;
        case ScreenScraperService::AccountState::DeveloperRejected: return ScreenScraperAccount::State::DeveloperRejected;
        case ScreenScraperService::AccountState::Unavailable: return ScreenScraperAccount::State::Unavailable;
        case ScreenScraperService::AccountState::Blocked: return ScreenScraperAccount::State::Blocked;
    }
    return ScreenScraperAccount::State::Other;
}

TheGamesDbStatus::State tgdbStateOf(TheGamesDBClient::CheckState s) {
    switch (s) {
        case TheGamesDBClient::CheckState::NotChecked: return TheGamesDbStatus::State::NotChecked;
        case TheGamesDBClient::CheckState::Checking: return TheGamesDbStatus::State::Checking;
        case TheGamesDBClient::CheckState::Success: return TheGamesDbStatus::State::Success;
        case TheGamesDBClient::CheckState::InvalidOrLimited: return TheGamesDbStatus::State::InvalidOrLimited;
        case TheGamesDBClient::CheckState::NetworkError: return TheGamesDbStatus::State::NetworkError;
        case TheGamesDBClient::CheckState::InvalidResponse: return TheGamesDbStatus::State::InvalidResponse;
    }
    return TheGamesDbStatus::State::NotChecked;
}

CoreMraFile::Type typeOf(CoreMraType t) {
    switch (t) {
        case CoreMraType::ArcadeCore: return CoreMraFile::Type::ArcadeCore;
        case CoreMraType::ConsoleCore: return CoreMraFile::Type::ConsoleCore;
        case CoreMraType::OtherCore: return CoreMraFile::Type::OtherCore;
        case CoreMraType::ArcadeMra: return CoreMraFile::Type::ArcadeMra;
    }
    return CoreMraFile::Type::OtherCore;
}

CoreMraFile::Status statusOf(CoreMraStatus s) {
    switch (s) {
        case CoreMraStatus::UpToDate: return CoreMraFile::Status::UpToDate;
        case CoreMraStatus::Missing: return CoreMraFile::Status::Missing;
        case CoreMraStatus::UpdateAvailable: return CoreMraFile::Status::UpdateAvailable;
        case CoreMraStatus::Modified: return CoreMraFile::Status::Modified;
        case CoreMraStatus::Error: return CoreMraFile::Status::Error;
    }
    return CoreMraFile::Status::Error;
}

CoreMraProgress::Phase phaseOf(CoreMraPhase p) {
    switch (p) {
        case CoreMraPhase::Idle: return CoreMraProgress::Phase::Idle;
        case CoreMraPhase::Checking: return CoreMraProgress::Phase::Checking;
        case CoreMraPhase::Ready: return CoreMraProgress::Phase::Ready;
        case CoreMraPhase::Installing: return CoreMraProgress::Phase::Installing;
        case CoreMraPhase::Complete: return CoreMraProgress::Phase::Complete;
        case CoreMraPhase::Failed: return CoreMraProgress::Phase::Failed;
    }
    return CoreMraProgress::Phase::Idle;
}

}  // namespace

AppLibrarySettings::AppLibrarySettings(const AdapterEnv &env) : env_(env) {
    refreshLogin();   // SettingsScreen::buildRows(): refreshScreenScraperLogin() once
    refreshCores();
    refreshTheGamesDb();
    previousTgdb_ = env_.context.theGamesDB.checkState();
}

// A host without the App hooks cannot run the scan, the box-art job or the reload: the category is then the plain first level.
bool AppLibrarySettings::available() const {
    const SettingsHost &h = env_.context.settingsHost;
    return h.scan && h.scraper && h.reloadLibrary;
}

// What SettingsScreen::update() does for this category every frame (SettingsScreen.cpp:1002-1012) plus what
// ScanScreen::update() does when a scan ends (ScanScreen.cpp:145-149). It never steps the scan or the box-art job: App's loop
// does that once per frame (App.cpp, "One system per frame while scanning, one game per frame while fetching artwork").
void AppLibrarySettings::update(float deltaSeconds) {
    const SettingsHost &h = env_.context.settingsHost;
    if (scanWatched_ && h.scan && h.scan->finished()) {
        scanWatched_ = false;
        // onFinished_ = App::reloadLibrary(), only for a scan that ended well (a cancelled one keeps the old list).
        if (h.scan->state() == LibraryScan::State::Done && h.reloadLibrary) h.reloadLibrary();
    }
    const TheGamesDBClient::CheckState gamesDb = env_.context.theGamesDB.checkState();
    if (previousTgdb_ == TheGamesDBClient::CheckState::Checking && gamesDb == TheGamesDBClient::CheckState::Success)
        env_.context.discArtwork.refresh(env_.context.discDrive.snapshot());
    previousTgdb_ = gamesDb;
    refreshTheGamesDb();

    // The updater snapshot copies up to 3000 entries: refreshed about 10 times a second while a check or an install runs,
    // and at once after every call that changes it (below).
    coresClock_ += deltaSeconds;
    const bool busy = coresCache_.phase == CoreMraProgress::Phase::Checking || coresCache_.phase == CoreMraProgress::Phase::Installing;
    if (coresDirty_ || (busy && coresClock_ >= 0.1f)) refreshCores();
}

void AppLibrarySettings::refreshCores() {
    const CoreMraSnapshot s = coreUpdater_.snapshot();
    coresCache_ = CoreMraProgress();
    coresCache_.phase = phaseOf(s.phase);
    coresCache_.entries.reserve(s.entries.size());
    for (const CoreMraEntry &e : s.entries) {
        CoreMraFile f;
        f.path = e.path;
        f.name = e.name;
        f.size = e.size;
        f.type = typeOf(e.type);
        f.status = statusOf(e.status);
        f.selected = e.selected;
        coresCache_.entries.push_back(f);
    }
    coresCache_.completed = s.completed;
    coresCache_.totalFiles = s.totalFiles;
    coresCache_.current = s.current;
    coresCache_.currentBytes = s.currentBytes;
    coresCache_.currentTotal = s.currentTotal;
    coresCache_.installed = s.installed;
    coresCache_.failed = s.failed;
    coresCache_.message = s.message;
    coresCache_.detail = s.detail;
    for (const CoreMraRecent &r : s.recent) coresCache_.recent.push_back({r.name, statusOf(r.status)});
    coresCache_.checkedAt = s.checkedAt;
    coresCache_.fromCache = s.fromCache;
    coresClock_ = 0.0f;
    coresDirty_ = false;
}

void AppLibrarySettings::refreshTheGamesDb() {
    TheGamesDBClient &client = env_.context.theGamesDB;
    tgdbCache_.state = tgdbStateOf(client.checkState());
    tgdbCache_.configured = client.configured();
    tgdbCache_.text = client.checkStatus();   // the row's value (SettingsScreen.cpp:316)
    tgdbCache_.settingsPath = TheGamesDBClient::settingsPath();
}

void AppLibrarySettings::refreshLogin() {
    const ScreenScraperCredentials login = ScreenScraperSettings::load();
    user_ = login.userId;
    passwordSet_ = !login.userPassword.empty();
}

// ---- Preferences (Classic: the same Preferences setters, SettingsScreen.cpp:310, 664, 704, 713, 720, 734)
bool AppLibrarySettings::organizeGameVersions() const { return env_.context.preferences.organizeGameVersions(); }

void AppLibrarySettings::setOrganizeGameVersions(bool on) {
    env_.context.preferences.setOrganizeGameVersions(on);
    if (env_.context.onGameVersionSettingChanged) env_.context.onGameVersionSettingChanged();
    ++env_.context.libraryRevision;   // AppLibraryAdapter reads organize() live: this makes it drop what it cached for the old grouping
}

bool AppLibrarySettings::screenScraperEnabled() const { return env_.context.preferences.screenScraperEnabled(); }

void AppLibrarySettings::setScreenScraperEnabled(bool on) {
    env_.context.preferences.setScreenScraperEnabled(on);
    if (on) {
        env_.context.preview.service().forgetAccount();
        env_.context.preview.service().checkAccount();
    } else {
        env_.context.preview.stop();
    }
}

int AppLibrarySettings::screenScraperVolume() const { return env_.context.preferences.screenScraperVolume(); }
void AppLibrarySettings::setScreenScraperVolume(int percent) { env_.context.preferences.setScreenScraperVolume(percent); }
std::string AppLibrarySettings::screenScraperRegion() const { return env_.context.preferences.screenScraperRegion(); }
void AppLibrarySettings::setScreenScraperRegion(const std::string &region) { env_.context.preferences.setScreenScraperRegion(region); }
int AppLibrarySettings::screenScraperDelay() const { return env_.context.preferences.screenScraperDelay(); }
void AppLibrarySettings::setScreenScraperDelay(int seconds) { env_.context.preferences.setScreenScraperDelay(seconds); }
int AppLibrarySettings::screenScraperStorageLimitMb() const { return env_.context.preferences.screenScraperStorageLimitMb(); }
void AppLibrarySettings::setScreenScraperStorageLimitMb(int megabytes) {
    env_.context.preferences.setScreenScraperStorageLimitMb(megabytes);
}

// ScreenScraper.fr videos: the cache's finished files, named for the list by title and system.
std::vector<patsgui::LibraryService::ScrapedVideo> AppLibrarySettings::scrapedVideos() {
    std::vector<ScrapedVideo> out;
    for (const ScreenScraperCache::VideoInfo &v : env_.context.preview.service().cache().videos()) {
        ScrapedVideo video;
        video.id = std::to_string(v.gameId) + "-" + v.type;
        video.title = v.title.empty() ? "Video " + std::to_string(v.gameId) : v.title;   // no stored result names it
        video.system = v.systemKey.empty() ? std::string() : SystemCatalog::displayName(v.systemKey);
        video.bytes = v.bytes;
        out.push_back(video);
    }
    return out;
}

bool AppLibrarySettings::deleteScrapedVideo(const std::string &id) {
    const size_t dash = id.find('-');
    if (dash == std::string::npos || dash == 0) return false;
    return env_.context.preview.service().cache().deleteVideo(std::atoll(id.substr(0, dash).c_str()), id.substr(dash + 1));
}

size_t AppLibrarySettings::deleteAllScrapedVideos() { return env_.context.preview.service().cache().deleteAllVideos(); }

// Reload saved game list (SettingsScreen.cpp:296-298): the only path that walks the game volume, and only on request.
void AppLibrarySettings::reloadSavedGameList() {
    env_.context.library.setScanningAllowed(true);
    if (env_.context.settingsHost.reloadLibrary) env_.context.settingsHost.reloadLibrary();   // App::reloadLibrary()
    env_.context.library.setScanningAllowed(false);
}

// ---- TheGamesDB
patsgui::TheGamesDbStatus AppLibrarySettings::theGamesDb() const { return tgdbCache_; }

bool AppLibrarySettings::startTheGamesDbCheck() {
    const bool started = env_.context.theGamesDB.startApiKeyCheck();
    refreshTheGamesDb();   // the row shows "Checking API key…" at once
    return started;
}

// ---- ScreenScraper account
patsgui::ScreenScraperAccount AppLibrarySettings::account() const {
    const ScreenScraperService::Account a = env_.context.preview.service().account();
    patsgui::ScreenScraperAccount out;
    out.state = accountStateOf(a.state);
    out.detail = a.detail;
    return out;
}

void AppLibrarySettings::checkAccount() {
    env_.context.preview.service().forgetAccount();
    env_.context.preview.service().checkAccount();
}

std::string AppLibrarySettings::screenScraperUser() const { return user_; }
bool AppLibrarySettings::screenScraperPasswordSet() const { return passwordSet_; }

// SettingsScreen::handleAccountEntry(), SettingsScreen.cpp:775-783.
bool AppLibrarySettings::saveScreenScraperLogin(bool user, const std::string &value) {
    if (!ScreenScraperSettings::save(user ? "SCREENSCRAPER_SSID" : "SCREENSCRAPER_SSPASSWORD", value)) return false;
    refreshLogin();
    env_.context.preview.service().forgetAccount();
    if (env_.context.preferences.screenScraperEnabled()) env_.context.preview.service().checkAccount();
    return true;
}

// ---- Build game list and Prepare box art
std::vector<std::string> AppLibrarySettings::scanRoots() const { return LibraryScan::detectRoots(); }

patsgui::DriveSpace AppLibrarySettings::driveSpace(const std::string &root) const {
    struct statvfs fs;
    patsgui::DriveSpace space;
    if (::statvfs(root.c_str(), &fs) != 0) return space;   // a read of the file system's header only; nothing is written
    space.totalBytes = uint64_t(fs.f_blocks) * uint64_t(fs.f_frsize);
    space.freeBytes = uint64_t(fs.f_bavail) * uint64_t(fs.f_frsize);
    return space;
}

void AppLibrarySettings::startScan(const std::vector<std::string> &roots) {
    if (!env_.context.settingsHost.scan) return;
    env_.context.settingsHost.scan->start(roots);
    scanWatched_ = true;
}

void AppLibrarySettings::cancelScan() {
    if (env_.context.settingsHost.scan) env_.context.settingsHost.scan->cancel();
}

patsgui::ScanProgress AppLibrarySettings::scan() const {
    ScanProgress out;
    const LibraryScan *s = env_.context.settingsHost.scan;
    if (!s) return out;
    switch (s->state()) {
        case LibraryScan::State::Idle: out.state = ScanProgress::State::Idle; break;
        case LibraryScan::State::Discovering: out.state = ScanProgress::State::Discovering; break;
        case LibraryScan::State::Scanning: out.state = ScanProgress::State::Scanning; break;
        case LibraryScan::State::Writing: out.state = ScanProgress::State::Writing; break;
        case LibraryScan::State::Done: out.state = ScanProgress::State::Done; break;
        case LibraryScan::State::Failed: out.state = ScanProgress::State::Failed; break;
    }
    out.statusLine = s->statusLine();
    out.systemsFound = s->systemsFound();
    out.gamesFound = s->gamesFound();
    out.progress = s->progress();
    out.error = s->error();
    return out;
}

// ScanScreen::refreshArtworkSystems() (ScanScreen.cpp:30-41).
std::vector<patsgui::ArtworkSystem> AppLibrarySettings::artworkSystems() const {
    std::vector<patsgui::ArtworkSystem> out;
    for (const DatabaseSystem &system : env_.context.library.database().systems()) {
        if (system.count == 0 || LibretroIndex::platformFor(system.key).empty()) continue;
        patsgui::ArtworkSystem a;
        a.key = system.key;
        a.name = system.name;
        a.games = system.count;
        out.push_back(a);
    }
    std::sort(out.begin(), out.end(), [](const patsgui::ArtworkSystem &a, const patsgui::ArtworkSystem &b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return out;
}

uint64_t AppLibrarySettings::libraryGameCount() const { return env_.context.library.database().totalGames(); }

// App::closeScan() (App.cpp:490-495): artwork that arrived is on disk but not in the image cache yet. Classic asks the counters
// of the last job whenever it closes; here only a job started since the last close counts (a stale count reloaded for nothing).
void AppLibrarySettings::libraryTaskClosed() {
    const SettingsHost &h = env_.context.settingsHost;
    const bool ran = artworkRan_;
    artworkRan_ = false;
    if (ran && h.scraper && (h.scraper->fetched() || h.scraper->prepared()) && h.reloadLibrary) h.reloadLibrary();
}

void AppLibrarySettings::startArtwork(const std::vector<std::string> &systemKeys) {
    if (!env_.context.settingsHost.scraper) return;
    MediaScraper::Options options;   // the defaults, as ScanScreen::handle() uses them (backgrounds included)
    env_.context.settingsHost.scraper->start(env_.context.library.database(), options, systemKeys);
    artworkRan_ = true;
}

// ScanScreen::handle() page JumpStart: scan_.startJumpStart(roots_, 20, 20), roots_ = detectRoots() (an empty list does the same).
void AppLibrarySettings::startJumpStart() {
    if (!env_.context.settingsHost.scan) return;
    env_.context.settingsHost.scan->startJumpStart(LibraryScan::detectRoots(), 20, 20);
    scanWatched_ = true;   // onFinished_ = App::reloadLibrary() for a scan that ended well
}

// ScanScreen::update(), the jump-start branch after a scan with games: boxart only, no backgrounds, all systems.
void AppLibrarySettings::startJumpStartArtwork() {
    if (!env_.context.settingsHost.scraper) return;
    MediaScraper::Options options;
    options.boxart = true;
    options.backgrounds = false;
    env_.context.settingsHost.scraper->start(env_.context.library.database(), options);
    artworkRan_ = true;
}

void AppLibrarySettings::cancelArtwork() {
    if (env_.context.settingsHost.scraper) env_.context.settingsHost.scraper->cancel();
}

patsgui::ArtworkProgress AppLibrarySettings::artwork() const {
    ArtworkProgress out;
    const MediaScraper *s = env_.context.settingsHost.scraper;
    if (!s) return out;
    switch (s->state()) {
        case MediaScraper::State::Idle: out.state = ArtworkProgress::State::Idle; break;
        case MediaScraper::State::Preparing: out.state = ArtworkProgress::State::Preparing; break;
        case MediaScraper::State::Working: out.state = ArtworkProgress::State::Working; break;
        case MediaScraper::State::Done: out.state = ArtworkProgress::State::Done; break;
        case MediaScraper::State::Failed: out.state = ArtworkProgress::State::Failed; break;
    }
    out.statusLine = s->statusLine();
    out.fetched = s->fetched();
    out.prepared = s->prepared();
    out.skipped = s->skipped();
    out.missing = s->missing();
    out.progress = s->progress();
    out.etaSeconds = s->estimatedRemainingSeconds();
    out.totalGames = env_.context.library.database().totalGames();
    out.error = s->error();
    out.missesFile = MediaScraper::kMissesFile;
    return out;
}

// ---- Core and MRA updates (CoreMraUpdater is the Classic class, unchanged)
bool AppLibrarySettings::internetRouteAvailable() const { return routeAvailable(); }

bool AppLibrarySettings::startCoreCheck(bool fresh) {
    const bool started = coreUpdater_.startCheck(fresh);
    refreshCores();   // the dialog goes to Checking at once
    return started;
}

void AppLibrarySettings::cancelCoreCheck() { coreUpdater_.cancelCheck(); }

bool AppLibrarySettings::startCoreInstall() {
    const bool started = coreUpdater_.startInstall();
    refreshCores();
    return started;
}

void AppLibrarySettings::selectAllMissingOrOutdated() {
    coreUpdater_.selectAllMissingOrOutdated();
    refreshCores();
}

bool AppLibrarySettings::toggleCoreSelected(size_t index) {
    const bool toggled = coreUpdater_.toggleSelected(index);
    refreshCores();
    return toggled;
}

const patsgui::CoreMraProgress &AppLibrarySettings::cores() const { return coresCache_; }
