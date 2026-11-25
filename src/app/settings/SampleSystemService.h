#pragma once

// Sample System service: a settable disc drive, the disc package installer (all phases), the Info facts, the clock,
// time-zone and screensaver preferences, the backup task, the icon download, the Wi-Fi state machine (scan, list,
// test, country, save, rollback) and the leave/reboot calls. Every state is reachable with `--set system.<key>=...`
// (see help()), and every call is recorded in `calls` so a test can assert what a page did.
// The words of the texts it returns are Classic's (src/legacy/DiscCoreInstaller.cpp, src/legacy/Wifi.cpp, src/legacy/App.cpp).

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "app/settings/SampleControl.h"
#include "app/settings/SystemService.h"

namespace patsgui {

class SampleSystemService : public SystemService, public SampleControl {
public:
    // ---- settable state
    DiscDriveInfo driveInfo;
    DiscInstallInfo installInfo;
    bool hour24 = true;
    std::string zone;
    int saverDelay = 0;
    std::string saverStyle = "dimmed";
    BackupInfo backupInfo;
    bool backupOk = true;
    BackupInfo::State backupReported = BackupInfo::State::Idle;   // App::backupStateReported_
    int iconsMissing = 3;
    bool iconsRunning = false, iconsFailed = false, iconsOkResult = true;
    std::string iconError;
    int iconStartKind = -1;               // -1: derived (AllReady with nothing missing); else IconStart::Kind
    bool wifiPresent = true, scanFails = false, scanNone = false, scanError = false, beginFails = false;
    bool needsCountry = false, saveOk = true;
    enum class TestMode { Ok, Fail, Stall } testMode = TestMode::Ok;
    bool zonesEmpty = false, regionEmpty = false, zoneSaveOk = true, updateCommit = false;
    int volumes = 2;                      // Info: storage rows
    bool rebooted = false, rebootFails = false, leftToMenu = false, autoAdvance = true;
    std::vector<std::string> calls;       // every mutating call, in order ("wifi.rollback", "backup.start", ...)

    // ---- Wi-Fi state machine
    bool testing = false;
    float scanTimer = 0.0f, testClock = 0.0f;
    std::string testSsid;

    SampleSystemService() {
        driveInfo.media = DiscDriveInfo::Media::Readable;
        driveInfo.device = "/dev/sr0";
        driveInfo.model = "Example drive";
        driveInfo.stateName = "Disc ready";
        driveInfo.packageVersion = "0.7.0";
        driveInfo.projectUrl = "https://github.com/theshaneobrien/mister-disc-drive-support";
        setPackage("none");
        installInfo.message.clear();
    }

    bool available() const override { return true; }
    void update(float dt) override {
        if (scanTimer > 0.0f) scanTimer -= dt;
        if (!autoAdvance) return;
        clock_ += dt;
        if (clock_ >= 1.0f) {
            clock_ -= 1.0f;
            advanceInstall();
            if (backupInfo.state == BackupInfo::State::Running) {
                backupInfo.state = backupOk ? BackupInfo::State::Complete : BackupInfo::State::Failed;
                backupInfo.path = backupOk ? "/media/usb0/mister-pat-backups/mister-pat-settings-20261123-101500.tar.gz" : "";
            }
            if (iconsRunning) {
                iconsRunning = false;
                iconsFailed = !iconsOkResult;
                iconError = iconsOkResult ? "" : "failed to download nes: Could not resolve host";
                if (iconsOkResult) iconsMissing = 0;
            }
        }
    }

    // ---- Disc
    DiscDriveInfo disc() const override { return driveInfo; }
    DiscInstallInfo discInstall() const override { return installInfo; }
    bool startDiscInstall() override {
        calls.push_back("disc.start");
        if (installInfo.recoveryBlocked || installInfo.busy) return false;   // the message stays as it is (Classic)
        installInfo.state = DiscInstallInfo::State::Downloading;
        installInfo.message = "Preparing download\xE2\x80\xA6";
        installInfo.bytes = 0;
        installInfo.busy = true;
        cancelFlag_ = false;
        phaseClock_ = 0;
        return true;
    }
    void cancelDiscInstall() override { calls.push_back("disc.cancel"); cancelFlag_ = true; }

    // ---- Info
    std::vector<SystemFact> facts() const override {
        std::vector<SystemFact> f = {{"Machine", ""}, {"Processor", "ARMv7 Processor rev 5 (v7l), 2 cores"},
                                     {"In use", "12%  \xC2\xB7  load 0.31"}, {"Memory", "212 MB of 1.0 GB"}, {"Running for", "2h 14min"},
                                     {"", ""}, {"Networking", ""}, {"Ethernet", "192.168.1.20"}, {"Wi-Fi", "Offline"},
                                     {"Wi-Fi network", wifiPresent ? "Not connected" : "No Wi-Fi adapter"}, {"", ""}, {"Storage", ""}};
        for (int i = 0; i < volumes; ++i) {
            char label[32];
            std::snprintf(label, sizeof(label), i == 0 ? "/media/fat" : "/media/usb%d", i - 1);
            f.push_back({label, "86.4 GB of 119.1 GB free"});
        }
        const std::vector<SystemFact> rest = {{"", ""}, {"Library", ""}, {"Database", "18423 games in 41 systems"},
                                              {"Systems listed", "41"}, {"Favorites", "12"}, {"Imported game list", "18423 games"},
                                              {"", ""}, {"Display", ""}, {"Resolution", "1920x1080 @ 32 bpp, stride 7680, id=MiSTer FB, direct=1"},
                                              {"Font", "SpaceGrotesk-Regular.ttf"}, {"Images cached", "94"}};
        f.insert(f.end(), rest.begin(), rest.end());
        return f;
    }

    // ---- Clock, time zone, screensaver
    bool use24HourClock() const override { return hour24; }
    void setUse24HourClock(bool on) override { hour24 = on; calls.push_back(on ? "clock.24" : "clock.12"); }
    std::string timeZone() const override { return zone; }
    bool setTimeZone(const std::string &z) override {
        calls.push_back("zone.set " + z);
        if (!zoneSaveOk) return false;
        zone = z;
        return true;
    }
    std::vector<std::string> timeZoneRegions() const override {
        if (zonesEmpty) return {};
        return {"Africa", "America", "Antarctica", "Arctic", "Asia", "Atlantic", "Australia", "Brazil", "Canada", "Chile",
                "Etc", "Europe", "Indian", "Mexico", "Pacific", "US"};
    }
    std::vector<std::string> timeZonesIn(const std::string &region) const override {
        if (regionEmpty && region == "Antarctica") return {};
        if (region == "Europe") return {"Amsterdam", "Athens", "Berlin", "Brussels", "Dublin", "Helsinki", "Lisbon", "London",
                                        "Madrid", "Paris", "Rome", "Stockholm", "Vienna", "Zurich"};
        if (region == "America")   // a long list, to try the letter jump (the real list has 140+ entries)
            return {"Anchorage", "Argentina/Buenos_Aires", "Asuncion", "Atikokan", "Bogota", "Boise", "Cancun", "Caracas", "Cayenne",
                    "Chicago", "Costa_Rica", "Denver", "Detroit", "Edmonton", "El_Salvador", "Guatemala", "Guayaquil", "Halifax",
                    "Havana", "Indiana/Indianapolis", "Jamaica", "Juneau", "La_Paz", "Lima", "Los_Angeles", "Managua", "Manaus",
                    "Mexico_City", "Miami", "Montevideo", "Montreal", "Nassau", "New_York", "Nome", "Panama", "Phoenix",
                    "Puerto_Rico", "Regina", "Santiago", "Santo_Domingo", "Sao_Paulo", "St_Johns", "Tijuana", "Toronto",
                    "Vancouver", "Winnipeg"};
        return {"Example_City_A", "Example_City_B"};
    }
    int screensaverDelay() const override { return saverDelay; }
    void setScreensaverDelay(int s) override { saverDelay = s; calls.push_back("saver.delay " + std::to_string(s)); }
    std::string screensaverStyle() const override { return saverStyle; }
    void setScreensaverStyle(const std::string &s) override { saverStyle = s; calls.push_back("saver.style " + s); }

    // ---- Backup
    bool startBackup() override {
        calls.push_back("backup.start");
        if (backupInfo.state == BackupInfo::State::Running) return false;
        backupInfo.state = BackupInfo::State::Running;
        backupInfo.path.clear();
        backupReported = BackupInfo::State::Idle;   // App's start lambda: backupStateReported_ = Idle
        return true;
    }
    BackupInfo backup() const override {
        BackupInfo b = backupInfo;   // App::update(): state != backupStateReported_ && state != Running is reported once
        b.unreported = b.state != backupReported && b.state != BackupInfo::State::Running;
        return b;
    }
    void acknowledgeBackup() override { calls.push_back("backup.ack"); backupReported = backupInfo.state; }

    // ---- Icons (Classic: App::assetDownloadStatus / startAssetDownload)
    IconDownloadInfo icons() const override {
        IconDownloadInfo i;
        i.running = iconsRunning;
        i.failed = iconsFailed;
        i.error = iconError;
        i.missing = iconsMissing;
        i.statusText = iconsRunning ? "Downloading icons\xE2\x80\xA6" : iconsFailed ? "Try download again"
                       : iconsMissing ? std::to_string(iconsMissing) + " icons missing" : "All icons ready";
        return i;
    }
    IconStart startIconDownload() override {
        calls.push_back("icons.start");
        IconStart r;
        if (iconsRunning) { r.kind = IconStart::Kind::AlreadyRunning; return r; }
        if (iconStartKind == int(IconStart::Kind::Failed)) {
            iconsFailed = true;
            iconError = "cannot create the system icon directory";
            r.kind = IconStart::Kind::Failed;
            r.error = iconError;
            return r;
        }
        iconsFailed = false;
        iconError.clear();
        if (iconsMissing == 0) { r.kind = IconStart::Kind::AllReady; return r; }
        iconsRunning = true;
        r.kind = IconStart::Kind::Started;
        return r;
    }

    // ---- Wi-Fi
    bool wifiAvailable() const override { return wifiPresent; }
    bool wifiStartScan(std::string &error) override {
        calls.push_back("wifi.scan");
        if (!wifiPresent) { error = "No Wi-Fi adapter"; return false; }
        if (scanFails) { error = "The Wi-Fi supplicant rejected a command"; return false; }
        scanTimer = 2.5f;
        return true;
    }
    bool wifiScanComplete() const override { return scanTimer <= 0.0f; }
    std::vector<WifiNetwork> wifiScanResults(std::string &error) override {
        calls.push_back("wifi.results");
        if (scanError) { error = "wpa_cli did not complete successfully"; return {}; }
        if (scanNone) return {};
        return {{"Example-Home", "[WPA2-PSK-CCMP][ESS]", -48, false, true}, {"Example-Guest", "[ESS]", -61, true, true},
                {"Example-Legacy", "[WEP][ESS]", -70, false, false}, {"Example-Office", "[WPA2-PSK-CCMP][WPS][ESS]", -75, false, true},
                {"Example-WPA3-Only-Network-With-A-Very-Long-Name", "[SAE][ESS]", -80, false, false}};
    }
    bool wifiBeginTest(const std::string &ssid, const std::string &password, bool open, bool hidden, std::string &error) override {
        calls.push_back(std::string("wifi.begin ") + ssid + (open ? " open" : " secured") + (hidden ? " hidden" : "") +
                        " pw=" + std::to_string(password.size()));
        if (ssid.empty() || ssid.size() > 32) { error = "Network names must contain 1 to 32 bytes"; return false; }
        if (!wifiPresent) { error = "No Wi-Fi adapter"; return false; }
        if (!open && (password.size() < 8 || password.size() > 63)) { error = "Wi-Fi passwords must be 8 to 63 characters"; return false; }
        if (beginFails) { error = "The Wi-Fi supplicant rejected a command"; return false; }
        testing = true;
        testClock = 0.0f;
        testSsid = ssid;
        return true;
    }
    WifiTest wifiPollTest(float dt) override {
        WifiTest t;
        if (!testing) { t.status = "No Wi-Fi test is running"; t.result = WifiTest::Result::Failed; return t; }
        testClock += dt;
        if (testMode == TestMode::Stall) {
            if (testClock >= 30.0f) {
                t.status = "Could not connect to " + testSsid + ". Check the password and signal.";
                t.result = WifiTest::Result::Failed;
                testing = false;
                calls.push_back("wifi.timeout");
                return t;
            }
            t.status = "Waiting for Wi-Fi connection\xE2\x80\xA6";
            return t;
        }
        if (testClock < 0.5f) t.status = "Waiting for Wi-Fi connection\xE2\x80\xA6";
        else if (testClock < 1.0f) t.status = "Joining " + testSsid + "\xE2\x80\xA6";
        else if (testClock < 2.0f) t.status = "Connected; waiting for an IP address\xE2\x80\xA6";
        else if (testMode == TestMode::Ok) {
            t.status = "Connected to " + testSsid + " \xE2\x80\x94 IP 192.168.1.40";
            t.result = WifiTest::Result::Connected;
        } else {
            t.status = "Could not connect to " + testSsid + ". Check the password and signal.";
            t.result = WifiTest::Result::Failed;
            testing = false;   // Wifi::pollTest rolls back by itself on a timeout
            calls.push_back("wifi.autorollback");
        }
        return t;
    }
    bool wifiTesting() const override { return testing; }
    bool wifiNeedsCountryCode() const override { return needsCountry; }
    bool wifiSetCountryCode(const std::string &country, std::string &error) override {
        std::string n = country;
        for (char &c : n) c = char(std::toupper(static_cast<unsigned char>(c)));
        const bool valid = n.size() == 2 && n != "00" && std::isupper(static_cast<unsigned char>(n[0])) &&
                           std::isupper(static_cast<unsigned char>(n[1]));
        calls.push_back("wifi.country " + n);
        if (!valid) { error = "Enter a valid two-letter country code, for example DE"; return false; }
        country_ = n;
        return true;
    }
    bool wifiSaveTestedNetwork(std::string &error) override {
        calls.push_back("wifi.save");
        if (!testing) { error = "No tested Wi-Fi network to save"; return false; }
        if (!saveOk) { error = "Cannot save Wi-Fi config: Permission denied"; return false; }
        testing = false;   // the tested network stays active in the supplicant; no rollback later
        needsCountry = false;
        return true;
    }
    void wifiRollback() override { calls.push_back("wifi.rollback"); testing = false; }

    // ---- Leaving
    void returnToMisterMenu() override { calls.push_back("leave"); leftToMenu = true; }
    bool reboot(std::string &error) override {
        calls.push_back("reboot");
        if (rebootFails) { error = "Operation not permitted"; return false; }
        rebooted = true;
        return true;
    }

    bool set(const std::string &key, const std::string &v) override {
        if (key == "system.disc") {
            using M = DiscDriveInfo::Media;
            driveInfo.media = v == "nodrive" ? M::NoDrive : v == "nodisc" ? M::NoDisc : v == "checking" ? M::Checking
                              : v == "unsupported" ? M::Unsupported : v == "unreadable" ? M::Unreadable : M::Readable;
            static const char *names[] = {"No drive detected", "Please insert disc", "Checking disc", "Disc ready",
                                          "No supported game detected", "Disc could not be read"};
            driveInfo.stateName = names[int(driveInfo.media)];
            if (v == "nodrive") { driveInfo.device.clear(); driveInfo.model.clear(); }
            else { driveInfo.device = "/dev/sr0"; driveInfo.model = "Example drive"; }
            return true;
        }
        if (key == "system.discpackage") { setPackage(v); return true; }   // none|partial|ready|unverified
        if (key == "system.discinstall") {
            using S = DiscInstallInfo::State;
            installInfo.busy = false;
            installInfo.bytes = 0;
            installInfo.message.clear();
            if (v == "idle") installInfo.state = S::Idle;
            else if (v == "downloading") { installInfo.state = S::Downloading; installInfo.busy = true; installInfo.bytes = 3355443; installInfo.message = "Downloading MiSTer-disc\xE2\x80\xA6"; }
            else if (v == "verifying") { installInfo.state = S::Verifying; installInfo.busy = true; installInfo.message = "Verifying official package\xE2\x80\xA6"; }
            else if (v == "installing") { installInfo.state = S::Installing; installInfo.busy = true; installInfo.message = "Saving backups and installing\xE2\x80\xA6"; }
            else if (v == "ready") { installInfo.state = S::Ready; installInfo.message = "Disc package v0.7.0 installed"; }
            else if (v == "failed") { installInfo.state = S::Failed; installInfo.message = "downloaded package verification failed"; }
            else if (v == "cancelled") { installInfo.state = S::Cancelled; installInfo.message = "Download cancelled; installed files unchanged"; }
            else if (v == "blocked") {
                installInfo.state = S::Failed;
                installInfo.recoveryBlocked = true;
                installInfo.message = "Disc package recovery needs attention: missing managed-file backup required for recovery";
            } else return false;
            if (v != "blocked") installInfo.recoveryBlocked = false;
            installFinal_ = "";
            return true;
        }
        if (key == "system.installend") { installEnd_ = v; return v == "ready" || v == "failed" || v == "cancelled"; }
        if (key == "system.wifi") { wifiPresent = v != "none"; return true; }
        if (key == "system.wifiscan") { scanFails = v == "fail"; scanError = v == "error"; scanNone = v == "none"; return true; }   // ok|fail|error|none
        if (key == "system.wifibegin") { beginFails = v == "fail"; return true; }
        if (key == "system.wifitest") { testMode = v == "fail" ? TestMode::Fail : v == "stall" ? TestMode::Stall : TestMode::Ok; return true; }
        if (key == "system.country") { needsCountry = sampleBool(v); return true; }
        if (key == "system.save") { saveOk = v == "ok"; return true; }
        if (key == "system.backup") {
            backupInfo.state = v == "running" ? BackupInfo::State::Running : v == "complete" ? BackupInfo::State::Complete
                               : v == "failed" ? BackupInfo::State::Failed : BackupInfo::State::Idle;
            backupInfo.path = v == "complete" ? "/media/usb0/mister-pat-backups/mister-pat-settings-20261123-101500.tar.gz" : "";
            return true;
        }
        if (key == "system.backupresult") { backupOk = v == "ok"; return true; }
        if (key == "system.icons") {   // idle|running|failed|startfails|<n> missing
            iconsRunning = v == "running";
            iconsFailed = v == "failed";
            iconError = iconsFailed ? "failed to download nes: Could not resolve host" : "";
            if (v == "startfails") iconStartKind = int(IconStart::Kind::Failed);
            else if (v == "ready") iconsMissing = 0;
            else if (!v.empty() && std::isdigit(static_cast<unsigned char>(v[0]))) iconsMissing = std::atoi(v.c_str());
            return true;
        }
        if (key == "system.iconsresult") { iconsOkResult = v == "ok"; return true; }
        if (key == "system.zones") { zonesEmpty = v == "empty"; regionEmpty = v == "regionempty"; return true; }
        if (key == "system.zonesave") { zoneSaveOk = v == "ok"; return true; }
        if (key == "system.volumes") { volumes = std::max(0, std::atoi(v.c_str())); return true; }
        if (key == "system.clock") { hour24 = v != "12"; return true; }
        if (key == "system.zone") { zone = v == "default" ? "" : v; return true; }
        if (key == "system.saver") { saverDelay = std::atoi(v.c_str()); return true; }
        if (key == "system.saverstyle") { saverStyle = v; return true; }
        if (key == "system.reboot") { rebootFails = v == "fails"; return true; }
        if (key == "system.auto") { autoAdvance = sampleBool(v); return true; }
        return false;
    }
    std::vector<std::string> help() const override {
        return {"system.disc=nodrive|nodisc|checking|readable|unsupported|unreadable",
                "system.discpackage=none|partial|ready|unverified",
                "system.discinstall=idle|downloading|verifying|installing|ready|failed|cancelled|blocked (a state to look at)",
                "system.installend=ready|failed|cancelled (how a started install ends; default ready)",
                "system.wifi=none|ready", "system.wifiscan=ok|fail|error|none (start fails / results error / no networks)",
                "system.wifibegin=ok|fail (the test cannot start)", "system.wifitest=ok|fail|stall (stall: 30 s timeout)",
                "system.country=0|1 (the save asks for a country code)", "system.save=ok|fail",
                "system.backup=idle|running|complete|failed", "system.backupresult=ok|fail (how a started backup ends)",
                "system.icons=idle|running|failed|startfails|ready|<n> (n icons missing)", "system.iconsresult=ok|fail",
                "system.zones=ok|empty|regionempty", "system.zonesave=ok|fail", "system.volumes=<n> (Info storage rows)",
                "system.clock=24|12", "system.zone=default|Region/City", "system.saver=0|30|300|600|1200|1800|3600",
                "system.saverstyle=dimmed|boxart|boxart-collage", "system.reboot=ok|fails",
                "system.auto=0|1 (tasks advance by themselves)"};
    }

private:
    void setPackage(const std::string &v) {
        driveInfo.packageReady = v == "ready";
        const bool any = v != "none";
        driveInfo.alternateMainInstalled = any;
        driveInfo.alternateMainVerified = v == "ready";
        driveInfo.scopedMainConfigured = v == "ready" || v == "unverified";
        driveInfo.psxMglInstalled = driveInfo.psxMglCompatible = v == "ready" || v == "unverified" || v == "partial";
        driveInfo.psxMglVerified = v == "ready";
        driveInfo.psxCoreInstalled = v == "ready";
        driveInfo.saturnMglInstalled = v == "ready" || v == "unverified";
        driveInfo.saturnMglCompatible = v == "ready" || v == "unverified";
        driveInfo.saturnMglVerified = v == "ready";
        driveInfo.saturnCoreInstalled = v == "ready";
    }
    // The installer's phases with Classic's messages (DiscCoreInstaller::installPackage): downloading the binary, the
    // MGL archive, verifying, installing, then Ready / Cancelled / Failed. The message is kept after the run.
    void advanceInstall() {
        using S = DiscInstallInfo::State;
        if (!installInfo.busy) return;
        ++phaseClock_;
        if (cancelFlag_ && installInfo.state != S::Installing) {
            installInfo.state = S::Cancelled;
            installInfo.message = "Download cancelled; installed files unchanged";
            installInfo.busy = false;
            return;
        }
        switch (phaseClock_) {
            case 1: installInfo.state = S::Downloading; installInfo.message = "Downloading MiSTer-disc\xE2\x80\xA6"; installInfo.bytes = 2411724; break;
            case 2: installInfo.message = "Downloading Disc MGLs\xE2\x80\xA6"; installInfo.bytes = 3482; break;
            case 3: installInfo.state = S::Verifying; installInfo.message = "Verifying official package\xE2\x80\xA6"; installInfo.bytes = 0; break;
            case 4: installInfo.state = S::Installing; installInfo.message = "Saving backups and installing\xE2\x80\xA6"; break;
            default:
                installInfo.busy = false;
                if (installEnd_ == "failed") { installInfo.state = S::Failed; installInfo.message = "downloaded package verification failed"; }
                else if (installEnd_ == "cancelled") { installInfo.state = S::Cancelled; installInfo.message = "Download cancelled; installed files unchanged"; }
                else {
                    installInfo.state = S::Ready;
                    installInfo.message = "Disc package v0.7.0 installed";
                    setPackage("ready");
                }
                break;
        }
    }

    float clock_ = 0.0f;
    int phaseClock_ = 0;
    bool cancelFlag_ = false;
    std::string installEnd_ = "ready", installFinal_, country_;
};

}  // namespace patsgui
