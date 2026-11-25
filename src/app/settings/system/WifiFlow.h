#pragma once

// System -> Connect to Wi-Fi…: the Classic wizard (SettingsScreen::startWifiSetup / handleWifiFlow / renderWifiFlow) as a
// Wizard (WizardFlowPage, one Medium frame). The flow's data lives in one shared object that the pages' lambdas capture; the
// steps are Scan, Networks, Ssid, Password, Country, Prompt, Testing, Review and the Result pages Failed, SaveFailed,
// ScanFailed, Done.
// See src/app/PARITY-settings.md (System) for the step-by-step comparison with Classic.

#include <memory>
#include <string>

#include "app/settings/SettingsCategory.h"

namespace patsgui {

class SystemService;

// A on "Connect to Wi-Fi…": starts a scan; opens the flow (or toasts the error and stays), as Classic's startWifiSetup().
bool openWifiFlow(SystemService &service, PageStack &pages);

// The host runner opens one step directly (`--page system/wifi.review`). Ids: networks password ssid country prompt
// testing review failed (without the "wifi." prefix), also scanning savefailed scanfailed done. Returns false for an unknown id.
bool openWifiStepForTest(SystemService &service, PageStack &pages, const std::string &step);

void registerWifiStrings(ui::Catalog &c);

}  // namespace patsgui
