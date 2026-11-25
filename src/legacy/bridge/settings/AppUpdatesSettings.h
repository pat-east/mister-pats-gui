#pragma once

// Host side of the Updates service: implements src/app/settings/UpdatesService.h on top of the Classic objects.
// Every method is the call Classic makes (src/legacy/App.cpp 275-289, 618-633; src/legacy/SettingsScreen.cpp), on the ONE UpdateService
// that App owns (Context::settingsHost.updates). available() is false until the host fills it.
//   Needs: Context: preferences (checkForUpdates); SettingsHost: updates (UpdateService), startUpdateCheck; SystemInfo::formatBytes

#include "app/settings/UpdatesService.h"
#include "bridge/settings/AdapterEnv.h"

class AppUpdatesSettings : public patsgui::UpdatesService {
public:
    explicit AppUpdatesSettings(const AdapterEnv &env) : env_(env) {}

    bool available() const override;
    void update(float deltaSeconds) override;
    patsgui::UpdateInfo snapshot() const override;
    bool commitRunning() const override;
    bool checkAutomatically() const override;
    void setCheckAutomatically(bool on) override;
    bool startCheck() override;
    bool beginConfirmation() override;
    bool startInstall() override;
    void cancelBeforeCommit() override;
    void dismissInstallSuccess() override;
    void showInstallSuccess() override;
    bool removePinnedVersion(std::string &error) override;
    bool restartAfterUpdate(bool removePin, std::string &error) override;
    bool startChangelog() override;

private:
    UpdateService *service() const;   // Context::settingsHost.updates (App::updateService_)
    AdapterEnv env_;
};
