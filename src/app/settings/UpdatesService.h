#pragma once

// What the Updates category (and System's "Read full changelog") need from the host (Classic: UpdateService and its
// UpdateSnapshot, Preferences::checkForUpdates(), App::removePinnedVersion(), App::restartMisterAfterUpdate()).
// Owned and extended by the Updates agent. System's changelog row reads changelog() and calls startChangelog():
// keep those two signatures stable.

#include <cstdint>
#include <string>
#include <vector>

#include "app/SettingsMenu.h"   // UpdatePhase

namespace patsgui {

// Mirrors Classic's UpdateSnapshot field for field.
struct UpdateInfo {
    UpdatePhase phase = UpdatePhase::Idle;
    std::string targetVersion, releaseNotes, statusText, errorText;
    std::string runningVersion, selectedVersion, newestInstalledVersion;
    bool pinned = false;
    std::vector<std::string> repairPaths;
    uint64_t bytesDone = 0, bytesTotal = 0;
    std::string bytesDoneText;     // SystemInfo::formatBytes(bytesDone): "12 MB" (the host formats; the progress row says "Downloaded 12 MB")
    bool databaseRebuildHint = false;
    bool canInstall = false;
    bool successPromptVisible = false;
    bool pinRemoved = false;
    bool changelogLoading = false;
    uint32_t changelogGeneration = 0;
    std::string changelog, changelogError;
};

class UpdatesService {
public:
    virtual ~UpdatesService() = default;
    virtual bool available() const = 0;
    // The host calls this once per frame while the Settings tab is shown (Classic: App::update polls the
    // service every frame; the screen receives the snapshot through setUpdateSnapshot()).
    virtual void update(float deltaSeconds) = 0;

    virtual UpdateInfo snapshot() const = 0;                  // UpdateService::snapshot()
    virtual bool commitRunning() const = 0;

    // ---- Check for updates automatically (Classic: Preferences + UpdateService::cancelAutomaticCheck() when turned off)
    virtual bool checkAutomatically() const = 0;
    virtual void setCheckAutomatically(bool on) = 0;

    // ---- Commands (Classic: SettingsScreen callbacks in App.cpp)
    virtual bool startCheck() = 0;                            // App: autoCheckDecisionMade_ = true; startCheck(false)
    virtual bool beginConfirmation() = 0;
    virtual bool startInstall() = 0;
    virtual void cancelBeforeCommit() = 0;
    virtual void dismissInstallSuccess() = 0;
    virtual void showInstallSuccess() = 0;
    virtual bool removePinnedVersion(std::string &error) = 0; // App::removePinnedVersion(): an error is shown as the "Cannot remove version pin" dialog
    virtual bool restartAfterUpdate(bool removePin, std::string &error) = 0;   // App::restartMisterAfterUpdate(): prepareRestart + sync + reboot
    virtual bool startChangelog() = 0;                        // false: another online task is running
};

}  // namespace patsgui
