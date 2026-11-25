#pragma once

// Host side of the Accessibility service: implements src/app/settings/AccessibilityService.h on top of the Classic objects.
// It adds no behavior: every method is the call Classic makes.
//   Classic source: src/legacy/SettingsScreen.cpp buildRows() Accessibility rows (472-511); src/legacy/App.cpp
//   activateSpaceGrotesk()/spaceGroteskStatus() and the download's completion in run().
//   Needs: Context: preferences, theme, onThemeChanged; Context::settingsHost: spaceGroteskStatus, activateSpaceGrotesk
//   (App owns the one SpaceGroteskDownload and steps it every frame, so this adapter starts no download itself).
// The new GUI's look (text and hint scale, high contrast and its color, the font files) is not applied here: the look
// bridge (src/legacy/bridge/LookBridge.cpp, PatsGuiScreen::syncLook) reads the same Preferences and the Classic Theme's font paths
// every frame. The notices of Classic's notify() reach the new GUI as toasts through PatsGuiScreen, so takeNotice()
// stays at its default (none).

#include <string>

#include "app/settings/AccessibilityService.h"
#include "bridge/settings/AdapterEnv.h"

class AppAccessibilitySettings : public patsgui::AccessibilityService {
public:
    explicit AppAccessibilitySettings(const AdapterEnv &env);

    bool available() const override;
    void update(float deltaSeconds) override;
    bool fontInstalled() const override;
    bool fontEnabled() const override;
    std::string fontStatus() const override;
    patsgui::FontDownloadInfo fontDownload() const override;
    void activateFont() override;
    int fontSizeScale() const override;
    void setFontSizeScale(int percent) override;
    int hintSizeScale() const override;
    void setHintSizeScale(int percent) override;
    bool highContrast() const override;
    void setHighContrast(bool on) override;
    int highContrastColor() const override;
    void setHighContrastColor(int index) override;

private:
    void refreshFont();
    void themeChanged();

    AdapterEnv env_;
    // Classic reads the two font files (Theme::spaceGroteskAvailable) every time the row is drawn. The new GUI asks every
    // frame, so the answer is cached and re-read twice a second, and at once after a press (the download's progress and
    // its end show within half a second).
    bool installed_ = false;
    std::string status_;
    float sinceRefresh_ = 0.0f;
};
