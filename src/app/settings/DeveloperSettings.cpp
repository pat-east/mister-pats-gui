#include "app/settings/DeveloperSettings.h"

#include "app/settings/StandardPages.h"

namespace patsgui {
namespace {

using namespace menu;


// The Classic rows of this category, in Classic's order (src/legacy/SettingsScreen.cpp buildRows() 639-643). Classic has two:
// "Wireframes" and "MiSTer Pats GUI (UI-Framework)". The second opens this very GUI from Classic, so inside the new GUI it
// does not exist (opening the GUI from itself would rebuild it under its own feet): not in the table, not in the service.
// Classic pushes the category unconditionally and both rows are always enabled; the same here (Cond::Always).
const MenuCategory &table() {
    static const MenuCategory category = [] {
        MenuCategory developer{"developer", "Developer", Cond::Always, {}};
        developer.rows = {
            action("developer.wireframes", "Wireframes",
                   "Opens a demonstration of the new UI framework: a splash screen, the main-screen layout with tabs, system items and help, and example screens that show the framework's components. It is a development tool and changes nothing on the device. Back leaves it.",
                   "Classic: opens the framework demo (onOpenWireframes -> App::openWireframes)"),
        };
        return developer;
    }();
    return category;
}

void strings(ui::Catalog &c) {
    registerTable(c, table());
}

}  // namespace

std::vector<VisibleRow> DeveloperSettings::rows(const MenuState &state) const { return visibleRows(table(), state); }

DeveloperService *DeveloperSettings::service() const {
    return attached() ? usable(env().services.developerService) : nullptr;
}

// A on "Wireframes": the host opens the framework demo as a takeover over the new GUI; B in the demo returns here.
// Handled (true) also without a service: the row then simply does nothing, it never falls through to the empty seam.
bool DeveloperSettings::activate(const std::string &rowId, PageStack &pages) {
    (void)pages;
    if (rowId == "developer.wireframes") {
        if (DeveloperService *s = service()) s->openWireframes();
        return true;
    }
    return false;
}

PATSGUI_REGISTER_SETTINGS_CATEGORY(DeveloperSettings, strings);

}  // namespace patsgui
