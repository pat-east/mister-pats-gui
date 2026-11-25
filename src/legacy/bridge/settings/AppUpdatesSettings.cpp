#include "bridge/settings/AppUpdatesSettings.h"

#include <cerrno>
#include <cstring>
#include <sys/reboot.h>
#include <unistd.h>

#include "Preferences.h"
#include "SystemInfo.h"
#include "UpdateService.h"

// The host side of the Updates category (src/app/settings/UpdatesService.h) on Classic's objects. It adds no behavior: every
// method is the call Classic makes (src/legacy/App.cpp 275-289 and 618-633, src/legacy/SettingsScreen.cpp), through the ONE UpdateService
// that App owns (Context::settingsHost.updates). Never construct a second UpdateService: it would start a second worker
// thread with its own snapshot. The per-frame polling, the startup auto check (8 s after start) and Classic's global input
// rules (Back cancels a download on every tab, A is ignored while the install commits) stay in App.

namespace {

patsgui::UpdatePhase phaseOf(UpdatePhase phase) {   // by name: the two enums are in different orders
    using P = patsgui::UpdatePhase;
    switch (phase) {
        case UpdatePhase::Idle: return P::Idle;
        case UpdatePhase::WaitingForInternet: return P::WaitingForInternet;
        case UpdatePhase::Checking: return P::Checking;
        case UpdatePhase::Current: return P::Current;
        case UpdatePhase::Available: return P::Available;
        case UpdatePhase::Blocked: return P::Blocked;
        case UpdatePhase::Confirming: return P::Confirming;
        case UpdatePhase::Downloading: return P::Downloading;
        case UpdatePhase::Verifying: return P::Verifying;
        case UpdatePhase::Installing: return P::Installing;
        case UpdatePhase::RestartRequired: return P::RestartRequired;
        case UpdatePhase::Error: return P::Error;
    }
    return P::Idle;
}

}  // namespace

UpdateService *AppUpdatesSettings::service() const { return env_.context.settingsHost.updates; }

bool AppUpdatesSettings::available() const { return service() != nullptr; }

// Nothing to step: UpdateService has its own worker thread. (App polls and runs the startup auto check.)
void AppUpdatesSettings::update(float) {}

patsgui::UpdateInfo AppUpdatesSettings::snapshot() const {
    patsgui::UpdateInfo out;
    UpdateService *s = service();
    if (!s) return out;
    const UpdateSnapshot snap = s->snapshot();
    out.phase = phaseOf(snap.phase);
    out.targetVersion = snap.targetVersion;
    out.releaseNotes = snap.releaseNotes;
    out.statusText = snap.statusText;
    out.errorText = snap.errorText;
    out.runningVersion = snap.runningVersion;
    out.selectedVersion = snap.selectedVersion;
    out.newestInstalledVersion = snap.newestInstalledVersion;
    out.pinned = snap.pinned;
    out.repairPaths = snap.repairPaths;
    out.bytesDone = snap.bytesDone;
    out.bytesTotal = snap.bytesTotal;
    out.bytesDoneText = SystemInfo::formatBytes(snap.bytesDone);   // "Downloaded <size>" (SettingsScreen.cpp:866)
    out.databaseRebuildHint = snap.databaseRebuildHint;
    out.canInstall = snap.canInstall;
    out.successPromptVisible = snap.successPromptVisible;
    out.pinRemoved = snap.pinRemoved;
    out.changelogLoading = snap.changelogLoading;
    out.changelogGeneration = snap.changelogGeneration;
    out.changelog = snap.changelog;
    out.changelogError = snap.changelogError;
    return out;
}

bool AppUpdatesSettings::commitRunning() const {
    UpdateService *s = service();
    return s && s->commitRunning();
}

bool AppUpdatesSettings::checkAutomatically() const { return env_.context.preferences.checkForUpdates(); }

// App.cpp:275-279
void AppUpdatesSettings::setCheckAutomatically(bool on) {
    env_.context.preferences.setCheckForUpdates(on);
    if (!on)
        if (UpdateService *s = service()) s->cancelAutomaticCheck();
}

// App.cpp:280 (SettingsHost::startUpdateCheck): autoCheckDecisionMade_ = true; updateService_.startCheck(false)
bool AppUpdatesSettings::startCheck() {
    UpdateService *s = service();
    if (!s) return false;
    if (env_.context.settingsHost.startUpdateCheck) env_.context.settingsHost.startUpdateCheck();
    else s->startCheck(false);
    const UpdatePhase phase = s->snapshot().phase;
    return phase == UpdatePhase::Checking || phase == UpdatePhase::WaitingForInternet;
}

bool AppUpdatesSettings::beginConfirmation() {
    UpdateService *s = service();
    return s && s->beginConfirmation();
}

bool AppUpdatesSettings::startInstall() {
    UpdateService *s = service();
    return s && s->startInstall();
}

void AppUpdatesSettings::cancelBeforeCommit() {
    if (UpdateService *s = service()) s->cancelBeforeCommit();
}

void AppUpdatesSettings::dismissInstallSuccess() {
    if (UpdateService *s = service()) s->dismissInstallSuccess();
}

void AppUpdatesSettings::showInstallSuccess() {
    if (UpdateService *s = service()) s->showInstallSuccess();
}

// App::removePinnedVersion (App.cpp:629-633); the error text is what Classic's "Cannot remove version pin" dialog shows.
bool AppUpdatesSettings::removePinnedVersion(std::string &error) {
    UpdateService *s = service();
    if (!s) { error = "updates are not available"; return false; }
    return s->removePinnedVersion(error);
}

// App::restartMisterAfterUpdate (App.cpp:618-627): prepareRestart, sync, reboot. The error text is what Classic's
// "Cannot restart MiSTer" dialog shows. On success the device reboots; this process keeps running until it does.
bool AppUpdatesSettings::restartAfterUpdate(bool removePin, std::string &error) {
    UpdateService *s = service();
    if (!s) { error = "updates are not available"; return false; }
    if (!s->prepareRestart(removePin, error)) return false;
    ::sync();
    if (::reboot(RB_AUTOBOOT) != 0) {
        error = std::strerror(errno);
        return false;
    }
    return true;
}

// App.cpp:289 (SettingsScreen: onShowChangelog_). false only while the service shuts down.
bool AppUpdatesSettings::startChangelog() {
    UpdateService *s = service();
    return s && s->startChangelog();
}
