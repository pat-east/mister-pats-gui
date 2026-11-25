#pragma once

// Sample Onboarding service for the host runner and tests: the flag in memory, calls counted.

#include "app/settings/OnboardingService.h"
#include "app/settings/SampleControl.h"

namespace patsgui {

class SampleOnboardingService : public OnboardingService, public SampleControl {
public:
    bool done = false;
    int completes = 0;   // calls of setComplete(): a test checks that it is exactly one at the end of a visit and none otherwise

    bool available() const override { return true; }
    bool complete() const override { return done; }
    void setComplete() override { done = true; ++completes; }

    bool set(const std::string &key, const std::string &v) override {
        if (key == "onboarding.complete") { done = sampleBool(v); return true; }
        return false;
    }
    std::vector<std::string> help() const override { return {"onboarding.complete=0|1"}; }
};

}  // namespace patsgui
