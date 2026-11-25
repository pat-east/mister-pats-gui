#pragma once

// System category: disc drive and cores, info, clock, time zone, screensaver, backup, icons, Wi-Fi, reboot, changelog.
// Table, behavior and pages of this category only; see src/app/settings/PORTING.md. Classic source:
// src/legacy/SettingsScreen.cpp buildRows() (System rows), facts(), renderInfo(), renderDiscDriveSettings(), renderDiscCoreInstallConfirmation(), startWifiSetup()/handleWifiFlow()/renderWifiFlow(), handleTimeZoneChooser()/renderTimeZoneChooser(), renderChangelog(); src/legacy/App.cpp (rebootMister, startMisterCore, startAssetDownload, the backup report).
// The small flows (time zone, backup, icons, Disc cores, reboot) are in system/*Flow.cpp (SystemFlows.h).
// The Wi-Fi flow is in system/WifiFlow.*.

#include "app/settings/SettingsCategory.h"

namespace patsgui {

class SystemSettings : public SettingsCategory {
public:
    const char *id() const override { return "system"; }
    std::vector<VisibleRow> rows(const MenuState &state) const override;
    int current(const std::string &rowId) const override;
    void changed(const std::string &rowId, int value) override;
    bool activate(const std::string &rowId, PageStack &pages) override;
    bool update(float deltaSeconds) override;
    void onShow() override;
    std::vector<std::string> pageIds() const override;
    bool openPage(const std::string &pageId, PageStack &pages) override;

private:
    SystemService *service() const { return usable(env().services.systemService); }
    bool refresh(bool force);   // copies the service state into MenuState; true when something changed
    void reportBackup(SystemService *s);
    void openDiscPage(PageStack &pages);
    void openInfoPage(PageStack &pages);

    std::shared_ptr<bool> backupReporting_ = std::make_shared<bool>(false);   // the backup flow shows its own result
};

}  // namespace patsgui
