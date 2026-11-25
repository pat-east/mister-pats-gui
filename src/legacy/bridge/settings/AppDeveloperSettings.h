#pragma once

// Host side of the Developer service: implements src/app/settings/DeveloperService.h on top of the Classic objects.
//   Classic source: src/legacy/SettingsScreen.cpp buildRows() Developer rows; src/legacy/App.cpp openWireframes()
//   Needs: Context::settingsHost.openWireframes (App::openWireframes). Classic's "MiSTer Pats GUI" row is not ported.

#include "app/settings/DeveloperService.h"
#include "bridge/settings/AdapterEnv.h"

class AppDeveloperSettings : public patsgui::DeveloperService {
public:
    explicit AppDeveloperSettings(const AdapterEnv &env) : env_(env) {}

    bool available() const override;
    bool developmentBuild() const override;
    void openWireframes() override;

private:
    AdapterEnv env_;
};
