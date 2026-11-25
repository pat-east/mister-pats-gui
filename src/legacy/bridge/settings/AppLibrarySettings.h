#pragma once

// Host side of the Library service: implements src/app/settings/LibraryService.h on top of the Classic objects, calling
// what SettingsScreen, ScanScreen and App call (the comment of each method names the Classic code).
//   Classic source: src/legacy/SettingsScreen.cpp buildRows() Library rows, openCoreUpdater()/handleCoreUpdater(), openAccountEntry()/handleAccountEntry(); src/legacy/ScanScreen.*; src/legacy/App.cpp reloadLibrary()/openScan()/openArtwork()/closeScan()
//   Needs: Context: preferences, preview.service() (ScreenScraperService), preview.stop(), theGamesDB, library, discArtwork, discDrive,
//          onGameVersionSettingChanged, libraryRevision; SettingsHost: scan (LibraryScan), scraper (MediaScraper), reloadLibrary
//   Owns: one CoreMraUpdater (in Classic a member of SettingsScreen), the cached ScreenScraper login (read once and after a save).
// The scan and the box-art job are App's objects and App's frame loop steps them (one system or one game per frame, App.cpp);
// update() must therefore never step them.

#include <string>

#include "CoreMraUpdater.h"
#include "ScreenScraperSettings.h"
#include "TheGamesDBClient.h"
#include "app/settings/LibraryService.h"
#include "bridge/settings/AdapterEnv.h"

class AppLibrarySettings : public patsgui::LibraryService {
public:
    explicit AppLibrarySettings(const AdapterEnv &env);

    bool available() const override;
    void update(float deltaSeconds) override;
    bool organizeGameVersions() const override;
    void setOrganizeGameVersions(bool on) override;
    bool screenScraperEnabled() const override;
    void setScreenScraperEnabled(bool on) override;
    int screenScraperVolume() const override;
    void setScreenScraperVolume(int percent) override;
    std::string screenScraperRegion() const override;
    void setScreenScraperRegion(const std::string &region) override;
    int screenScraperDelay() const override;
    void setScreenScraperDelay(int seconds) override;
    int screenScraperStorageLimitMb() const override;
    std::vector<ScrapedVideo> scrapedVideos() override;
    bool deleteScrapedVideo(const std::string &id) override;
    size_t deleteAllScrapedVideos() override;
    void setScreenScraperStorageLimitMb(int megabytes) override;
    void reloadSavedGameList() override;
    patsgui::TheGamesDbStatus theGamesDb() const override;
    bool startTheGamesDbCheck() override;
    patsgui::ScreenScraperAccount account() const override;
    void checkAccount() override;
    std::string screenScraperUser() const override;
    bool screenScraperPasswordSet() const override;
    bool saveScreenScraperLogin(bool user, const std::string &value) override;
    std::vector<std::string> scanRoots() const override;
    patsgui::DriveSpace driveSpace(const std::string &root) const override;
    void startScan(const std::vector<std::string> &roots) override;
    void cancelScan() override;
    patsgui::ScanProgress scan() const override;
    std::vector<patsgui::ArtworkSystem> artworkSystems() const override;
    uint64_t libraryGameCount() const override;
    void libraryTaskClosed() override;
    void startArtwork(const std::vector<std::string> &systemKeys) override;
    void cancelArtwork() override;
    patsgui::ArtworkProgress artwork() const override;
    void startJumpStart() override;
    void startJumpStartArtwork() override;
    bool internetRouteAvailable() const override;
    bool startCoreCheck(bool fresh) override;
    void cancelCoreCheck() override;
    bool startCoreInstall() override;
    void selectAllMissingOrOutdated() override;
    bool toggleCoreSelected(size_t index) override;
    const patsgui::CoreMraProgress &cores() const override;

private:
    void refreshLogin();                 // ScreenScraperSettings::load(): SettingsScreen::refreshScreenScraperLogin()
    void refreshCores();                 // a copy of CoreMraUpdater::snapshot() into coresCache_
    void refreshTheGamesDb();            // a copy of the client's state into tgdbCache_ (its key file read is cached 1 s inside)

    AdapterEnv env_;
    CoreMraUpdater coreUpdater_;
    patsgui::CoreMraProgress coresCache_;
    float coresClock_ = 0.0f;
    bool coresDirty_ = true;

    patsgui::TheGamesDbStatus tgdbCache_;
    float tgdbClock_ = 0.0f;
    TheGamesDBClient::CheckState previousTgdb_ = TheGamesDBClient::CheckState::NotChecked;

    std::string user_;                   // read once, so drawing never touches the card
    bool passwordSet_ = false;

    bool scanWatched_ = false;           // a scan was started here and its end has not been handled yet (onFinished_)
    bool artworkRan_ = false;            // a box-art job started here since the last libraryTaskClosed()
};
