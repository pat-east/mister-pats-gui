#pragma once

// Settings > Interface > Manage Consoles: Classic's SystemVisibilityScreen (src/legacy/SystemVisibilityScreen.cpp). A plain
// scrolling list of every system that has games, sorted by name, each on or off; A shows or hides the console at once
// (HiddenSystems::toggle writes hidden-systems.txt), B closes. Hidden consoles are skipped by the Consoles tab.
//
// A Content page (it replaces the three panes, the tabs are locked: Classic disables tab switching while it is open).
// The list is read once when the page opens (Classic: refresh() on open) and not rebuilt while it is open.

#include <memory>

#include "app/settings/InterfaceService.h"
#include "app/settings/SettingsPage.h"

namespace patsgui {

std::unique_ptr<SettingsPage> makeManageConsolesPage(InterfaceService &service);

}  // namespace patsgui
