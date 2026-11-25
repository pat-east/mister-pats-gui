#pragma once

// The services seam of the Settings tab. One abstract interface per category (each in its own header, owned by
// that category's agent); this aggregate only holds pointers to them. A null pointer, or a service whose
// available() is false, means the host cannot do that yet: the category then behaves as the plain first level
// (values in memory, A does nothing) and never crashes.
//
// The host (src/legacy/bridge/settings/, PatsGuiScreen) creates the adapters and fills the aggregate; the host runner of the
// framework (src/app/host) fills it with the sample services (SampleSettingsServices.h). Adding a method to an
// interface changes that interface's header, its sample and its adapter, never this file.

#include "app/settings/AccessibilityService.h"
#include "app/settings/ControllersService.h"
#include "app/settings/DeveloperService.h"
#include "app/settings/InterfaceService.h"
#include "app/settings/LibraryService.h"
#include "app/settings/OnboardingService.h"
#include "app/settings/SystemService.h"
#include "app/settings/UpdatesService.h"

namespace patsgui {

struct SettingsServices {
    LibraryService *libraryService = nullptr;
    ControllersService *controllersService = nullptr;
    InterfaceService *interfaceService = nullptr;
    AccessibilityService *accessibilityService = nullptr;
    UpdatesService *updatesService = nullptr;
    SystemService *systemService = nullptr;
    DeveloperService *developerService = nullptr;
    OnboardingService *onboardingService = nullptr;   // the first-start flow (src/app/FirstRun.cpp), not a Settings category
};

// Null-safe access: the service when it exists and is available, else nullptr.
template <class S>
inline S *usable(S *service) { return service && service->available() ? service : nullptr; }

}  // namespace patsgui
