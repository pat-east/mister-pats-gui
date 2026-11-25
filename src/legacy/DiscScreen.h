#pragma once

#include <chrono>

#include "DiscCoreManager.h"
#include "Screen.h"

class DiscScreen : public Screen {
public:
    DiscScreen(Context &context, DiscCoreManager &cores, std::function<void(const std::string &)> launch);
    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;
    bool hasBackOverlay() const override { return screenshotOverlay_; }
    void cancelPendingLaunch() { launchProbePending_ = false; }
private:
    Context &context_;
    DiscCoreManager &cores_;
    std::function<void(const std::string &)> launch_;
    DiscCoreManager::Status coreStatus_;
    DiscArtworkService::Snapshot artStatus_;
    int cursor_ = 0;
    int overlayIndex_ = 0;
    bool screenshotOverlay_ = false;
    bool hasDiscGeneration_ = false;
    uint64_t viewedDiscGeneration_ = 0;
    bool launchProbePending_ = false;
    uint64_t launchProbeGeneration_ = 0;
    uint64_t launchCoreRevision_ = 0;
    std::chrono::steady_clock::time_point launchProbeStarted_{};
};
