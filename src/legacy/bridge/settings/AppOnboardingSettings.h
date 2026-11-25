#pragma once

// Host side of the Onboarding service (src/app/settings/OnboardingService.h): the first-use guide's flag in Preferences.
//   Classic source: src/legacy/App.cpp (onOnboardingComplete_ = preferences_.setOnboardingComplete(true)), src/legacy/Preferences.h
//   Needs: Context::preferences.

#include "app/settings/OnboardingService.h"
#include "bridge/settings/AdapterEnv.h"

class AppOnboardingSettings : public patsgui::OnboardingService {
public:
    explicit AppOnboardingSettings(const AdapterEnv &env) : env_(env) {}
    bool available() const override { return true; }
    bool complete() const override { return env_.context.preferences.onboardingComplete(); }
    void setComplete() override { env_.context.preferences.setOnboardingComplete(true); }

private:
    AdapterEnv env_;
};
