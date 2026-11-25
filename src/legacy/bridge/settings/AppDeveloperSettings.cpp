#include "bridge/settings/AppDeveloperSettings.h"

// available() when the host has the Wireframes hook; Classic has no build flag (Developer is unconditional), so
// developmentBuild() is always true.

bool AppDeveloperSettings::available() const { return bool(env_.context.settingsHost.openWireframes); }
bool AppDeveloperSettings::developmentBuild() const { return true; }
void AppDeveloperSettings::openWireframes() {
    if (env_.context.settingsHost.openWireframes) env_.context.settingsHost.openWireframes();
}
