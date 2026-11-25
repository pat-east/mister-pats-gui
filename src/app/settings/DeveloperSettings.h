#pragma once

// Developer category: the development tool of the Classic GUI that stays in the new one, "Wireframes" (the framework
// demo). Table, behavior and pages of this category only; see src/app/settings/PORTING.md. Classic source:
// src/legacy/SettingsScreen.cpp buildRows() (Developer rows), App::openWireframes(). The row that opens the new GUI is not
// ported (it is redundant inside the new GUI).

#include "app/settings/SettingsCategory.h"

namespace patsgui {

class DeveloperSettings : public SettingsCategory {
public:
    const char *id() const override { return "developer"; }
    std::vector<VisibleRow> rows(const MenuState &state) const override;
    // Classic shows Developer unconditionally (SettingsScreen::buildRows), so shown() keeps its default (true). To hide
    // it in release builds (owner decision D44), return state.devBuild here.
    bool activate(const std::string &rowId, PageStack &pages) override;

private:
    DeveloperService *service() const;
};

}  // namespace patsgui
