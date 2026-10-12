#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Screen.h"
#include "SystemInfo.h"
#include "UpdateService.h"
#include "OnScreenKeyboard.h"
#include "Wifi.h"

class SettingsScreen : public Screen {
public:
    SettingsScreen(Context &context, std::function<void()> onReload,
                   std::function<void()> onBuildDatabase,
                   std::function<void()> onFetchArtwork,
                   std::function<void()> onDownloadAssets,
                   std::function<std::string()> assetDownloadStatus,
                   std::function<void()> onManageSystems,
                   std::function<void()> onManageControllers,
                   std::function<void()> onManageArcade,
                   std::function<void()> onToggleGamesTab,
                   std::function<void()> onToggleArcadeTab,
                   std::function<void()> onCycleDefaultView,
                   std::function<void()> onStartMisterCore,
                   std::function<void()> onToggleAutomaticCheck,
                   std::function<void()> onCheckUpdates,
                   std::function<void()> onConfirmUpdate,
                   std::function<void()> onInstallUpdate,
                   std::function<void()> onCancelUpdate,
                   std::function<void()> onDismissInstallSuccess,
                   std::function<void()> onShowInstallSuccess,
                   std::function<void()> onRemovePinnedVersion,
                   std::function<void(bool)> onRestartAfterUpdate,
                   std::function<std::string()> spaceGroteskStatus,
                   std::function<void()> onSpaceGrotesk,
                   std::function<bool()> onShowChangelog,
                   std::function<void()> onReboot);

    void setFramebufferInfo(const std::string &info) { framebufferInfo_ = info; }
    void setUpdateSnapshot(const UpdateSnapshot &snapshot);
    bool wantsBack() const {
        return optionsFocused_ || wifiStep_ != WifiStep::None || timeZoneChooserActive_ ||
               rebootConfirmationActive_ || changelogActive_ || infoModalActive_ ||
               discDriveSettingsActive_ || discCoreInstallConfirmationActive_;
    }
    bool wantsStartForWifiPassword() const { return wifiStep_ == WifiStep::PasswordEntry; }

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    struct Row {
        std::string label;
        std::function<std::string()> value;
        std::function<void()> activate;
        bool enabled = true;
    };

    struct Category {
        std::string label;
        std::vector<Row> rows;
    };

    struct Fact {
        std::string label;
        std::string value;
    };

    struct ChangelogLine {
        std::string text;
        bool heading = false;
        bool blank = false;
        int height = 0;
    };

    void buildRows();
    void buildUpdateRows();
    void renderUpdateConfirmation(Canvas &canvas, const Rect &area);
    void renderUpdateSuccess(Canvas &canvas, const Rect &area);
    void renderChangelog(Canvas &canvas, const Rect &area);
    void prepareChangelogLines(const std::string &text);
    void renderTimeZoneChooser(Canvas &canvas, const Rect &area);
    void handleTimeZoneChooser(Action action);
    void startWifiSetup();
    void handleWifiFlow(Action action);
    void renderWifiFlow(Canvas &canvas, const Rect &area);
    void cancelWifiFlow(bool restoreConnection);
    std::vector<Fact> facts() const;
    void renderCategories(Canvas &canvas, const Rect &area);
    void renderOptions(Canvas &canvas, const Rect &area);
    void renderSettingHelp(Canvas &canvas, const Rect &area);
    void renderInfo(Canvas &canvas, const Rect &area);
    void renderDiscDriveSettings(Canvas &canvas, const Rect &area);
    void renderDiscCoreInstallConfirmation(Canvas &canvas, const Rect &area);

    Context &context_;
    std::function<void()> onReload_;
    std::function<void()> onBuildDatabase_;
    std::function<void()> onFetchArtwork_;
    std::function<void()> onDownloadAssets_;
    std::function<std::string()> assetDownloadStatus_;
    std::function<void()> onManageSystems_;
    std::function<void()> onManageControllers_;
    std::function<void()> onManageArcade_;
    std::function<void()> onToggleGamesTab_;
    std::function<void()> onToggleArcadeTab_;
    std::function<void()> onCycleDefaultView_;
    std::function<void()> onStartMisterCore_;
    std::function<void()> onToggleAutomaticCheck_;
    std::function<void()> onCheckUpdates_;
    std::function<void()> onConfirmUpdate_;
    std::function<void()> onInstallUpdate_;
    std::function<void()> onCancelUpdate_;
    std::function<void()> onDismissInstallSuccess_;
    std::function<void()> onShowInstallSuccess_;
    std::function<void()> onRemovePinnedVersion_;
    std::function<void(bool)> onRestartAfterUpdate_;
    std::function<std::string()> spaceGroteskStatus_;
    std::function<void()> onSpaceGrotesk_;
    std::function<bool()> onShowChangelog_;
    std::function<void()> onReboot_;
    UpdateSnapshot updateSnapshot_;
    std::vector<ChangelogLine> changelogLines_;
    uint32_t changelogGeneration_ = 0;
    int changelogScroll_ = 0;
    int changelogMaxScroll_ = 0;
    int notesScroll_ = 0;
    std::vector<Category> categories_;
    TheGamesDBClient::CheckState previousTheGamesDbCheckState_ =
        TheGamesDBClient::CheckState::NotChecked;
    SystemInfo system_;
    std::string framebufferInfo_;
    int categoryCursor_ = 0;
    int optionCursor_ = 0;
    bool optionsFocused_ = false;
    bool timeZoneChooserActive_ = false;
    bool rebootConfirmationActive_ = false;
    bool changelogActive_ = false;
    bool infoModalActive_ = false;
    bool discDriveSettingsActive_ = false;
    bool discCoreInstallConfirmationActive_ = false;
    enum class WifiStep { None, Scanning, Networks, SsidEntry, PasswordEntry, CountryEntry,
                          TestPrompt, Testing, TestReview, TestFailed };
    Wifi wifi_;
    OnScreenKeyboard wifiKeyboard_{64};
    WifiStep wifiStep_ = WifiStep::None;
    std::vector<Wifi::Network> wifiNetworks_;
    int wifiCursor_ = 0;
    int wifiScroll_ = 0;
    std::string wifiSsid_;
    std::string wifiPassword_;
    std::string wifiMessage_;
    bool wifiOpen_ = false;
    bool wifiHidden_ = false;
    bool choosingTimeZoneCity_ = false;
    std::vector<std::string> timeZoneRegions_;
    std::vector<std::string> timeZoneCities_;
    int timeZoneCursor_ = 0;
    int timeZoneRegionCursor_ = 0;
    int timeZoneScroll_ = 0;
};
