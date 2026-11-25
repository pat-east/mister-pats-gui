#pragma once

// What the first-start flow (src/app/FirstRun.cpp) needs of the host beyond the Library service: whether the first-use
// guide is finished. Classic: Preferences::onboardingComplete() / setOnboardingComplete(). Classic's App marks the guide as
// started before it opens it (Preferences::setOnboardingStarted), the host does that itself; the flow only completes it.

namespace patsgui {

class OnboardingService {
public:
    virtual ~OnboardingService() = default;
    virtual bool available() const = 0;
    // True once the guide was finished or skipped; it is not shown again.
    virtual bool complete() const = 0;
    // The guide ended by the user (Welcome or Jump Start skipped, Jump Start finished, the failed scan skipped). Not called when
    // the GUI closes in the middle: the guide then starts again at the next start (Classic).
    virtual void setComplete() = 0;
};

}  // namespace patsgui
