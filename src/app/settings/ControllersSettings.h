#pragma once

// Controllers category: the controller list with the input test, the stick deadzone, the button-mapping wizard and
// Bluetooth pairing (Classic: ControllersScreen), and the face-button labels. Table, behavior and pages of this
// category only; see src/app/settings/PORTING.md. Classic source: src/legacy/SettingsScreen.cpp buildRows() (Controllers rows),
// src/legacy/ControllersScreen.*, src/legacy/ButtonLabels.*; the parity table is in src/app/PARITY-settings.md.

#include <memory>

#include "app/settings/SettingsCategory.h"

namespace patsgui {

namespace controllers { struct ListFlow; }

class ControllersSettings : public SettingsCategory {
public:
    ControllersSettings();
    ~ControllersSettings() override;

    const char *id() const override { return "controllers"; }
    std::vector<VisibleRow> rows(const MenuState &state) const override;
    int current(const std::string &rowId) const override;
    void changed(const std::string &rowId, int value) override;
    bool activate(const std::string &rowId, PageStack &pages) override;
    bool update(float deltaSeconds) override;
    void onHide() override;
    std::vector<std::string> pageIds() const override;
    bool openPage(const std::string &pageId, PageStack &pages) override;

private:
    ControllersService *service() const { return usable(env().services.controllersService); }
    std::shared_ptr<controllers::ListFlow> flow_;
};

}  // namespace patsgui
