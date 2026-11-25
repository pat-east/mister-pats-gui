#include "app/settings/SampleSettingsServices.h"

namespace patsgui {

SampleSettingsServices::SampleSettingsServices() {
    services_.libraryService = &library;
    services_.controllersService = &controllers;
    services_.interfaceService = &interfaceUi;
    services_.accessibilityService = &accessibility;
    services_.updatesService = &updates;
    services_.systemService = &system;
    services_.developerService = &developer;
    services_.onboardingService = &onboarding;
}

bool SampleSettingsServices::set(const std::string &key, const std::string &value) {
    SampleControl *all[] = {&library, &controllers, &interfaceUi, &accessibility, &updates, &system, &developer, &onboarding};
    for (SampleControl *c : all)
        if (c->set(key, value)) return true;
    return false;
}

std::vector<std::string> SampleSettingsServices::help() const {
    std::vector<std::string> lines;
    const SampleControl *all[] = {&library, &controllers, &interfaceUi, &accessibility, &updates, &system, &developer, &onboarding};
    for (const SampleControl *c : all)
        for (const std::string &line : c->help()) lines.push_back(line);
    return lines;
}

}  // namespace patsgui
