#pragma once

// Host side of the System service: implements src/app/settings/SystemService.h on top of the Classic objects. It adds no
// behavior: every method is the call Classic makes (src/legacy/SettingsScreen.cpp buildRows() System rows, facts(), the Wi-Fi flow;
// src/legacy/App.cpp rebootMister()/startMisterCore()/startAssetDownload()/the backup report).
//   Needs: Context: preferences (clock, time zone, screensaver), discDrive, discCores, discInstaller, library, favorites,
//   images, theme; Context::settingsHost: backup hooks, rebootMister, startAssetDownload/assetDownloadStatus/...,
//   startMisterCore, framebufferInfo; owns a Wifi and a SystemInfo (as SettingsScreen does).

#include "Preferences.h"
#include "SystemInfo.h"
#include "Wifi.h"
#include "app/settings/SystemService.h"
#include "bridge/settings/AdapterEnv.h"

class AppSystemSettings : public patsgui::SystemService {
public:
    explicit AppSystemSettings(const AdapterEnv &env);

    bool available() const override;
    void update(float deltaSeconds) override;
    patsgui::DiscDriveInfo disc() const override;
    patsgui::DiscInstallInfo discInstall() const override;
    bool startDiscInstall() override;
    void cancelDiscInstall() override;
    std::vector<patsgui::SystemFact> facts() const override;
    bool use24HourClock() const override;
    void setUse24HourClock(bool on) override;
    std::string timeZone() const override;
    bool setTimeZone(const std::string &zone) override;
    std::vector<std::string> timeZoneRegions() const override;
    std::vector<std::string> timeZonesIn(const std::string &region) const override;
    int screensaverDelay() const override;
    void setScreensaverDelay(int seconds) override;
    std::string screensaverStyle() const override;
    void setScreensaverStyle(const std::string &style) override;
    bool startBackup() override;
    patsgui::BackupInfo backup() const override;
    void acknowledgeBackup() override;
    patsgui::IconDownloadInfo icons() const override;
    patsgui::IconStart startIconDownload() override;
    bool wifiAvailable() const override;
    bool wifiStartScan(std::string &error) override;
    bool wifiScanComplete() const override;
    std::vector<patsgui::WifiNetwork> wifiScanResults(std::string &error) override;
    bool wifiBeginTest(const std::string &ssid, const std::string &password, bool open, bool hidden,
                       std::string &error) override;
    patsgui::WifiTest wifiPollTest(float deltaSeconds) override;
    bool wifiTesting() const override;
    bool wifiNeedsCountryCode() const override;
    bool wifiSetCountryCode(const std::string &country, std::string &error) override;
    bool wifiSaveTestedNetwork(std::string &error) override;
    void wifiRollback() override;
    void returnToMisterMenu() override;
    bool reboot(std::string &error) override;

private:
    void redrawShell() const { if (env_.invalidate) env_.invalidate(); }

    AdapterEnv env_;
    Wifi wifi_;           // SettingsScreen::wifi_
    SystemInfo info_;     // SettingsScreen::system_
};
