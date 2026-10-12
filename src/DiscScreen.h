#pragma once

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
private:
    Context &context_;
    DiscCoreManager &cores_;
    std::function<void(const std::string &)> launch_;
    DiscCoreManager::Status coreStatus_;
    DiscArtworkService::Snapshot artStatus_;
    float refreshTimer_ = 0;
    int cursor_ = 0;
    int overlayIndex_ = 0;
    bool screenshotOverlay_ = false;
};
