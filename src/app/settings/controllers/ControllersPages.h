#pragma once

// The pages of the Controllers category: the port of Classic's ControllersScreen modes (src/legacy/ControllersScreen.*):
// List, InputTest, Deadzone (SettingsPages) and the mapping wizard and Bluetooth pairing (WizardFlowPage flows) on the Settings tab's
// PageStack; the list is the base and the others are pushed over it, so B returns to the list exactly as Classic's
// modes return to Mode::List. All I/O goes through ControllersService; the rules live in PadLogic.

#include <memory>
#include <string>

#include "app/settings/ControllersService.h"
#include "app/settings/SettingsPage.h"

namespace patsgui {
namespace controllers {

// The list's cursor, shared with the pages above it: Classic sets it after a refresh (back to the first row after
// Bluetooth or a lost device, onto the paired pad before its wizard).
struct ListFlow {
    int cursor = 0;
};

// Registers every text of the pages (settings.controllers.*): called by ControllersSettings' strings function.
void registerPageStrings(ui::Catalog &c);

// What A on the Controllers row does (App::openControllers): refreshes the list and shows it.
std::unique_ptr<SettingsPage> makeListPage(ControllersService &service, std::shared_ptr<ListFlow> flow);
std::unique_ptr<SettingsPage> makeInputTestPage(ControllersService &service, std::shared_ptr<ListFlow> flow, const ControllerInfo &device);
std::unique_ptr<SettingsPage> makeDeadzonePage(ControllersService &service, std::shared_ptr<ListFlow> flow, const ControllerInfo &device);

// The two sequences are wizard flows (ControllersFlows.cpp): one Medium dialog each, over the list.
void openMappingFlow(ControllersService &service, std::shared_ptr<ListFlow> flow, const ControllerInfo &device, PageStack &pages);
void openBluetoothFlow(ControllersService &service, std::shared_ptr<ListFlow> flow, PageStack &pages);
void registerFlowStrings(ui::Catalog &c);

}  // namespace controllers
}  // namespace patsgui
