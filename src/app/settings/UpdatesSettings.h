#pragma once

// Updates category: automatic check, installed versions, check, install, restart. A 1:1 port of Classic's Updates
// rows and update flow (src/app/PARITY-settings.md, "Updates"). Classic source: src/legacy/SettingsScreen.cpp buildRows() 625-628,
// buildUpdateRows() 833-891, setUpdateSnapshot() 893-919, handle() 1131-1151 (the two modals), renderUpdateConfirmation(),
// renderUpdateSuccess(), hints() 2813-2824; src/legacy/App.cpp (update callbacks 275-289, removePinnedVersion(), restartMisterAfterUpdate(),
// dispatch() 984-1013, run() 1481-1494); src/legacy/UpdateService.*, UpdateInstaller.*, UpdateFiles.*, VerifiedDownloader.*, ReleaseInfo.*.
// The update itself (downloads, verification, the library switch) stays in src/legacy/ behind UpdatesService.

#include "app/settings/SettingsCategory.h"
#include "app/settings/UpdatesService.h"
#include "app/settings/updates/UpdatePages.h"

namespace patsgui {

class UpdatesSettings : public SettingsCategory {
public:
    const char *id() const override { return "updates"; }
    std::vector<VisibleRow> rows(const MenuState &state) const override;
    ui::Text valueOf(const VisibleRow &row, const MenuState &state, int current) const override;
    int current(const std::string &rowId) const override;
    void changed(const std::string &rowId, int value) override;
    std::string backActivates() const override;
    bool activate(const std::string &rowId, PageStack &pages) override;
    bool update(float deltaSeconds) override;
    bool background(float deltaSeconds) override;
    bool wantsAttention() const override;
    ui::Text footerNote() const override;
    void onShow() override;
    bool hintsHook(std::vector<ui::Hint> &hints) const override;
    std::vector<std::string> pageIds() const override { return {"confirm", "busy", "restart", "error.restart", "error.pin", "changelog"}; }
    bool openPage(const std::string &pageId, PageStack &pages) override;

private:
    UpdatesService *service() const { return usable(env().services.updatesService); }
    bool refresh(float deltaSeconds);                 // poll the service into info_ and MenuState; true when something changed
    bool syncPages(PageStack &pages);                 // open or forget the two snapshot-driven dialogs
    UpdatePageEnv pageEnv(bool forced, bool *openFlag);

    UpdateInfo info_;                  // the snapshot of the last poll (the dialogs read it)
    bool confirmOpen_ = false;         // the update flow is open
};

}  // namespace patsgui
