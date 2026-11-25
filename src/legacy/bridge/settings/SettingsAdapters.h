#pragma once

#include <memory>

#include "app/settings/SettingsServices.h"
#include "bridge/settings/AdapterEnv.h"

class AppLibrarySettings;
class AppControllersSettings;
class AppInterfaceSettings;
class AppAccessibilitySettings;
class AppUpdatesSettings;
class AppSystemSettings;
class AppDeveloperSettings;
class AppOnboardingSettings;

// The host side of the Settings tab's services: one adapter per category on top of the Classic objects, and the
// aggregate the tab receives (src/app/settings/SettingsServices.h). Created by PatsGuiScreen::open(). This file never
// changes when an adapter grows: each adapter has the same constructor, AppXSettings(const AdapterEnv &).
class SettingsAdapters {
public:
    explicit SettingsAdapters(const AdapterEnv &env);
    ~SettingsAdapters();
    patsgui::SettingsServices &services() { return services_; }

private:
    std::unique_ptr<AppLibrarySettings> library_;
    std::unique_ptr<AppControllersSettings> controllers_;
    std::unique_ptr<AppInterfaceSettings> interface_;
    std::unique_ptr<AppAccessibilitySettings> accessibility_;
    std::unique_ptr<AppUpdatesSettings> updates_;
    std::unique_ptr<AppSystemSettings> system_;
    std::unique_ptr<AppDeveloperSettings> developer_;
    std::unique_ptr<AppOnboardingSettings> onboarding_;
    patsgui::SettingsServices services_;
};
