#pragma once

// Sample Accessibility service for the host runner and tests. It repeats what the device does, with the same words:
//   Classic App::spaceGroteskStatus() / activateSpaceGrotesk() and the completion block of App::run(), over
//   SpaceGroteskDownload's state machine (Bold, Medium, licence; "Downloading n/m"; the first frame only shows the
//   status; a failure stops the run and leaves the row at "Try font download again").
// Sizes and contrast are kept in memory; the host runner applies them to the framework theme (src/app/host/main.cpp), as the
// device's look bridge does from Preferences.
//
//   --set accessibility.font=on|off|missing|downloading|failed|failing|nodir
//   --set accessibility.auto=0|1        the download advances by itself (default 1, one file per second) or by step
//   --set accessibility.step=1          one download step (with auto=0)
//   --set accessibility.fontsize=100|115|130   hintsize=...   contrast=on|off   contrastcolor=0..7

#include <cstdlib>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "app/settings/AccessibilityService.h"
#include "app/settings/SampleControl.h"

namespace patsgui {

class SampleAccessibilityService : public AccessibilityService, public SampleControl {
public:
    bool installed = true, enabled = true, contrast = false;
    int fontScale = 100, hintScale = 100, contrastColor = 2;
    bool autoRun = true;

    bool available() const override { return true; }

    void update(float dt) override {
        if (!running_ || !autoRun) return;
        timer_ += dt;
        while (running_ && timer_ >= 1.0f) { timer_ -= 1.0f; step(); }
    }
    bool fontInstalled() const override { return installed; }
    bool fontEnabled() const override { return enabled; }

    // App::spaceGroteskStatus()
    std::string fontStatus() const override {
        if (running_) return "Downloading " + std::to_string(next_) + "/" + std::to_string(total_);
        if (!installed) return failedState_ ? "Try font download again" : "Download font";
        return enabled ? "On" : "Off";
    }

    FontDownloadInfo fontDownload() const override {
        FontDownloadInfo i;
        i.running = running_;
        i.failed = failedState_ && !installed;
        if (i.failed) i.error = "could not install SpaceGrotesk-Bold.ttf: download failed (tried curl, curl -k, wget)";
        return i;
    }

    // App::activateSpaceGrotesk()
    void activateFont() override {
        if (running_) { notices_.push_back({fontStatus(), 3.5f}); return; }
        if (installed) {
            enabled = !enabled;
            notices_.push_back({enabled ? "Space Grotesk font enabled" : "Space Grotesk font disabled", 3.5f});
            return;
        }
        if (noDirectory_) {   // SpaceGroteskDownload::start() could not create the font directory
            failedState_ = true;
            notices_.push_back({"cannot create the font directory", 6.0f});
            return;
        }
        running_ = true;
        next_ = 0;
        total_ = 3;
        timer_ = 0.0f;
        showStartedFrame_ = true;
        notices_.push_back({"Downloading the Space Grotesk font\xE2\x80\xA6", 4.0f});
    }

    bool takeNotice(std::string &text, float &seconds) override {
        if (notices_.empty()) return false;
        text = notices_.front().first;
        seconds = notices_.front().second;
        notices_.pop_front();
        return true;
    }

    int fontSizeScale() const override { return fontScale; }
    void setFontSizeScale(int p) override { fontScale = p; }
    int hintSizeScale() const override { return hintScale; }
    void setHintSizeScale(int p) override { hintScale = p; }
    bool highContrast() const override { return contrast; }
    void setHighContrast(bool on) override { contrast = on; }
    int highContrastColor() const override { return contrastColor; }
    void setHighContrastColor(int i) override { contrastColor = i; }

    bool set(const std::string &key, const std::string &v) override {
        if (key == "accessibility.font") {
            running_ = false;
            failedState_ = false;
            noDirectory_ = false;
            failing_ = false;
            installed = v == "on" || v == "off";
            enabled = v != "off";
            if (v == "downloading") { running_ = true; next_ = 0; total_ = 3; timer_ = 0.0f; showStartedFrame_ = false; }
            if (v == "failed") failedState_ = true;
            if (v == "failing") failing_ = true;     // missing; the next download fails on its first file
            if (v == "nodir") noDirectory_ = true;   // missing; the font directory cannot be created
            return v == "on" || v == "off" || v == "missing" || v == "downloading" || v == "failed" || v == "failing" ||
                   v == "nodir";
        }
        if (key == "accessibility.auto") { autoRun = sampleBool(v); return true; }
        if (key == "accessibility.step") { if (running_) step(); return true; }
        if (key == "accessibility.fontsize") { fontScale = std::atoi(v.c_str()); return true; }
        if (key == "accessibility.hintsize") { hintScale = std::atoi(v.c_str()); return true; }
        if (key == "accessibility.contrast") { contrast = sampleBool(v); return true; }
        if (key == "accessibility.contrastcolor") { contrastColor = std::atoi(v.c_str()); return true; }
        return false;
    }
    std::vector<std::string> help() const override {
        return {"accessibility.font=on|off|missing|downloading|failed|failing|nodir",
                "accessibility.auto=1|0   accessibility.step=1",
                "accessibility.fontsize=100|115|130   accessibility.hintsize=100|115|130",
                "accessibility.contrast=on|off   accessibility.contrastcolor=0..7"};
    }

private:
    // SpaceGroteskDownload::step() and App::run()'s completion block.
    void step() {
        if (!running_) return;
        if (showStartedFrame_) { showStartedFrame_ = false; return; }
        if (failing_) {
            running_ = false;
            failedState_ = true;
            notices_.push_back({"could not install SpaceGrotesk-Bold.ttf: download failed (tried curl, curl -k, wget)", 8.0f});
            return;
        }
        ++next_;
        if (next_ >= total_) {
            running_ = false;
            installed = true;
            enabled = true;
            notices_.push_back({"Space Grotesk font installed and enabled", 6.0f});
        }
    }

    bool running_ = false, showStartedFrame_ = false, failedState_ = false, failing_ = false, noDirectory_ = false;
    int next_ = 0, total_ = 3;
    float timer_ = 0.0f;
    std::deque<std::pair<std::string, float>> notices_;
};

}  // namespace patsgui
