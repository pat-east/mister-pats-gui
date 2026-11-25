#pragma once

// Library category: game list, box art, core and MRA updates, TheGamesDB, ScreenScraper.fr.
// Table, behavior and pages of this category only; see src/app/settings/PORTING.md. Classic source:
// src/legacy/SettingsScreen.cpp buildRows() (Library rows), openCoreUpdater()/handleCoreUpdater()/renderCoreUpdater(), openAccountEntry()/handleAccountEntry(); src/legacy/ScanScreen.*, src/legacy/ArcadeGamesScreen.*.

#include "app/settings/SettingsCategory.h"

namespace patsgui {

class LibrarySettings : public SettingsCategory {
public:
    const char *id() const override { return "library"; }
    std::vector<VisibleRow> rows(const MenuState &state) const override;
    int current(const std::string &rowId) const override;
    void changed(const std::string &rowId, int value) override;
    bool activate(const std::string &rowId, PageStack &pages) override;
    bool update(float deltaSeconds) override;
    std::vector<std::string> pageIds() const override;
    bool openPage(const std::string &pageId, PageStack &pages) override;

private:
    TheGamesDbStatus::State previousTgdb_ = TheGamesDbStatus::State::NotChecked;
    LibraryService *service() const { return usable(env().services.libraryService); }
};

}  // namespace patsgui
