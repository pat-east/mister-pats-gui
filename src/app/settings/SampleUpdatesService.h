#pragma once

// Sample Updates service: the update state machine of Classic's UpdateService (src/legacy/UpdateService.cpp) with a settable
// phase, and commands that obey the same guards as the real ones (startCheck only in Idle/Current/Available/Error/Blocked,
// beginConfirmation only Available + canInstall, startInstall only Confirming, removePinnedVersion only in the phases
// Classic allows, ...), so every row, dialog and message can be shown and driven on the host without a device.
// Defaults equal the first level's sample MenuState (running 0.8.10, "You are using the newest release").
//
//   --set updates.phase=...        puts the service into a phase (help() lists them)
//   --set updates.result=...       what the next check finds (available | current | error | blocked)
//   --set updates.total=BYTES      0 (Classic: bytesTotal is never set, the row says "Downloaded N MB") or a size (the row says "N%")
//   --set updates.fail=restart|pin|none   the next restart / pin removal fails with Classic's error text
//   --set updates.auto=0|1         the phases advance by themselves (default on)

#include <cstdio>

#include "app/settings/SampleControl.h"
#include "app/settings/UpdatesService.h"

namespace patsgui {

class SampleUpdatesService : public UpdatesService, public SampleControl {
public:
    UpdateInfo info;
    bool automatic = false;
    bool changelogBusy = false;
    bool autoAdvance = true;
    std::string result = "available";       // what a check finds
    std::string fail = "none";              // restart | pin | none
    uint64_t total = 0;                     // 0 = Classic's real behavior (bytesTotal stays 0)
    int restarts = 0, checks = 0, installs = 0, cancels = 0, pinRemovals = 0;
    bool lastRestartRemovedPin = false;

    SampleUpdatesService() {
        info.phase = UpdatePhase::Current;
        info.runningVersion = info.selectedVersion = info.newestInstalledVersion = "0.8.10";
        info.statusText = "You are using the newest release";
        info.targetVersion = "0.9.0";
        info.releaseNotes = "# Version 0.9.0\n\n**New:** the new GUI.\n\n## Changes\n\n- Example change one\n- Example change two\n"
                            "- Example change three with a longer line that wraps over the width of the dialog to show how the release notes behave\n"
                            "- Example change four\n- Example change five\n- Example change six\n- Example change seven\n- Example change eight\n"
                            "- Example change nine\n- Example change ten\n- Example change eleven\n- Example change twelve\n";
        info.changelog = "# Changelog\n\n## 0.9.0\n\n- The new GUI.\n\n## 0.8.10\n\n- Fixes.\n";
    }

    bool available() const override { return true; }

    void update(float dt) override {
        if (!autoAdvance) return;
        clock_ += dt;
        const float step = info.phase == UpdatePhase::Downloading ? 0.5f : 1.5f;
        if (clock_ < step) return;
        clock_ = 0.0f;
        using P = UpdatePhase;
        if (info.phase == P::WaitingForInternet) {
            info.phase = P::Checking;
            info.statusText = "Checking GitHub for a newer version…";
        } else if (info.phase == P::Checking) {
            finishCheck();
        } else if (info.phase == P::Downloading) {
            info.bytesDone += total ? total / 5 : 8u * 1024u * 1024u;
            const uint64_t goal = total ? total : 40u * 1024u * 1024u;
            if (info.bytesDone >= goal) { info.phase = P::Verifying; info.statusText = "Checking downloaded files…"; }
            info.bytesDoneText = bytesText(info.bytesDone);
        } else if (info.phase == P::Verifying) {
            info.phase = P::Installing;
            info.statusText = "Installing the update…";
        } else if (info.phase == P::Installing) {
            info.phase = P::RestartRequired;
            info.successPromptVisible = true;
            info.pinRemoved = false;
            info.statusText = "v" + info.targetVersion + (info.pinned ? " installed; version pin remains active" : " installed — restart MiSTer");
            info.newestInstalledVersion = info.targetVersion;
            if (!info.pinned) info.selectedVersion = info.targetVersion;
        }
        if (changelogBusy) { changelogBusy = false; info.changelogLoading = false; ++info.changelogGeneration; }
    }
    UpdateInfo snapshot() const override { return info; }
    bool commitRunning() const override { return info.phase == UpdatePhase::Installing; }
    bool checkAutomatically() const override { return automatic; }
    void setCheckAutomatically(bool on) override { automatic = on; }   // Classic: Preferences + cancelAutomaticCheck()

    bool startCheck() override {   // UpdateService::startCheck(false)
        using P = UpdatePhase;
        if (!(info.phase == P::Idle || info.phase == P::Current || info.phase == P::Available || info.phase == P::Error ||
              info.phase == P::Blocked)) return false;
        ++checks;
        info.phase = P::Checking;
        info.statusText = "Checking GitHub for a newer version…";
        info.errorText.clear();
        info.canInstall = false;
        return true;
    }
    bool beginConfirmation() override {
        if (info.phase != UpdatePhase::Available || !info.canInstall) return false;
        info.phase = UpdatePhase::Confirming;
        return true;
    }
    bool startInstall() override {
        if (info.phase != UpdatePhase::Confirming) return false;
        ++installs;
        info.phase = UpdatePhase::Downloading;
        info.statusText = "Downloading the update…";
        info.bytesDone = 0;
        info.bytesTotal = total;
        info.bytesDoneText = bytesText(0);
        info.canInstall = false;
        return true;
    }
    void cancelBeforeCommit() override {
        if (info.phase == UpdatePhase::Confirming) {
            info.phase = UpdatePhase::Available;
            info.canInstall = true;
        } else if (info.phase == UpdatePhase::Downloading || info.phase == UpdatePhase::Verifying) {
            ++cancels;   // the real one aborts within ~200 ms and returns to Available
            info.phase = UpdatePhase::Available;
            info.statusText = "New version available: v" + info.targetVersion;
            info.canInstall = true;
        }
    }
    void dismissInstallSuccess() override {
        if (info.phase == UpdatePhase::RestartRequired) info.successPromptVisible = false;
    }
    void showInstallSuccess() override {
        if (info.phase == UpdatePhase::RestartRequired) info.successPromptVisible = true;
    }
    bool removePinnedVersion(std::string &error) override {
        using P = UpdatePhase;
        const P p = info.phase;
        if (!info.pinned || !(p == P::Idle || p == P::Current || p == P::Available || p == P::Blocked || p == P::Error || p == P::RestartRequired)) {
            error = "version pin cannot be removed while an update is active";
            return false;
        }
        if (fail == "pin") { error = "version pin changed; review the startup selection"; fail = "none"; return false; }
        ++pinRemovals;
        info.pinned = false;
        info.selectedVersion = info.newestInstalledVersion;
        info.phase = P::RestartRequired;
        info.statusText = "Version pin removed; next start selects v" + info.selectedVersion;
        info.canInstall = false;
        info.successPromptVisible = true;
        info.pinRemoved = true;
        return true;
    }
    bool restartAfterUpdate(bool removePin, std::string &error) override {   // UpdateService::prepareRestart + sync + reboot
        if (info.phase != UpdatePhase::RestartRequired) { error = "no completed update to restart for"; return false; }
        if (fail == "restart") {
            error = "startup selection changed; review the restart destination";
            fail = "none";
            return false;
        }
        if (removePin) {
            if (!info.pinned) { error = "no version pin to remove"; return false; }
            info.pinned = false;
            info.selectedVersion = info.newestInstalledVersion;
            info.statusText = "v" + info.targetVersion + " installed; restart MiSTer to load v" + info.selectedVersion;
        }
        lastRestartRemovedPin = removePin;
        ++restarts;
        return true;
    }
    bool startChangelog() override {
        if (changelogBusy) return true;
        changelogBusy = true; info.changelogLoading = true; ++info.changelogGeneration;
        return true;
    }
    bool restarted() const { return restarts > 0; }

    bool set(const std::string &key, const std::string &v) override {
        if (key == "updates.phase") {
            using P = UpdatePhase;
            info.phase = v == "idle" ? P::Idle : v == "waiting" ? P::WaitingForInternet : v == "checking" ? P::Checking
                         : v == "available" ? P::Available : v == "blocked" ? P::Blocked : v == "confirming" ? P::Confirming
                         : v == "downloading" ? P::Downloading : v == "verifying" ? P::Verifying : v == "installing" ? P::Installing
                         : v == "restart" || v == "restart-hidden" ? P::RestartRequired : v == "error" ? P::Error : P::Current;
            info.targetVersion = "0.9.0";
            info.canInstall = v == "available" || v == "confirming";
            info.bytesTotal = total;
            info.bytesDone = 16u * 1024u * 1024u;
            if (total) info.bytesDone = total * 42 / 100;
            info.bytesDoneText = bytesText(info.bytesDone);
            info.successPromptVisible = v == "restart";
            info.pinRemoved = false;
            info.repairPaths.clear();
            info.statusText = v == "available" ? "New version available: v0.9.0"
                : v == "error" ? "Network not ready — try Check now after Wi-Fi connects"
                : v == "blocked" ? "A required MiSTer startup file is missing. Reinstall the GUI to repair it."
                : v == "waiting" ? "Waiting for an Internet connection…" : v == "checking" ? "Checking GitHub for a newer version…"
                : (v == "restart" || v == "restart-hidden") ? (info.pinned ? "v0.9.0 installed; version pin remains active" : "v0.9.0 installed — restart MiSTer")
                : v == "downloading" ? "Downloading the update…" : v == "verifying" ? "Checking downloaded files…"
                : v == "installing" ? "Installing the update…" : v == "idle" ? "" : "You are using the newest release (GitHub: v0.8.10)";
            if (v == "idle") info.statusText.clear();
            if (v == "blocked") info.repairPaths = {"/media/fat/mister-pat/mister-pats-gui-0.9.0.so"};
            if (info.phase == P::RestartRequired && !info.pinned) info.selectedVersion = "0.9.0";
            return true;
        }
        if (key == "updates.pinned") {
            info.pinned = sampleBool(v);
            info.selectedVersion = info.pinned ? "0.8.9" : "0.8.10";
            if (info.phase == UpdatePhase::RestartRequired && !info.pinned) info.selectedVersion = "0.9.0";
            return true;
        }
        if (key == "updates.running") { info.runningVersion = v; return true; }
        if (key == "updates.newest") { info.newestInstalledVersion = v; return true; }
        if (key == "updates.selected") { info.selectedVersion = v; return true; }
        if (key == "updates.target") { info.targetVersion = v; return true; }
        if (key == "updates.rebuild") { info.databaseRebuildHint = sampleBool(v); return true; }
        if (key == "updates.notes") { info.releaseNotes = v == "short" ? "Fixes." : v == "empty" ? "" : info.releaseNotes; return true; }
        if (key == "updates.status") { info.statusText = v; return true; }
        if (key == "updates.paths") {
            info.repairPaths.clear();
            if (v != "none") info.repairPaths = {"/media/fat/mister-pat/mister-pats-gui-0.9.0.so", "/media/fat/mister-pat/tools"};
            return true;
        }
        if (key == "updates.result") { result = v; return true; }
        if (key == "updates.total") { total = uint64_t(std::strtoull(v.c_str(), nullptr, 10)); info.bytesTotal = total; return true; }
        if (key == "updates.fail") { fail = v; return true; }
        if (key == "updates.changelog") {
            info.changelogLoading = v == "loading";
            info.changelogError = v == "error" ? "Could not reach GitHub" : "";
            if (v == "empty") info.changelog.clear();
            ++info.changelogGeneration;
            return true;
        }
        if (key == "updates.auto") { autoAdvance = sampleBool(v); return true; }
        if (key == "updates.automatic") { automatic = sampleBool(v); return true; }
        return false;
    }
    std::vector<std::string> help() const override {
        return {"updates.phase=current|idle|waiting|checking|available|blocked|error|confirming|downloading|verifying|installing|restart|restart-hidden",
                "updates.pinned=0|1", "updates.running|newest|selected|target=X.Y.Z", "updates.result=available|current|error|blocked (what the next check finds)",
                "updates.total=BYTES (0: the row says Downloaded N MB, as the real service does)", "updates.fail=restart|pin|none (the next restart / pin removal fails)",
                "updates.rebuild=0|1", "updates.notes=short|empty|long", "updates.status=TEXT", "updates.paths=list|none",
                "updates.changelog=ok|loading|error|empty", "updates.automatic=0|1 (the switch)", "updates.auto=0|1 (the phases advance by themselves)"};
    }

private:
    // SystemInfo::formatBytes (src/legacy/SystemInfo.cpp:125): KB below 1 MiB, integer MB below 1 GiB, one-decimal GB.
    static std::string bytesText(uint64_t bytes) {
        char b[32];
        if (bytes >= 1024ull * 1024 * 1024) std::snprintf(b, sizeof(b), "%.1f GB", double(bytes) / (1024.0 * 1024 * 1024));
        else if (bytes >= 1024ull * 1024) std::snprintf(b, sizeof(b), "%llu MB", (unsigned long long)(bytes / (1024 * 1024)));
        else std::snprintf(b, sizeof(b), "%llu KB", (unsigned long long)(bytes / 1024));
        return b;
    }
    void finishCheck() {
        using P = UpdatePhase;
        if (result == "error") {
            info.phase = P::Error;
            info.statusText = info.errorText = "Network not ready — try Check now after Wi-Fi connects";
        } else if (result == "blocked") {
            info.phase = P::Blocked;
            info.statusText = "A required MiSTer startup file is missing. Reinstall the GUI to repair it. · latest v" + info.targetVersion;
        } else if (result == "current") {
            info.phase = P::Current;
            info.statusText = "You are using the newest release (GitHub: v" + info.selectedVersion + ")";
        } else {
            info.phase = P::Available;
            info.statusText = "New version available: v" + info.targetVersion;
            info.canInstall = true;
        }
    }
    float clock_ = 0.0f;
};

}  // namespace patsgui
