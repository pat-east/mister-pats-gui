#include "bridge/settings/AppAccessibilitySettings.h"

#include "Preferences.h"
#include "Theme.h"

AppAccessibilitySettings::AppAccessibilitySettings(const AdapterEnv &env) : env_(env) { refreshFont(); }

bool AppAccessibilitySettings::available() const {
    const SettingsHost &host = env_.context.settingsHost;
    return host.spaceGroteskStatus && host.activateSpaceGrotesk;
}

// App::spaceGroteskStatus() decides the text (running download, missing, failed, On, Off); it reads the font files.
void AppAccessibilitySettings::refreshFont() {
    installed_ = Theme::spaceGroteskAvailable();
    const SettingsHost &host = env_.context.settingsHost;
    status_ = host.spaceGroteskStatus ? host.spaceGroteskStatus() : std::string();
    sinceRefresh_ = 0.0f;
}

void AppAccessibilitySettings::update(float deltaSeconds) {
    sinceRefresh_ += deltaSeconds;
    if (sinceRefresh_ >= 0.5f) refreshFont();
}

bool AppAccessibilitySettings::fontInstalled() const { return installed_; }
bool AppAccessibilitySettings::fontEnabled() const { return env_.context.preferences.useSpaceGrotesk(); }
std::string AppAccessibilitySettings::fontStatus() const { return status_; }

// The download's own state, read live (the cached status above lags up to half a second).
patsgui::FontDownloadInfo AppAccessibilitySettings::fontDownload() const {
    const SettingsHost &host = env_.context.settingsHost;
    patsgui::FontDownloadInfo out;
    out.running = host.spaceGroteskRunning && host.spaceGroteskRunning();
    out.failed = !out.running && host.spaceGroteskFailed && host.spaceGroteskFailed();
    if (out.failed && host.spaceGroteskError) out.error = host.spaceGroteskError();
    return out;
}

// App::activateSpaceGrotesk(): toggles the installed font, or starts App's own download. Its notices go through
// Context::notify (the shell shows them as toasts).
void AppAccessibilitySettings::activateFont() {
    const SettingsHost &host = env_.context.settingsHost;
    if (host.activateSpaceGrotesk) host.activateSpaceGrotesk();
    refreshFont();
}

// What every theme row of Classic ends with: the Classic theme is already set, the prepared background is rendered
// again and the Classic screen redraws (src/legacy/App.cpp onThemeChanged); the new GUI redraws too.
void AppAccessibilitySettings::themeChanged() {
    if (env_.context.onThemeChanged) env_.context.onThemeChanged();
    if (env_.invalidate) env_.invalidate();
}

int AppAccessibilitySettings::fontSizeScale() const { return env_.context.preferences.fontSizeScale(); }

void AppAccessibilitySettings::setFontSizeScale(int percent) {
    env_.context.preferences.setFontSizeScale(percent);
    env_.context.theme.setFontSizeScale(percent);
    themeChanged();
}

int AppAccessibilitySettings::hintSizeScale() const { return env_.context.preferences.hintSizeScale(); }

void AppAccessibilitySettings::setHintSizeScale(int percent) {
    env_.context.preferences.setHintSizeScale(percent);
    env_.context.theme.setHintSizeScale(percent);
    themeChanged();
}

bool AppAccessibilitySettings::highContrast() const { return env_.context.preferences.highContrast(); }

void AppAccessibilitySettings::setHighContrast(bool on) {
    env_.context.preferences.setHighContrast(on);
    env_.context.theme.setHighContrast(on);
    themeChanged();
}

int AppAccessibilitySettings::highContrastColor() const { return env_.context.preferences.highContrastColor(); }

void AppAccessibilitySettings::setHighContrastColor(int index) {
    env_.context.preferences.setHighContrastColor(index);
    env_.context.theme.setHighContrastColor(index);
    themeChanged();
}
