#include "app/settings/ControllersSettings.h"

#include "app/settings/controllers/ControllersPages.h"

namespace patsgui {
namespace {

using namespace menu;

PATSGUI_OPTIONS(kFaceLabels, {"settings.opt.xbox", "XBOX"}, {"settings.opt.playstation", "PlayStation"},
     {"settings.opt.nintendo", "Nintendo"});

// Classic's cycle code (SS:339-345): xbox -> playstation -> nintendo -> xbox, stored as these Preferences strings.
const char *const kStyles[] = {"xbox", "playstation", "nintendo"};

// The Classic rows of this category, in Classic's order (src/legacy/SettingsScreen.cpp buildRows()).
const MenuCategory &table() {
    static const MenuCategory category = [] {
        MenuCategory controllers{"controllers", "Controllers", Cond::Always, {}};
        controllers.rows = {
            action("controllers.controllers", "Controllers",
                   "Test connected controllers, change how far a stick must move, or set up button controls.",
                   "Classic: Controllers screen (onManageControllers): list, input test, deadzone, button mapping, Bluetooth"),
            cycle("controllers.faces", "Face-button labels",
                  "Choose the A, B, X and Y labels that match your controller. This changes labels only.",
                  kFaceLabels, PATSGUI_COUNT(kFaceLabels)),
        };
        return controllers;
    }();
    return category;
}

void strings(ui::Catalog &c) {
    registerTable(c, table());
    controllers::registerPageStrings(c);
}

}  // namespace

ControllersSettings::ControllersSettings() : flow_(std::make_shared<controllers::ListFlow>()) {}
ControllersSettings::~ControllersSettings() = default;

std::vector<VisibleRow> ControllersSettings::rows(const MenuState &state) const { return visibleRows(table(), state); }

// The real value of "Face-button labels": the index of Preferences::faceButtonStyle().
int ControllersSettings::current(const std::string &rowId) const {
    ControllersService *s = service();
    if (!s || rowId != "controllers.faces") return kNoValue;
    const std::string style = s->faceButtonStyle();
    for (int i = 0; i < 3; ++i)
        if (style == kStyles[i]) return i;
    return 0;   // Classic shows XBOX for anything else
}

// The same setters Classic calls: Preferences::setFaceButtonStyle and ButtonLabels' setFaceButtonStyle (in the service).
void ControllersSettings::changed(const std::string &rowId, int value) {
    ControllersService *s = service();
    if (s && rowId == "controllers.faces" && value >= 0 && value < 3) s->setFaceButtonStyle(kStyles[value]);
}

// A on "Controllers": App::openControllers() = refresh the list, show it with the cursor on the first row.
bool ControllersSettings::activate(const std::string &rowId, PageStack &pages) {
    if (rowId == "controllers.controllers") return openPage("list", pages);
    return false;
}

bool ControllersSettings::update(float deltaSeconds) {
    if (ControllersService *s = service()) s->update(deltaSeconds);
    return false;
}

// Leaving the tab or closing the GUI closes the pages; make sure nothing stays captured or scanning.
void ControllersSettings::onHide() {
    if (ControllersService *s = service()) {
        s->setRawInputWanted(false);
        s->cancelBluetoothScan();
    }
}

std::vector<std::string> ControllersSettings::pageIds() const {
    return {"list", "test", "deadzone", "wizard", "bluetooth"};
}

// Every page opens over the list, as in Classic (the modes return to Mode::List); "list" is the screen itself.
bool ControllersSettings::openPage(const std::string &pageId, PageStack &pages) {
    ControllersService *s = service();
    if (!s) return false;
    flow_->cursor = 0;
    s->refreshControllers();
    pages.push(controllers::makeListPage(*s, flow_));
    if (pageId == "list") return true;
    const std::vector<ControllerInfo> &devices = s->controllers();
    if (pageId == "bluetooth") {
        controllers::openBluetoothFlow(*s, flow_, pages);
        return true;
    }
    if (devices.empty()) return true;   // no pad to test: the list says so
    if (pageId == "test") pages.push(controllers::makeInputTestPage(*s, flow_, devices[0]));
    else if (pageId == "deadzone") pages.push(controllers::makeDeadzonePage(*s, flow_, devices[0]));
    else if (pageId == "wizard") controllers::openMappingFlow(*s, flow_, devices[0], pages);
    else return true;
    return true;
}

PATSGUI_REGISTER_SETTINGS_CATEGORY(ControllersSettings, strings);

}  // namespace patsgui
