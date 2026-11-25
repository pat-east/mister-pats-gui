#pragma once

// All sample services in one object, for the host runner (src/app/host) and tests: `services()` is what the Settings
// tab receives (Services::settings), and set("system.disc", "nodrive") changes a state by its key (the prefix is
// the category id). help() lists every key. Each sample is header-only in its own file
// (src/app/settings/Sample<Category>Service.h) and belongs to that category's agent.

#include <string>
#include <vector>

#include "app/settings/SampleAccessibilityService.h"
#include "app/settings/SampleControllersService.h"
#include "app/settings/SampleDeveloperService.h"
#include "app/settings/SampleInterfaceService.h"
#include "app/settings/SampleLibraryService.h"
#include "app/settings/SampleOnboardingService.h"
#include "app/settings/SampleSystemService.h"
#include "app/settings/SampleUpdatesService.h"
#include "app/settings/SettingsServices.h"

namespace patsgui {

class SampleSettingsServices {
public:
    SampleSettingsServices();

    SampleLibraryService library;
    SampleControllersService controllers;
    SampleInterfaceService interfaceUi;
    SampleAccessibilityService accessibility;
    SampleUpdatesService updates;
    SampleSystemService system;
    SampleDeveloperService developer;
    SampleOnboardingService onboarding;

    // The aggregate the Settings tab takes. The pointers stay valid as long as this object lives.
    SettingsServices &services() { return services_; }
    // "category.name" = value. False when no service knows the key (the caller reports it).
    bool set(const std::string &key, const std::string &value);
    // Every key of every sample, one per line.
    std::vector<std::string> help() const;

private:
    SettingsServices services_;
};

}  // namespace patsgui
