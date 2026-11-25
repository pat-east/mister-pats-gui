#pragma once

// Interface category: tabs, theme, sounds, game view, Manage Consoles.
// Table, behavior and pages of this category only; see src/app/settings/PORTING.md. Classic source:
// src/legacy/SettingsScreen.cpp buildRows() (Interface rows), App::toggleGamesTab()/toggleArcadeTab()/cycleDefaultView();
// src/legacy/SystemVisibilityScreen.*. The port notes are in src/app/PARITY-settings.md ("Interface").

#include "app/settings/SettingsCategory.h"

namespace patsgui {

class InterfaceSettings : public SettingsCategory {
public:
    const char *id() const override { return "interface"; }
    std::vector<VisibleRow> rows(const MenuState &state) const override;
    int current(const std::string &rowId) const override;
    bool activate(const std::string &rowId, PageStack &pages) override;
    void changed(const std::string &rowId, int value) override;
    std::vector<std::string> pageIds() const override { return {"consoles"}; }
    bool openPage(const std::string &pageId, PageStack &pages) override;

private:
    InterfaceService *service() const;
};

}  // namespace patsgui
