#pragma once

// Accessibility category: font, text and hint size, high contrast.
// Table, behavior and pages of this category only; see src/app/settings/PORTING.md. Classic source:
// src/legacy/SettingsScreen.cpp buildRows() (Accessibility rows, 462-511) and update() (1037-1041), App::activateSpaceGrotesk()
// and spaceGroteskStatus(). There is no page, dialog or flow: every row acts at once.

#include "app/settings/SettingsCategory.h"

namespace patsgui {

class AccessibilitySettings : public SettingsCategory {
public:
    const char *id() const override { return "accessibility"; }
    std::vector<VisibleRow> rows(const MenuState &state) const override;
    int current(const std::string &rowId) const override;
    bool activate(const std::string &rowId, PageStack &pages) override;
    void changed(const std::string &rowId, int value) override;
    bool update(float deltaSeconds) override;

private:
    AccessibilityService *service() const;
};

}  // namespace patsgui
