#include "bridge/settings/SettingsAdapters.h"

#include "bridge/settings/AppAccessibilitySettings.h"
#include "bridge/settings/AppControllersSettings.h"
#include "bridge/settings/AppDeveloperSettings.h"
#include "bridge/settings/AppInterfaceSettings.h"
#include "bridge/settings/AppLibrarySettings.h"
#include "bridge/settings/AppOnboardingSettings.h"
#include "bridge/settings/AppSystemSettings.h"
#include "bridge/settings/AppUpdatesSettings.h"

SettingsAdapters::SettingsAdapters(const AdapterEnv &env)
    : library_(new AppLibrarySettings(env)),
      controllers_(new AppControllersSettings(env)),
      interface_(new AppInterfaceSettings(env)),
      accessibility_(new AppAccessibilitySettings(env)),
      updates_(new AppUpdatesSettings(env)),
      system_(new AppSystemSettings(env)),
      developer_(new AppDeveloperSettings(env)),
      onboarding_(new AppOnboardingSettings(env)) {
    services_.libraryService = library_.get();
    services_.controllersService = controllers_.get();
    services_.interfaceService = interface_.get();
    services_.accessibilityService = accessibility_.get();
    services_.updatesService = updates_.get();
    services_.systemService = system_.get();
    services_.developerService = developer_.get();
    services_.onboardingService = onboarding_.get();
}

SettingsAdapters::~SettingsAdapters() = default;
