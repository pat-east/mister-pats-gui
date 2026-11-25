#pragma once

// Sample Library service for the host runner and tests: preferences in memory (with Classic's validation), and
// state machines for the scan, the box-art job, the core updater, TheGamesDB and the ScreenScraper account that
// move by themselves (one step a second, `library.auto=0` stops that) or are set with `--set library.<key>=value`.
// Every call is counted, so a test can check that a page called the service exactly as Classic's code does.

#include <algorithm>
#include <cstdlib>
#include <ctime>

#include "app/settings/LibraryService.h"
#include "app/settings/SampleControl.h"

namespace patsgui {

class SampleLibraryService : public LibraryService, public SampleControl {
public:
    // ---- State the tests and `--set` change.
    bool versions = true, ssEnabled = false;
    int ssVolume = 0, ssDelay = 5, ssStorageMb = 256;
    std::string ssRegion = "eu", ssUser, ssPassword;
    TheGamesDbStatus tgdb;
    ScreenScraperAccount accountState;
    ScanProgress scanState;
    ArtworkProgress artState;
    CoreMraProgress coreState;
    bool online = true;
    bool autoAdvance = true;       // started tasks move on by themselves (1 s per step); false: only --set moves them
    bool saveFails = false;        // saveScreenScraperLogin() fails (the ini cannot be written)
    std::vector<std::string> roots{"/media/fat", "/media/usb0"};
    std::vector<ArtworkSystem> systems{{"NES", "Nintendo Entertainment System", 812}, {"SNES", "Super Nintendo", 640},
                                       {"MegaDrive", "Sega Mega Drive", 420}};
    std::string scanEnds = "done";       // what the running scan turns into: done | failed | nodrives | nogames (a jump start that finds none)
    std::string artEnds = "done";        // done | nomatch
    std::string checkEnds = "ready";     // what a core check turns into: ready | failed
    std::string installEnds = "complete";   // complete | failed
    std::string accountResult = "ok";    // what a started account check turns into: ok | nouser | rejected | unavailable
    std::string tgdbResult = "success";  // success | invalid | neterror
    // Counters of what the category called (a test compares them with what Classic would have called).
    int reloads = 0, tasksClosed = 0, accountChecks = 0, tgdbChecks = 0, scanStarts = 0, artworkStarts = 0, coreChecks = 0,
        coreFreshChecks = 0, coreInstalls = 0, loginSaves = 0, jumpStarts = 0, jumpArtworkStarts = 0;
    bool jumpScan = false;              // the running scan is a Jump Start (a sample of 20 games per system)
    std::vector<std::string> lastScanRoots;
    std::string lastArtworkKey = "?";   // the keys of the last start, joined with ","; "" = all systems

    SampleLibraryService() {
        tgdb.text = "Not configured";
        tgdb.settingsPath = "/media/fat/mister-pat/thegamesdb-settings.ini";
        accountState.detail = std::string();
        fillCores(24);
        coreState.phase = CoreMraProgress::Phase::Ready;   // a finished check, 2 minutes old, so the list pages show data at once
        coreState.completed = coreState.totalFiles;
        coreState.checkedAt = now() - 125;
    }

    bool available() const override { return true; }
    void update(float dt) override {
        if (!autoAdvance) return;
        clock_ += dt;
        if (clock_ < 1.0f) return;
        clock_ = 0.0f;
        advance();
    }

    // One step of every running machine (a second of the clock, or `--set library.step=1`).
    void advance() {
        using S = ScanProgress::State;
        if (scanState.state == S::Discovering) {
            if (scanEnds == "nodrives") {
                scanState.state = S::Failed;
                scanState.error = "No connected drives were found. Connect a drive with your games and try again.";
                scanState.statusLine = scanState.error;
            } else {
                scanState.state = S::Scanning;
                scanState.progress = 0.2f;
                scanState.statusLine = "System 1 of 3: Nintendo Entertainment System  ·  412 games found";
                scanState.systemsFound = 1;
                scanState.gamesFound = 412;
            }
        } else if (scanState.state == S::Scanning) {
            if (scanEnds == "failed") {
                scanState.state = S::Failed;
                scanState.error = "cannot write /media/fat/mister-pat/gamesdb.new/NES.tsv";
                scanState.statusLine = scanState.error;
            } else {
                scanState.state = S::Writing;
                scanState.progress = 0.6f;
                scanState.systemsFound = scanEnds == "nogames" ? 0 : 3;
                scanState.gamesFound = scanEnds == "nogames" ? 0 : jumpScan ? 52 : 1872;
                scanState.statusLine = "Saving game lists: 1 of 3";
            }
        } else if (scanState.state == S::Writing) {
            scanState.state = S::Done;
            scanState.progress = 1.0f;
            scanState.statusLine = "3 systems and 1872 games added to the saved list";
        }
        using A = ArtworkProgress::State;
        if (artState.state == A::Preparing) {
            artState.state = A::Working;
            artState.progress = 0.3f;
            artState.fetched = 12; artState.prepared = 10; artState.skipped = 3; artState.missing = 1;
            artState.etaSeconds = 95;
            artState.statusLine = "Game 15 of 50  ·  Nintendo Entertainment System  ·  Super Mario Bros.";
        } else if (artState.state == A::Working) {
            artState.state = A::Done;
            artState.progress = 1.0f;
            artState.fetched = 40; artState.prepared = 36; artState.skipped = 6; artState.missing = 4;
            artState.etaSeconds = 0;
            artState.statusLine = "Covers downloaded 40  ·  Prepared 36  ·  Already ready 6  ·  Not found 4";
        }
        using P = CoreMraProgress::Phase;
        if (coreState.phase == P::Checking) {
            if (coreState.completed < coreState.totalFiles / 2 && checkEnds != "failed") {
                coreState.completed = coreState.totalFiles / 2;
                for (size_t i = 0; i < size_t(coreState.completed) && i < coreState.entries.size(); ++i)
                    coreState.recent.push_back({coreState.entries[i].name, coreState.entries[i].status});
            } else if (checkEnds == "failed") {
                coreState.phase = P::Failed;
                coreState.message = "The official MiSTer distribution could not be reached. Check the network connection and try again.";
                coreState.detail = "Could not resolve host: raw.githubusercontent.com";
            } else {
                coreState.phase = P::Ready;
                coreState.completed = coreState.totalFiles;
                coreState.message = "Check complete";
                coreState.checkedAt = now();
            }
        } else if (coreState.phase == P::Installing) {
            const uint64_t selected = selectedCount();
            if (coreState.current < selected) {
                ++coreState.current;
                coreState.currentTotal = 2400000;
                coreState.currentBytes = 1200000;
                coreState.message = "Downloading " + std::to_string(coreState.current);
            } else {
                coreState.installed = installEnds == "failed" && selected > 0 ? selected - 1 : selected;
                coreState.failed = selected - coreState.installed;
                coreState.phase = coreState.failed ? P::Failed : P::Complete;
                coreState.message = coreState.failed ? "Some files could not be installed" : "Updates complete";
            }
        }
        if (tgdb.state == TheGamesDbStatus::State::Checking) {
            if (tgdbResult == "invalid") { tgdb.state = TheGamesDbStatus::State::InvalidOrLimited; tgdb.text = "Key invalid or allowance reached"; }
            else if (tgdbResult == "neterror") { tgdb.state = TheGamesDbStatus::State::NetworkError; tgdb.text = "Network/HTTP error; try again"; }
            else { tgdb.state = TheGamesDbStatus::State::Success; tgdb.text = "1,234 left · reset in 3d 4h"; }
        }
        if (accountState.state == ScreenScraperAccount::State::Checking) {
            using AS = ScreenScraperAccount::State;
            if (accountResult == "nouser") { accountState.state = AS::NoUserLogin; accountState.detail = "Enter your ScreenScraper user name and password"; }
            else if (accountResult == "rejected") { accountState.state = AS::Rejected; accountState.detail = "ScreenScraper did not accept your user name or password"; }
            else if (accountResult == "unavailable") { accountState.state = AS::Unavailable; accountState.detail = "Could not reach ScreenScraper"; }
            else { accountState.state = AS::Ok; accountState.detail = "Logged in (level 3)"; }
        }
    }

    // ---- Preferences (Classic's setters validate: Preferences.cpp:331-355)
    bool organizeGameVersions() const override { return versions; }
    void setOrganizeGameVersions(bool on) override { versions = on; }
    bool screenScraperEnabled() const override { return ssEnabled; }
    void setScreenScraperEnabled(bool on) override {
        ssEnabled = on;
        if (on) { forget(); checkAccount(); }
    }
    int screenScraperVolume() const override { return ssVolume; }
    void setScreenScraperVolume(int v) override {
        if (v == 0 || v == 100 || v == 75 || v == 50 || v == 25 || v == 10) ssVolume = v;
    }
    std::string screenScraperRegion() const override { return ssRegion; }
    void setScreenScraperRegion(const std::string &r) override {
        if (r == "eu" || r == "us" || r == "jp") ssRegion = r;
    }
    int screenScraperDelay() const override { return ssDelay; }
    void setScreenScraperDelay(int s) override {
        if (s >= 5 && s <= 30 && s % 5 == 0) ssDelay = s;
    }
    int screenScraperStorageLimitMb() const override { return ssStorageMb; }
    void setScreenScraperStorageLimitMb(int mb) override {
        if (mb == 0 || mb == 256 || mb == 512 || mb == 1024 || mb == 2048 || mb == 4096 || mb == 8192) ssStorageMb = mb;
    }

    // Sample ScreenScraper.fr videos (--set library.videos=none|few|many).
    std::vector<ScrapedVideo> videos = {
        {"101-video-normalized", "Super Mario World", "Super Nintendo", 6400000},
        {"102-video-normalized", "Sonic the Hedgehog", "Sega Mega Drive", 4900000},
        {"103-video", "The Legend of Zelda", "Nintendo Entertainment System", 11800000},
        {"104-video-normalized", "Street Fighter II", "Super Nintendo", 7200000},
    };
    std::vector<ScrapedVideo> scrapedVideos() override { return videos; }
    bool deleteScrapedVideo(const std::string &id) override {
        for (size_t i = 0; i < videos.size(); ++i)
            if (videos[i].id == id) { videos.erase(videos.begin() + long(i)); return true; }
        return false;
    }
    size_t deleteAllScrapedVideos() override { const size_t n = videos.size(); videos.clear(); return n; }

    void reloadSavedGameList() override { ++reloads; }

    TheGamesDbStatus theGamesDb() const override { return tgdb; }
    bool startTheGamesDbCheck() override {
        if (!tgdb.configured || tgdb.state == TheGamesDbStatus::State::Checking) return false;
        ++tgdbChecks;
        tgdb.state = TheGamesDbStatus::State::Checking;
        tgdb.text = "Checking API key…";
        return true;
    }

    ScreenScraperAccount account() const override { return accountState; }
    void checkAccount() override {
        ++accountChecks;
        if (accountState.state == ScreenScraperAccount::State::Checking) return;   // forgetAccount() and checkAccount() do nothing while checking
        accountState.state = ScreenScraperAccount::State::Checking;
        accountState.detail = "Checking...";
    }
    std::string screenScraperUser() const override { return ssUser; }
    bool screenScraperPasswordSet() const override { return !ssPassword.empty(); }
    bool saveScreenScraperLogin(bool user, const std::string &value) override {
        ++loginSaves;
        if (saveFails) return false;
        (user ? ssUser : ssPassword) = value;
        forget();
        if (ssEnabled) checkAccount();
        return true;
    }

    std::vector<std::string> scanRoots() const override { return roots; }
    DriveSpace driveSpace(const std::string &root) const override {
        const uint64_t gb = 1000ull * 1000 * 1000;
        if (root == "/media/fat") return {64 * gb, 41 * gb};
        if (root == "/media/usb0") return {2000 * gb, 640 * gb};
        if (root == "/media/usb1") return {512 * gb, 12 * gb};
        return {};   // "/media/usb2": unknown, the page shows the path only
    }
    void startScan(const std::vector<std::string> &r) override {
        ++scanStarts;
        jumpScan = false;
        lastScanRoots = r;
        scanState = ScanProgress();
        scanState.state = ScanProgress::State::Discovering;
        scanState.statusLine = "Looking for game folders on " + std::to_string(r.size()) + " drives…";
    }
    // The first-run Jump Start (a scan with limits on every detected drive); it ends like a scan (scanEnds).
    void startJumpStart() override {
        ++jumpStarts;
        startScan(roots);
        jumpScan = true;
        --scanStarts;   // counted as a jump start, not as a scan
    }
    void startJumpStartArtwork() override {
        startArtwork({});
        --artworkStarts;
        ++jumpArtworkStarts;
    }
    void cancelScan() override {
        using S = ScanProgress::State;
        if (scanState.state == S::Discovering || scanState.state == S::Scanning || scanState.state == S::Writing) {
            scanState.error = "Scan cancelled. The game list was not replaced.";
            scanState.statusLine = scanState.error;
            scanState.state = S::Failed;
        }
    }
    ScanProgress scan() const override { return scanState; }
    std::vector<ArtworkSystem> artworkSystems() const override { return systems; }
    uint64_t libraryGameCount() const override {
        uint64_t total = 0;
        for (const ArtworkSystem &s : systems) total += s.games;
        return total;
    }
    void libraryTaskClosed() override { ++tasksClosed; }
    void startArtwork(const std::vector<std::string> &keys) override {
        ++artworkStarts;
        lastArtworkKey.clear();
        for (const std::string &k : keys) lastArtworkKey += (lastArtworkKey.empty() ? "" : ",") + k;
        artState = ArtworkProgress();
        artState.totalGames = libraryGameCount();
        artState.missesFile = "/media/fat/mister-pat/scrape-misses.txt";
        if (artEnds == "nomatch") {
            artState.state = ArtworkProgress::State::Failed;
            artState.error = "No matching covers were found in the online artwork library.";
            artState.statusLine = "Could not prepare covers: " + artState.error;
            return;
        }
        artState.state = ArtworkProgress::State::Preparing;
        artState.statusLine = "Loading the cover list for Nintendo Entertainment System…";
    }
    void cancelArtwork() override {
        using A = ArtworkProgress::State;
        if (artState.state == A::Preparing || artState.state == A::Working) {
            artState.error = "Cover search stopped. Covers already saved were kept.";
            artState.statusLine = "Could not prepare covers: " + artState.error;
            artState.state = A::Failed;
        }
    }
    ArtworkProgress artwork() const override { return artState; }

    bool internetRouteAvailable() const override { return online; }
    bool startCoreCheck(bool fresh) override {
        if (coreState.phase == CoreMraProgress::Phase::Checking || coreState.phase == CoreMraProgress::Phase::Installing) return false;
        ++coreChecks;
        if (fresh) ++coreFreshChecks;
        const std::vector<CoreMraFile> keep = coreState.entries;
        coreState = CoreMraProgress();
        coreState.phase = CoreMraProgress::Phase::Checking;
        coreState.entries = keep;
        coreState.totalFiles = keep.size();
        coreState.currentTotal = keep.size();
        for (CoreMraFile &f : coreState.entries)
            f.selected = f.status == CoreMraFile::Status::Missing || f.status == CoreMraFile::Status::UpdateAvailable ||
                         f.status == CoreMraFile::Status::Modified;
        return true;
    }
    void cancelCoreCheck() override {
        if (coreState.phase == CoreMraProgress::Phase::Checking) {
            coreState.phase = CoreMraProgress::Phase::Failed;
            coreState.message = "Update check stopped";
        }
    }
    bool startCoreInstall() override {
        if (coreState.phase != CoreMraProgress::Phase::Ready || selectedCount() == 0) return false;
        ++coreInstalls;
        coreState.phase = CoreMraProgress::Phase::Installing;
        coreState.completed = coreState.installed = coreState.failed = coreState.current = 0;
        coreState.totalFiles = selectedCount();
        coreState.message = "Starting installation";
        return true;
    }
    void selectAllMissingOrOutdated() override {
        if (coreState.phase != CoreMraProgress::Phase::Ready) return;
        for (CoreMraFile &f : coreState.entries)
            f.selected = f.status == CoreMraFile::Status::Missing || f.status == CoreMraFile::Status::UpdateAvailable ||
                         f.status == CoreMraFile::Status::Modified;
    }
    bool toggleCoreSelected(size_t i) override {
        if (coreState.phase != CoreMraProgress::Phase::Ready || i >= coreState.entries.size()) return false;
        CoreMraFile &f = coreState.entries[i];
        if (f.status == CoreMraFile::Status::UpToDate || f.status == CoreMraFile::Status::Error) return false;
        f.selected = !f.selected;
        return true;
    }
    const CoreMraProgress &cores() const override { return coreState; }

    bool set(const std::string &key, const std::string &v) override {
        using S = ScanProgress::State;
        if (key == "library.scan") {
            scanState.state = v == "discovering" ? S::Discovering : v == "scanning" || v == "arcade" ? S::Scanning : v == "writing" ? S::Writing
                              : v == "done" ? S::Done : v == "failed" ? S::Failed : S::Idle;
            scanState.progress = v == "scanning" ? 0.35f : v == "arcade" ? 0.4f : v == "writing" ? 0.7f : v == "done" ? 1.0f : 0.0f;
            scanState.statusLine = v == "discovering" ? "Looking for game folders on 2 drives…"
                                 : v == "scanning" ? "System 2 of 3: Super Nintendo  ·  1100 games found"
                                 : v == "arcade" ? "Arcade  \xC2\xB7  Checking Arcade game 1200 of 3400"
                                 : v == "writing" ? "Saving game lists: 2 of 3"
                                 : v == "done" ? "3 systems and 1872 games added to the saved list" : "";
            scanState.systemsFound = v == "done" ? 3 : 2;
            scanState.gamesFound = v == "done" ? 1872 : 1100;
            if (v == "failed") scanState.error = "No supported game folders were found. Check that your games are in the folders used by the installed systems.";
            if (v == "cancelled") { scanState.state = S::Failed; scanState.error = "Scan cancelled. The game list was not replaced."; }
            return true;
        }
        if (key == "library.videos") {   // none | few (4, the default) | many (60)
            if (v == "none") videos.clear();
            else if (v == "many") {
                videos.clear();
                for (int i = 0; i < 60; ++i)
                    videos.push_back({std::to_string(200 + i) + "-video-normalized", "Sample game " + std::to_string(i + 1),
                                      i % 2 ? "Super Nintendo" : "Sega Mega Drive", 3000000LL + 170000LL * i});
            } else if (v != "few") return false;
            return true;
        }
        if (key == "library.scan.ends") { scanEnds = v; return true; }
        if (key == "library.art.ends") { artEnds = v; return true; }
        if (key == "library.artwork") {
            using A = ArtworkProgress::State;
            artState.state = v == "preparing" ? A::Preparing : v == "working" ? A::Working : v == "done" ? A::Done : v == "failed" ? A::Failed : A::Idle;
            artState.statusLine = v == "preparing" ? "Loading the cover list for Super Nintendo…"
                                : v == "working" ? "Game 15 of 50  ·  Super Nintendo  ·  Super Mario World" : "";
            artState.fetched = 120; artState.prepared = 100; artState.skipped = 8; artState.missing = v == "done" ? 5 : 2;
            artState.progress = v == "done" ? 1.0f : 0.3f;
            artState.etaSeconds = v == "working" ? 95 : v == "done" ? 0 : -1;
            artState.totalGames = libraryGameCount();
            artState.missesFile = "/media/fat/mister-pat/scrape-misses.txt";
            if (v == "failed") artState.error = "Cover search stopped. Covers already saved were kept.";
            return true;
        }
        if (key == "library.systems") {
            if (v == "none") systems.clear();
            else if (v == "many") {
                systems.clear();
                for (int i = 0; i < 12; ++i)
                    systems.push_back({"S" + std::to_string(i), "System number " + std::to_string(i), uint64_t(100 + i)});
            }
            return true;
        }
        if (key == "library.roots") {
            if (v == "none") roots.clear();
            else if (v == "one") roots = {"/media/fat"};
            else if (v == "six") roots = {"/media/fat", "/media/usb0", "/media/usb1", "/media/usb2", "/media/usb3", "/media/usb4"};
            else if (v == "many") roots = {"/media/fat", "/media/usb0", "/media/usb1", "/media/usb2"};
            else roots = {"/media/fat", "/media/usb0"};
            return true;
        }
        if (key == "library.cores") {
            using P = CoreMraProgress::Phase;
            coreState.phase = v == "checking" ? P::Checking : v == "ready" ? P::Ready : v == "installing" ? P::Installing
                              : v == "complete" ? P::Complete : v == "failed" ? P::Failed : P::Idle;
            coreState.totalFiles = coreState.entries.size();
            coreState.currentTotal = coreState.entries.size();
            coreState.completed = v == "ready" ? coreState.entries.size() : coreState.entries.size() / 3;
            coreState.current = 2;
            coreState.currentBytes = 900000;
            coreState.currentTotal = v == "installing" ? 2400000 : coreState.currentTotal;
            coreState.installed = 1;
            coreState.failed = v == "failed" && !coreState.detail.empty() ? 0 : (v == "failed" ? 1 : 0);
            coreState.message = v == "failed" ? "The official MiSTer distribution could not be reached. Check the network connection and try again."
                              : v == "installing" ? "Downloading Example2_20260101.rbf" : v == "complete" ? "Updates complete" : "";
            if (v == "failed") coreState.detail = "Could not resolve host: raw.githubusercontent.com";
            if (v == "ready") coreState.checkedAt = now() - 125;
            if (v == "checking") {
                coreState.recent.clear();
                for (size_t i = 0; i < 12 && i < coreState.entries.size(); ++i)
                    coreState.recent.push_back({coreState.entries[i].name, coreState.entries[i].status});
            }
            return true;
        }
        if (key == "library.cores.ends") { checkEnds = v; return true; }
        if (key == "library.install.ends") { installEnds = v; return true; }
        if (key == "library.cores.count") { fillCores(size_t(std::max(0, std::atoi(v.c_str())))); return true; }
        if (key == "library.tgdb") {
            using T = TheGamesDbStatus::State;
            tgdb.state = v == "checking" ? T::Checking : v == "success" ? T::Success : v == "invalid" ? T::InvalidOrLimited
                         : v == "neterror" ? T::NetworkError : v == "badresponse" ? T::InvalidResponse : T::NotChecked;
            tgdb.configured = v != "unconfigured";
            tgdb.text = v == "unconfigured" ? "Not configured" : v == "configured" || v == "notchecked" ? "Configured"
                      : v == "checking" ? "Checking API key…" : v == "success" ? "API key works"
                      : v == "invalid" ? "Key invalid or allowance reached" : v == "neterror" ? "Network/HTTP error; try again"
                      : v == "badresponse" ? "Unexpected API response" : v;
            return true;
        }
        if (key == "library.tgdb.result") { tgdbResult = v; return true; }
        if (key == "library.account") {
            using A = ScreenScraperAccount::State;
            accountState.state = v == "checking" ? A::Checking : v == "ok" ? A::Ok : v == "nologin" ? A::NoUserLogin
                                 : v == "nodev" ? A::NoDeveloperLogin : v == "rejected" ? A::Rejected
                                 : v == "unavailable" ? A::Unavailable : v == "blocked" ? A::Blocked : A::Unknown;
            accountState.detail = v == "checking" ? "Checking..." : v == "ok" ? "Logged in (level 3)"
                                : v == "nologin" ? "Enter your ScreenScraper user name and password"
                                : v == "nodev" ? "Developer login missing in the settings file"
                                : v == "rejected" ? "ScreenScraper did not accept your user name or password"
                                : v == "unavailable" ? "Could not reach ScreenScraper"
                                : v == "blocked" ? "The daily limit of ScreenScraper is used up" : std::string();
            return true;
        }
        if (key == "library.account.result") { accountResult = v; return true; }
        if (key == "library.user") { ssUser = v; return true; }
        if (key == "library.password") { ssPassword = v; return true; }
        if (key == "library.savefail") { saveFails = sampleBool(v); return true; }
        if (key == "library.online") { online = sampleBool(v); return true; }
        if (key == "library.auto") { autoAdvance = sampleBool(v); return true; }
        if (key == "library.step") { advance(); return true; }
        return false;
    }
    std::vector<std::string> help() const override {
        return {"library.scan=idle|discovering|scanning|writing|done|failed|cancelled", "library.scan.ends=done|failed|nodrives (a started scan)",
                "library.artwork=idle|preparing|working|done|failed", "library.art.ends=done|nomatch (a started box-art job)",
                "library.systems=none|many (the box-art system list)", "library.roots=none|one|two|many|six (the drives found)",
                "library.cores=idle|checking|ready|installing|complete|failed", "library.cores.ends=ready|failed (a started check)",
                "library.install.ends=complete|failed", "library.cores.count=N (files in the update list)",
                "library.tgdb=unconfigured|configured|checking|success|invalid|neterror|badresponse", "library.tgdb.result=success|invalid|neterror",
                "library.account=unknown|checking|ok|nologin|nodev|rejected|unavailable|blocked", "library.account.result=ok|nouser|rejected|unavailable",
                "library.user=NAME", "library.password=TEXT", "library.savefail=0|1 (the login cannot be saved)",
                "library.online=0|1", "library.auto=0|1 (tasks advance by themselves)", "library.step=1 (one step of every task)"};
    }

private:
    float clock_ = 0.0f;

    static int64_t now() { return int64_t(std::time(nullptr)); }
    void forget() {
        if (accountState.state != ScreenScraperAccount::State::Checking) accountState = ScreenScraperAccount();
    }
    uint64_t selectedCount() const {
        uint64_t n = 0;
        for (const CoreMraFile &f : coreState.entries)
            if (f.selected) ++n;
        return n;
    }
    void fillCores(size_t count) {
        coreState.entries.clear();
        static const CoreMraFile::Status statuses[] = {CoreMraFile::Status::UpToDate, CoreMraFile::Status::Missing,
                                                       CoreMraFile::Status::Modified, CoreMraFile::Status::UpToDate,
                                                       CoreMraFile::Status::Error, CoreMraFile::Status::Missing};
        for (size_t i = 0; i < count; ++i) {
            CoreMraFile f;
            const size_t kind = i * 4 / std::max<size_t>(1, count);
            f.type = kind == 0 ? CoreMraFile::Type::ArcadeCore : kind == 1 ? CoreMraFile::Type::ConsoleCore
                     : kind == 2 ? CoreMraFile::Type::OtherCore : CoreMraFile::Type::ArcadeMra;
            const char *folder = f.type == CoreMraFile::Type::ArcadeMra ? "_Arcade/" : f.type == CoreMraFile::Type::ArcadeCore ? "_Arcade/cores/" : f.type == CoreMraFile::Type::ConsoleCore ? "_Console/" : "_Computer/";
            const std::string base = f.type == CoreMraFile::Type::ArcadeMra ? "Game" + std::to_string(i) : "Example" + std::to_string(i) + "_20260101";
            f.name = base + (f.type == CoreMraFile::Type::ArcadeMra ? ".mra" : ".rbf");
            f.path = std::string(folder) + f.name;
            f.size = f.type == CoreMraFile::Type::ArcadeMra ? 3000 + 100 * i : 2400000 + 1000 * i;
            f.status = statuses[i % 6];
            f.selected = f.status == CoreMraFile::Status::Missing || f.status == CoreMraFile::Status::Modified;
            coreState.entries.push_back(f);
        }
        coreState.totalFiles = count;
    }
};

}  // namespace patsgui
