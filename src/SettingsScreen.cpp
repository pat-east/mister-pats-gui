#include "SettingsScreen.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

#include "Canvas.h"
#include "ButtonLabels.h"
#include "DiscCorePackage.h"
#include "GamesScreen.h"

namespace {

std::string settingHelp(const std::string &label) {
    if (label == "TheGamesDB")
        return "Looks up box art, screenshots and release year for PlayStation and Saturn discs. Add your API key to /media/fat/mister-pat/thegamesdb-settings.ini as apikey=\"YOUR_KEY\", then select this row to test it. Without a key or a matching result, artwork falls back to Libretro.";
    if (label == "Build game list")
        return "Search connected drives for games and save a list the menus can load quickly. Run this after adding or removing games.";
    if (label == "Prepare box art")
        return "Download missing covers and make copies sized for each view. Covers already on your drives are kept.";
    if (label == "Reload saved game list")
        return "Reload the saved game list. This does not search the drives or find newly added games.";
    if (label == "Organize game-versions")
        return "Group ROMs with region, revision or language suffixes under one title.";
    if (label == "Manage systems")
        return "Choose which consoles appear in the Consoles and Games views.";
    if (label == "Manage Arcade")
        return "See which Arcade games are ready to launch and why others are unavailable.";
    if (label == "Controllers")
        return "Test connected controllers, change how far a stick must move, or set up button controls.";
    if (label == "Face-button labels")
        return "Choose the A, B, X and Y labels that match your controller. This changes labels only.";
    if (label == "Show Games tab")
        return "Show or hide the tab that lists games from every system.";
    if (label == "Show Arcade tab")
        return "Show or hide the tab that lists playable Arcade games.";
    if (label == "Show Arcade in Consoles")
        return "Add Arcade to the Consoles grid. The Arcade tab has its own list either way.";
    if (label == "Tab display")
        return "Choose whether the top navigation shows tab names, icons, or both.";
    if (label == "Show box art")
        return "Show or hide cover pictures while browsing games.";
    if (label == "Load small boxarts in grid-view")
        return "Use smaller cover images in Grid view. This can noticeably reduce image loading time.";
    if (label == "Start sound")
        return "Play a sound after the GUI finishes starting.";
    if (label == "Menu sounds")
        return "Play sounds when moving through menus, changing tabs, or starting a game.";
    if (label == "Game view")
        return "Choose one view for every tab, or Individual to remember a separate view for Home, Favorites, Consoles and Games. Press Y while browsing to change that tab's view.";
    if (label == "Space Grotesk font")
        return "Use the Space Grotesk typeface. If its files are missing, choose this row to download them.";
    if (label == "Font size")
        return "Change text size across the interface. The new size takes effect immediately.";
    if (label == "High contrast")
        return "Use brighter text, darker backgrounds, and stronger focus outlines throughout the interface.";
    if (label == "Time format")
        return "Choose whether the clock uses 24-hour time or 12-hour time with AM and PM.";
    if (label == "Time zone")
        return "Choose the local time zone used by the clock and the MiSTer system.";
    if (label == "Info")
        return "View details about this MiSTer, its hardware, storage, network and software.";
    if (label == "Download missing icons")
        return "Download missing console and navigation icons. Existing icons are left alone.";
    if (label == "Connect to Wi-Fi…")
        return "Choose a wireless network and enter its password. The connection is tested before settings are saved.";
    if (label == "Return to MiSTer menu")
        return "Close this GUI and return to the standard MiSTer menu.";
    if (label == "Reboot MiSTer")
        return "Restart the whole MiSTer system. You will leave this GUI until it starts again.";
    if (label == "Read full changelog")
        return "Read the complete project change history from GitHub. An Internet connection is required.";
    if (label == "Check for updates automatically")
        return "When on, check GitHub for a newer GUI after startup once Internet is available.";
    if (label == "Version running now")
        return "The version currently open on MiSTer.";
    if (label == "Version used on next start")
        return "The version MiSTer will open after the next restart.";
    if (label == "Newest installed version")
        return "The newest GUI version already stored on this MiSTer.";
    if (label == "Use newest installed version")
        return "Remove the version pin so MiSTer uses the newest installed GUI next time.";
    if (label == "Check for updates now")
        return "Ask GitHub whether a newer GUI version is available.";
    if (label.rfind("Install v", 0) == 0)
        return "Download and verify this version. You can choose when to restart afterward.";
    if (label == "Cancel update download")
        return "Stop downloading this update. The currently installed version stays in place.";
    if (label == "Update progress")
        return "Shows how much of the update has downloaded or been installed.";
    if (label == "Restart options")
        return "Choose whether to restart now or keep using the current version for now.";
    if (label == "Disc drive")
        return "Show the optical drive, disc status and detected PlayStation and Saturn discs.";
    if (label == "Install/update Disc cores")
        return "Download the pinned Physical Disc package, verify its checksums, back up managed files and MiSTer.ini, then configure only [CD-*]. Press again during download to cancel.";
    return {};
}

std::vector<std::string> wrapHelp(Font &font, const std::string &text, int size, int width) {
    std::vector<std::string> lines;
    std::istringstream words(text);
    std::string line;
    for (std::string word; words >> word;) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && font.measure(candidate, size) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

} // namespace

SettingsScreen::SettingsScreen(Context &context, std::function<void()> onReload,
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
                               std::function<void()> onReboot)
    : context_(context), onReload_(std::move(onReload)),
      onBuildDatabase_(std::move(onBuildDatabase)),
      onFetchArtwork_(std::move(onFetchArtwork)),
      onDownloadAssets_(std::move(onDownloadAssets)),
      assetDownloadStatus_(std::move(assetDownloadStatus)),
      onManageSystems_(std::move(onManageSystems)),
      onManageControllers_(std::move(onManageControllers)),
      onManageArcade_(std::move(onManageArcade)),
      onToggleGamesTab_(std::move(onToggleGamesTab)),
      onToggleArcadeTab_(std::move(onToggleArcadeTab)),
      onCycleDefaultView_(std::move(onCycleDefaultView)),
      onStartMisterCore_(std::move(onStartMisterCore)),
      onToggleAutomaticCheck_(std::move(onToggleAutomaticCheck)),
      onCheckUpdates_(std::move(onCheckUpdates)),
      onConfirmUpdate_(std::move(onConfirmUpdate)),
      onInstallUpdate_(std::move(onInstallUpdate)),
      onCancelUpdate_(std::move(onCancelUpdate)),
      onDismissInstallSuccess_(std::move(onDismissInstallSuccess)),
      onShowInstallSuccess_(std::move(onShowInstallSuccess)),
      onRemovePinnedVersion_(std::move(onRemovePinnedVersion)),
      onRestartAfterUpdate_(std::move(onRestartAfterUpdate)),
      spaceGroteskStatus_(std::move(spaceGroteskStatus)),
      onSpaceGrotesk_(std::move(onSpaceGrotesk)),
      onShowChangelog_(std::move(onShowChangelog)), onReboot_(std::move(onReboot)) {
    buildRows();
}

void SettingsScreen::buildRows() {
    categories_.clear();

    categories_.push_back({"Library", {}});
    categories_.push_back({"Controllers", {}});
    categories_.push_back({"Interface", {}});
    categories_.push_back({"Accessibility", {}});
    categories_.push_back({"Updates", {}});
    categories_.push_back({"System", {}});
    auto &library = categories_[0].rows;

    // Library and content — build and refresh the saved library, manage its views,
    // then keep version grouping and the external artwork service at the bottom.

    library.push_back({"Build game list", [] { return std::string("A"); },
                     [this] { onBuildDatabase_(); }});

    library.push_back({"Prepare box art", [] { return std::string("A"); },
                     [this] { onFetchArtwork_(); }});

    library.push_back({"Reload saved game list", [] { return std::string("A"); },
                     [this] {
                         // The only path that walks the game volume, and only on request.
                         context_.notify("Reloading the saved game list…", 6.0f);
                         context_.library.setScanningAllowed(true);
                         onReload_();
                         context_.library.setScanningAllowed(false);
                         context_.notify("Saved game list loaded");
                     }});

    library.push_back({"Manage systems", [] { return std::string("A"); },
                     [this] { onManageSystems_(); }});

    library.push_back({"Manage Arcade", [] { return std::string("A"); },
                     [this] { onManageArcade_(); }});

    library.push_back({"Organize game-versions",
                     [this] { return std::string(context_.preferences.organizeGameVersions() ? "On" : "Off"); },
                     [this] { context_.preferences.setOrganizeGameVersions(
                                  !context_.preferences.organizeGameVersions());
                             if (context_.onGameVersionSettingChanged)
                                 context_.onGameVersionSettingChanged(); }});

    library.push_back({"TheGamesDB",
                     [this] { return context_.theGamesDB.checkStatus(); },
                     [this] {
                         if (!context_.theGamesDB.configured()) {
                             context_.notify(std::string("Add your key to ") +
                                             TheGamesDBClient::settingsPath(), 8.0f);
                         } else if (!context_.theGamesDB.startApiKeyCheck()) {
                             context_.notify(context_.theGamesDB.checkStatus(), 5.0f);
                         } else {
                             context_.notify("Checking TheGamesDB API key…", 4.0f);
                         }
                     }});

    // Hardware.
    auto &controllers = categories_[1].rows;
    controllers.push_back({"Controllers", [] { return std::string("A"); },
                     [this] { onManageControllers_(); }});
    controllers.push_back({"Face-button labels",
                     [this] {
                         const std::string &style = context_.preferences.faceButtonStyle();
                         if (style == "playstation") return std::string("PlayStation");
                         if (style == "nintendo") return std::string("Nintendo");
                         return std::string("XBOX");
                     },
                     [this] {
                         const std::string &style = context_.preferences.faceButtonStyle();
                         const std::string next = style == "xbox" ? "playstation"
                             : style == "playstation" ? "nintendo" : "xbox";
                         context_.preferences.setFaceButtonStyle(next);
                         setFaceButtonStyle(next);
                     }});

    // How the interface itself looks and navigates.
    auto &interface = categories_[2].rows;
    interface.push_back({"Show Games tab",
                     [this] { return std::string(context_.preferences.showGamesTab() ? "On" : "Off"); },
                     [this] { onToggleGamesTab_(); }});

    interface.push_back({"Show Arcade tab",
                     [this] { return std::string(context_.preferences.showArcadeTab() ? "On" : "Off"); },
                     [this] { onToggleArcadeTab_(); }});

    interface.push_back({"Show Arcade in Consoles",
                     [this] { return std::string(context_.preferences.showArcadeInConsoles()
                                                     ? "On" : "Off"); },
                     [this] {
                         context_.preferences.setShowArcadeInConsoles(
                             !context_.preferences.showArcadeInConsoles());
                     }});

    interface.push_back({"Tab display",
                     [this] {
                         switch (context_.preferences.tabDisplay()) {
                         case TabDisplay::Icons: return std::string("Icons");
                         case TabDisplay::Both: return std::string("Both");
                         case TabDisplay::Names: return std::string("Names");
                         }
                         return std::string("Names");
                     },
                     [this] {
                         const TabDisplay current = context_.preferences.tabDisplay();
                         const TabDisplay next = current == TabDisplay::Names ? TabDisplay::Icons
                             : current == TabDisplay::Icons ? TabDisplay::Both : TabDisplay::Names;
                         context_.preferences.setTabDisplay(next);
                     }});

    interface.push_back({"Show box art",
                     [this] { return std::string(context_.preferences.showBoxArt() ? "On" : "Off"); },
                     [this] {
                         context_.preferences.setShowBoxArt(!context_.preferences.showBoxArt());
                     }});

    interface.push_back({"Load small boxarts in grid-view",
                     [this] {
                         return std::string(context_.preferences.loadSmallBoxartsInGridView()
                                                ? "On" : "Off");
                     },
                     [this] {
                         context_.preferences.setLoadSmallBoxartsInGridView(
                             !context_.preferences.loadSmallBoxartsInGridView());
                     }});

    interface.push_back({"Start sound",
                     [this] { return std::string(context_.preferences.startSoundEnabled()
                                                     ? "On" : "Off"); },
                     [this] {
                         context_.preferences.setStartSoundEnabled(
                             !context_.preferences.startSoundEnabled());
                     }});

    interface.push_back({"Menu sounds",
                     [this] { return std::string(context_.preferences.menuSoundsEnabled()
                                                     ? "On" : "Off"); },
                     [this] {
                         context_.preferences.setMenuSoundsEnabled(
                             !context_.preferences.menuSoundsEnabled());
                     }});

    interface.push_back({"Game view",
                     [this] {
                         const std::string &view = context_.preferences.defaultView();
                         if (view == "individual") return std::string("Individual");
                         return std::string(
                             displayNameForGameView(gameViewFromName(view)));
                     },
                     [this] { onCycleDefaultView_(); }});

    auto &accessibility = categories_[3].rows;
    accessibility.push_back({"Space Grotesk font",
                     [this] { return spaceGroteskStatus_(); },
                     [this] { onSpaceGrotesk_(); }});

    accessibility.push_back({"Font size",
                     [this] {
                         const int scale = context_.preferences.fontSizeScale();
                         return scale == 130 ? std::string("Largest")
                             : scale == 115 ? std::string("Larger") : std::string("Standard");
                     },
                     [this] {
                         const int scale = context_.preferences.fontSizeScale();
                         const int next = scale == 100 ? 115 : scale == 115 ? 130 : 100;
                         context_.preferences.setFontSizeScale(next);
                         context_.theme.setFontSizeScale(next);
                         if (context_.onThemeChanged) context_.onThemeChanged();
                     }});

    accessibility.push_back({"High contrast",
                     [this] { return std::string(context_.preferences.highContrast() ? "On" : "Off"); },
                     [this] {
                         const bool enabled = !context_.preferences.highContrast();
                         context_.preferences.setHighContrast(enabled);
                         context_.theme.setHighContrast(enabled);
                         if (context_.onThemeChanged) context_.onThemeChanged();
                     }});

    // Time display is part of system configuration, next to the operating system actions.
    auto &system = categories_[5].rows;
    system.push_back({"Disc drive",
                     [this] {
                         const auto drive = context_.discDrive.snapshot();
                         return drive.state == DiscDriveService::MediaState::NoDrive
                             ? std::string("No drive") : std::string("A Status");
                     },
                     [this] { discDriveSettingsActive_ = true; }, false});
    system.push_back({"Install/update Disc cores",
                     [this] {
                         if (context_.discInstaller.recoveryBlocked())
                             return std::string("Recovery failed — package actions disabled");
                         const auto state = context_.discInstaller.snapshot();
                         if (state.state == DiscCoreInstaller::State::Installing)
                             return std::string("Installing — finishing safely");
                         if (state.state == DiscCoreInstaller::State::Downloading ||
                             state.state == DiscCoreInstaller::State::Verifying)
                             return std::string("Working — ") +
                                 std::to_string(state.bytes / 1024) + " KB — A cancel";
                         if (!state.message.empty()) return state.message;
                         const auto cores = context_.discCores.snapshot();
                         if (cores.packageReady())
                             return std::string("Installed v") + DiscCorePackage::kVersion;
                         return cores.alternateMainInstalled || cores.psxMglInstalled ||
                                cores.saturnMglInstalled
                             ? std::string("Repair/update v") + DiscCorePackage::kVersion
                             : std::string("Install v") + DiscCorePackage::kVersion;
                     },
                     [this] {
                         if (context_.discInstaller.busy()) {
                             if (context_.discInstaller.snapshot().state == DiscCoreInstaller::State::Installing) {
                                 context_.notify("Installation is finishing safely; please wait.", 5.0f);
                             } else {
                                 context_.discInstaller.cancel();
                                 context_.notify("Cancelling disc package download…", 5.0f);
                             }
                         } else {
                             if (context_.discInstaller.recoveryBlocked()) {
                                 context_.notify(context_.discInstaller.snapshot().message, 7.0f);
                                 return;
                             }
                             discCoreInstallConfirmationActive_ = true;
                         }
                     }});
    system.push_back({"Info", [] { return std::string("A"); },
                     [this] { infoModalActive_ = true; }});
    system.push_back({"Time format",
                     [this] { return std::string(context_.preferences.use24HourClock()
                                                   ? "24-hour" : "12-hour AM/PM"); },
                     [this] {
                         context_.preferences.setUse24HourClock(
                             !context_.preferences.use24HourClock());
                     }});

    system.push_back({"Time zone",
                     [this] {
                         return context_.preferences.timeZone().empty()
                             ? std::string("System default")
                             : context_.preferences.timeZone();
                     },
                     [this] {
                         timeZoneRegions_ = Preferences::timeZoneRegions();
                         if (timeZoneRegions_.empty()) {
                             context_.notify("Could not load the time-zone list from MiSTer.");
                             return;
                         }
                         timeZoneChooserActive_ = true;
                         choosingTimeZoneCity_ = false;
                         timeZoneCursor_ = 0;
                         timeZoneScroll_ = 0;
                         const size_t slash = context_.preferences.timeZone().find('/');
                         if (slash != std::string::npos) {
                             const std::string current = context_.preferences.timeZone().substr(0, slash);
                             for (size_t i = 0; i < timeZoneRegions_.size(); ++i)
                                 if (timeZoneRegions_[i] == current) timeZoneCursor_ = int(i);
                         }
                     }});

    // This project itself.
    categories_[4].rows.push_back({"Check for updates automatically",
                     [this] { return std::string(context_.preferences.checkForUpdates() ? "On" : "Off"); },
                     [this] { onToggleAutomaticCheck_(); }});
    buildUpdateRows();

    system.push_back({"Download missing icons",
                     [this] { return assetDownloadStatus_(); },
                     [this] { onDownloadAssets_(); }});
    system.push_back({"Connect to Wi-Fi…",
                     [this] { return wifi_.available() ? std::string("A")
                                                       : std::string("No adapter"); },
                     [this] { startWifiSetup(); }, wifi_.refreshInterface()});
    system.push_back({"Return to MiSTer menu", [] { return std::string("A"); },
                     [this] { onStartMisterCore_(); }});
    system.push_back({"Reboot MiSTer", [] { return std::string("A"); },
                     [this] { rebootConfirmationActive_ = true; }});
    system.push_back({"Read full changelog", [] { return std::string("A"); },
                     [this] {
                         if (onShowChangelog_ && onShowChangelog_()) {
                             changelogActive_ = true;
                             changelogScroll_ = 0;
                             changelogMaxScroll_ = 0;
                         } else {
                             context_.notify("Another online task is running. Try again when it finishes.");
                         }
                     }});
}

void SettingsScreen::buildUpdateRows() {
    auto &rows = categories_[4].rows;
    rows.resize(1);
    rows.push_back({"Version running now", [this] {
        return std::string("v") + updateSnapshot_.runningVersion;
    }, {}, false});
    rows.push_back({"Version used on next start", [this] {
        return updateSnapshot_.pinned
            ? std::string("Pinned v") + updateSnapshot_.selectedVersion
            : std::string("Newest v") + updateSnapshot_.selectedVersion;
    }, {}, false});
    if (updateSnapshot_.pinned &&
        updateSnapshot_.newestInstalledVersion != updateSnapshot_.selectedVersion)
        rows.push_back({"Newest installed version", [this] {
            return std::string("v") + updateSnapshot_.newestInstalledVersion;
        }, {}, false});
    const UpdatePhase phase = updateSnapshot_.phase;
    if (updateSnapshot_.pinned) {
        const bool enabled = phase == UpdatePhase::Idle || phase == UpdatePhase::Current ||
                             phase == UpdatePhase::Available || phase == UpdatePhase::Blocked ||
                             phase == UpdatePhase::Error || phase == UpdatePhase::RestartRequired;
        rows.push_back({"Use newest installed version", [] { return std::string("A"); },
                        [this] { onRemovePinnedVersion_(); }, enabled});
    }
    if (phase == UpdatePhase::WaitingForInternet || phase == UpdatePhase::Checking) {
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
    } else if (phase == UpdatePhase::Downloading || phase == UpdatePhase::Verifying ||
               phase == UpdatePhase::Installing) {
            rows.push_back({"Update progress", [this] {
            const auto &s = updateSnapshot_;
            if (s.phase == UpdatePhase::Verifying) return std::string("Checking downloaded files…");
            if (s.phase == UpdatePhase::Installing) return std::string("Installing update…");
            if (s.bytesTotal) return std::to_string(s.bytesDone * 100 / s.bytesTotal) + "%";
            return std::string("Downloaded ") + SystemInfo::formatBytes(s.bytesDone);
        }, {}, false});
        if (phase != UpdatePhase::Installing)
            rows.push_back({"Cancel update download", {}, {}, false});
    } else if (phase == UpdatePhase::RestartRequired) {
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
        if (!updateSnapshot_.successPromptVisible)
            rows.push_back({"Restart options", [] { return std::string("A"); },
                            [this] { onShowInstallSuccess_(); }});
    } else {
        rows.push_back({"Check for updates now", [] { return std::string("A"); },
                        [this] { onCheckUpdates_(); }});
    }
    if (phase == UpdatePhase::Available && updateSnapshot_.canInstall)
        rows.push_back({"Install v" + updateSnapshot_.targetVersion + " update",
                        [] { return std::string("A"); },
                        [this] { onConfirmUpdate_(); }});
    if (phase == UpdatePhase::Blocked || phase == UpdatePhase::Error ||
        (phase == UpdatePhase::Available && !updateSnapshot_.canInstall))
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
    if (phase == UpdatePhase::Blocked)
        for (const std::string &path : updateSnapshot_.repairPaths)
            rows.push_back({path, {}, {}, false});
    if (phase == UpdatePhase::Current)
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
}

void SettingsScreen::setUpdateSnapshot(const UpdateSnapshot &snapshot) {
    if (snapshot.changelogGeneration != changelogGeneration_) {
        changelogGeneration_ = snapshot.changelogGeneration;
        changelogScroll_ = 0;
        prepareChangelogLines(snapshot.changelog);
    }
    const bool rebuild = snapshot.phase != updateSnapshot_.phase ||
                         snapshot.targetVersion != updateSnapshot_.targetVersion ||
                         snapshot.canInstall != updateSnapshot_.canInstall ||
                         snapshot.runningVersion != updateSnapshot_.runningVersion ||
                         snapshot.selectedVersion != updateSnapshot_.selectedVersion ||
                         snapshot.newestInstalledVersion != updateSnapshot_.newestInstalledVersion ||
                         snapshot.pinned != updateSnapshot_.pinned ||
                         snapshot.statusText != updateSnapshot_.statusText ||
                         snapshot.successPromptVisible != updateSnapshot_.successPromptVisible ||
                         snapshot.pinRemoved != updateSnapshot_.pinRemoved ||
                         snapshot.repairPaths != updateSnapshot_.repairPaths;
    updateSnapshot_ = snapshot;
    if (!rebuild || categories_.size() < 5) return;
    buildUpdateRows();
    if (categoryCursor_ == 4) {
        const int count = int(categories_[4].rows.size());
        if (!count) { optionCursor_ = 0; optionsFocused_ = false; }
        else optionCursor_ = std::max(0, std::min(optionCursor_, count - 1));
    }
    if (snapshot.phase == UpdatePhase::Confirming) notesScroll_ = 0;
}

std::vector<SettingsScreen::Fact> SettingsScreen::facts() const {
    std::vector<Fact> out;
    char buffer[128];

    out.push_back({"Machine", ""});

    std::snprintf(buffer, sizeof(buffer), "%s, %d cores", system_.cpuModel().c_str(),
                  system_.cores());
    out.push_back({"Processor", buffer});

    if (system_.cpuBusy() >= 0.0f) {
        std::snprintf(buffer, sizeof(buffer), "%.0f%%  ·  load %.2f", system_.cpuBusy() * 100.0f,
                      system_.loadAverage());
        out.push_back({"In use", buffer});
    }

    if (system_.memoryTotalBytes()) {
        std::snprintf(buffer, sizeof(buffer), "%s of %s",
                      SystemInfo::formatBytes(system_.memoryUsedBytes()).c_str(),
                      SystemInfo::formatBytes(system_.memoryTotalBytes()).c_str());
        out.push_back({"Memory", buffer});
    }

    out.push_back({"Running for", SystemInfo::formatDuration(system_.uptimeSeconds())});

    out.push_back({"", ""});
    out.push_back({"Networking", ""});
    out.push_back({"Ethernet", system_.ethernetAddress()});
    out.push_back({"Wi-Fi", system_.wifiAddress()});
    out.push_back({"Wi-Fi network", system_.wifiNetwork()});

    out.push_back({"", ""});
    out.push_back({"Storage", ""});
    for (const SystemInfo::Volume &volume : system_.volumes()) {
        const uint64_t used = volume.totalBytes - volume.freeBytes;
        std::snprintf(buffer, sizeof(buffer), "%s of %s free",
                      SystemInfo::formatBytes(volume.freeBytes).c_str(),
                      SystemInfo::formatBytes(volume.totalBytes).c_str());
        (void)used;
        out.push_back({volume.path, buffer});
    }

    out.push_back({"", ""});
    out.push_back({"Library", ""});

    if (context_.library.usingDatabase()) {
        const GameDatabase &db = context_.library.database();
        std::snprintf(buffer, sizeof(buffer), "%zu games in %zu systems", db.totalGames(),
                      db.systems().size());
        out.push_back({"Database", buffer});

        for (const std::string &missing : db.missingRoots())
            out.push_back({"Drive missing", missing});
    } else {
        out.push_back({"Database", "not built"});
    }

    out.push_back({"Systems listed", std::to_string(context_.library.systems().size())});
    out.push_back({"Favorites", std::to_string(context_.favorites.size())});

    const size_t indexed = context_.library.index().size();
    if (indexed) out.push_back({"Imported game list", std::to_string(indexed) + " games"});

    out.push_back({"", ""});
    out.push_back({"Display", ""});
    out.push_back({"Resolution", framebufferInfo_});

    Font &font = context_.theme.bold();
    if (!font.usingTrueType()) {
        out.push_back({"Font", "built-in typeface"});
    } else {
        const std::string &path = font.path();
        const size_t slash = path.find_last_of('/');
        out.push_back({"Font", slash == std::string::npos ? path : path.substr(slash + 1)});
    }

    out.push_back({"Images cached", std::to_string(context_.images.size())});
    return out;
}

void SettingsScreen::update(float deltaSeconds) {
    system_.update(deltaSeconds);
    wifi_.update(deltaSeconds);
    wifi_.refreshInterface();
    const auto gamesDbCheckState = context_.theGamesDB.checkState();
    if (previousTheGamesDbCheckState_ == TheGamesDBClient::CheckState::Checking &&
        gamesDbCheckState == TheGamesDBClient::CheckState::Success) {
        context_.discArtwork.refresh(context_.discDrive.snapshot());
        context_.notify("TheGamesDB key works: " + context_.theGamesDB.checkStatus() +
                        ".", 7.0f);
    }
    previousTheGamesDbCheckState_ = gamesDbCheckState;
    if (categories_.size() > 5) {
        auto &systemRows = categories_[5].rows;
        const auto wifiRow = std::find_if(systemRows.begin(), systemRows.end(),
            [](const Row &row) { return row.label == "Connect to Wi-Fi…"; });
        if (wifiRow != systemRows.end()) wifiRow->enabled = wifi_.available();
        const auto discRow = std::find_if(systemRows.begin(), systemRows.end(),
            [](const Row &row) { return row.label == "Disc drive"; });
        if (discRow != systemRows.end())
            discRow->enabled = context_.discDrive.snapshot().state !=
                               DiscDriveService::MediaState::NoDrive;
    }

    if (wifiStep_ == WifiStep::Scanning && wifi_.scanComplete()) {
        std::string error;
        wifiNetworks_ = wifi_.scanResults(error);
        if (!error.empty()) wifiMessage_ = error;
        else if (wifiNetworks_.empty()) wifiMessage_ = "No Wi-Fi networks found. Scan again or enter a network name manually.";
        else wifiMessage_ = "Choose the Wi-Fi network to join.";
        wifiCursor_ = 0;
        wifiScroll_ = 0;
        wifiStep_ = WifiStep::Networks;
    } else if (wifiStep_ == WifiStep::Testing) {
        const Wifi::TestResult result = wifi_.pollTest(deltaSeconds, wifiMessage_);
        if (result == Wifi::TestResult::Connected) wifiStep_ = WifiStep::TestReview;
        else if (result == Wifi::TestResult::Failed) wifiStep_ = WifiStep::TestFailed;
    }
}

void SettingsScreen::handle(Action action) {
    if (discCoreInstallConfirmationActive_) {
        if (action == Action::Confirm) {
            discCoreInstallConfirmationActive_ = false;
            if (context_.discInstaller.start())
                context_.notify("Installing verified Disc Drive package…", 5.0f);
            else
                context_.notify(context_.discInstaller.snapshot().message, 5.0f);
        } else if (action == Action::Back) {
            discCoreInstallConfirmationActive_ = false;
        }
        return;
    }
    if (discDriveSettingsActive_) {
        if (action == Action::Back || action == Action::Confirm)
            discDriveSettingsActive_ = false;
        return;
    }
    if (infoModalActive_) {
        if (action == Action::Back || action == Action::Confirm)
            infoModalActive_ = false;
        return;
    }
    if (changelogActive_) {
        if (action == Action::Back) {
            changelogActive_ = false;
        } else if (action == Action::Up && changelogScroll_ > 0) {
            --changelogScroll_;
        } else if (action == Action::Down && changelogScroll_ < changelogMaxScroll_) {
            ++changelogScroll_;
        } else if (action == Action::Confirm && !updateSnapshot_.changelogError.empty()) {
            if (onShowChangelog_ && onShowChangelog_()) changelogScroll_ = 0;
            else context_.notify("Another online task is running. Try again when it finishes.");
        }
        return;
    }
    if (wifiStep_ != WifiStep::None) {
        handleWifiFlow(action);
        return;
    }
    if (rebootConfirmationActive_) {
        if (action == Action::Confirm) {
            rebootConfirmationActive_ = false;
            onReboot_();
        } else if (action == Action::Back) {
            rebootConfirmationActive_ = false;
        }
        return;
    }
    if (timeZoneChooserActive_) {
        handleTimeZoneChooser(action);
        return;
    }
    if (updateSnapshot_.phase == UpdatePhase::RestartRequired &&
        updateSnapshot_.successPromptVisible) {
        switch (action) {
        case Action::Confirm: onRestartAfterUpdate_(false); break;
        case Action::Back: onDismissInstallSuccess_(); break;
        case Action::FaceXPress:
            if (updateSnapshot_.pinned) onRestartAfterUpdate_(true);
            break;
        default: break;
        }
        return;
    }
    if (updateSnapshot_.phase == UpdatePhase::Confirming) {
        switch (action) {
        case Action::Confirm: onInstallUpdate_(); break;
        case Action::Back: onCancelUpdate_(); break;
        case Action::Up: notesScroll_ = std::max(0, notesScroll_ - 1); break;
        case Action::Down: ++notesScroll_; break;
        default: break;
        }
        return;
    }
    if (categories_.empty()) return;
    switch (action) {
    case Action::Up:
        if (optionsFocused_) {
            if (optionCursor_ > 0) --optionCursor_;
        } else if (categoryCursor_ > 0) {
            --categoryCursor_;
            optionCursor_ = std::min(optionCursor_, int(categories_[size_t(categoryCursor_)].rows.size()) - 1);
        }
        break;
    case Action::Down:
        if (optionsFocused_) {
            if (optionCursor_ + 1 < int(categories_[size_t(categoryCursor_)].rows.size())) ++optionCursor_;
        } else if (categoryCursor_ + 1 < int(categories_.size())) {
            ++categoryCursor_;
            optionCursor_ = std::min(optionCursor_, int(categories_[size_t(categoryCursor_)].rows.size()) - 1);
        }
        break;
    case Action::Left:
        optionsFocused_ = false;
        break;
    case Action::Right:
        optionsFocused_ = true;
        break;
    case Action::Back:
        optionsFocused_ = false;
        break;
    case Action::Confirm:
        if (!optionsFocused_) {
            optionsFocused_ = true;
        } else {
            const auto &rows = categories_[size_t(categoryCursor_)].rows;
            if (!rows.empty() && rows[size_t(optionCursor_)].enabled &&
                rows[size_t(optionCursor_)].activate) {
                const auto activate = rows[size_t(optionCursor_)].activate;
                activate();
            }
        }
        break;
    default:
        break;
    }
}

void SettingsScreen::prepareChangelogLines(const std::string &text) {
    changelogLines_.clear();
    changelogMaxScroll_ = 0;
    changelogScroll_ = 0;
    Theme &theme = context_.theme;
    const int maximumWidth = std::max(theme.px(120),
        std::min(theme.screenWidth() - theme.px(100), theme.px(1100)) -
        theme.px(88));
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        std::string raw = text.substr(start, end == std::string::npos ? end : end - start);
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        for (size_t markerPos = raw.find("**"); markerPos != std::string::npos;
             markerPos = raw.find("**", markerPos))
            raw.erase(markerPos, 2);
        raw.erase(std::remove(raw.begin(), raw.end(), '`'), raw.end());
        bool heading = false;
        size_t marker = 0;
        while (marker < raw.size() && raw[marker] == '#' && marker < 6) ++marker;
        if (marker > 0 && marker < raw.size() && raw[marker] == ' ') {
            heading = true;
            raw.erase(0, marker + 1);
        }
        if (raw.empty()) {
            changelogLines_.push_back({{}, false, true, theme.px(10)});
        } else {
            const int size = heading ? theme.sizeBody() : theme.sizeSmall();
            Font &font = heading ? theme.bold() : theme.regular();
            std::istringstream words(raw);
            std::string word, line;
            auto commitLine = [&] {
                if (line.empty()) return;
                changelogLines_.push_back({line, heading, false,
                    font.lineHeight(size) + theme.px(4)});
                line.clear();
            };
            while (words >> word) {
                if (font.measure(word, size) > maximumWidth) {
                    if (!line.empty()) commitLine();
                    size_t offset = 0;
                    while (offset < word.size()) {
                        size_t end = offset;
                        size_t accepted = offset;
                        while (end < word.size()) {
                            const unsigned char lead = static_cast<unsigned char>(word[end]);
                            const size_t count = lead < 0x80 ? 1 : lead < 0xe0 ? 2 :
                                                 lead < 0xf0 ? 3 : 4;
                            if (font.measure(word.substr(offset, end + count - offset), size) >
                                maximumWidth && accepted > offset) break;
                            end += std::min(count, word.size() - end);
                            accepted = end;
                            if (font.measure(word.substr(offset, accepted - offset), size) >
                                maximumWidth) break;
                        }
                        if (accepted == offset) accepted = std::min(word.size(), offset + 1);
                        line = word.substr(offset, accepted - offset);
                        offset = accepted;
                        if (offset < word.size()) commitLine();
                    }
                    continue;
                }
                const std::string candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && font.measure(candidate, size) > maximumWidth) {
                    commitLine();
                    line = word;
                } else {
                    line = candidate;
                }
            }
            commitLine();
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
}

void SettingsScreen::renderChangelog(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(215));
    const int width = std::min(area.w - theme.px(40), theme.px(1100));
    const int height = std::min(area.h - theme.px(40), theme.px(820));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                     width, height};
    canvas.dropShadow(panel, theme.radius(), theme.px(24), theme.shadow.withAlpha(150));
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    canvas.strokeRoundedRect(panel, theme.radius(), std::max(1, theme.px(2)), theme.focus);

    const int margin = theme.px(30);
    const int left = panel.x + margin;
    const int right = panel.right() - margin;
    int y = panel.y + margin;
    theme.bold().draw(canvas, left, y, "Changelog", theme.sizeHeading(), theme.textPrimary);
    y += theme.bold().lineHeight(theme.sizeHeading()) + theme.px(4);
    theme.regular().draw(canvas, left, y, "Full project changelog · fetched from GitHub",
                         theme.sizeSmall(), theme.textMuted);
    y += theme.regular().lineHeight(theme.sizeSmall()) + theme.px(10);

    const int footerSize = theme.sizeSmall();
    const int footerHeight = theme.regular().lineHeight(footerSize) + theme.px(4);
    const int footerY = panel.bottom() - margin - footerHeight;
    const int bottom = footerY - theme.px(12);
    if (updateSnapshot_.changelogLoading) {
        theme.regular().draw(canvas, left, y, "Loading the full changelog from GitHub…",
                             theme.sizeBody(), theme.textMuted);
        theme.regular().draw(canvas, left, footerY, "B Close", footerSize, theme.accent);
        return;
    }
    if (!updateSnapshot_.changelogError.empty()) {
        const std::string message = theme.regular().elide(
            "Could not load the changelog: " + updateSnapshot_.changelogError,
            theme.sizeBody(), right - left);
        theme.regular().draw(canvas, left, y, message, theme.sizeBody(), theme.warning);
        theme.regular().draw(canvas, left, footerY, "A Try again   B Close", footerSize,
                             theme.accent);
        return;
    }
    if (changelogLines_.empty()) {
        theme.regular().draw(canvas, left, y, "The changelog is empty.",
                             theme.sizeBody(), theme.textMuted);
        theme.regular().draw(canvas, left, footerY, "B Close", footerSize, theme.accent);
        return;
    }

    int contentHeight = 0;
    for (const ChangelogLine &line : changelogLines_) contentHeight += line.height;
    const int availableHeight = std::max(0, bottom - y);
    changelogMaxScroll_ = 0;
    while (contentHeight > availableHeight &&
           changelogMaxScroll_ < int(changelogLines_.size()))
        contentHeight -= changelogLines_[size_t(changelogMaxScroll_++)].height;
    changelogScroll_ = std::min(changelogScroll_, changelogMaxScroll_);

    const int arrowWidth = theme.px(24);
    const int textRight = right - arrowWidth - theme.px(8);
    const Rect clip{left, y, textRight - left, availableHeight};
    canvas.pushClip(clip);
    for (int i = changelogScroll_; i < int(changelogLines_.size()); ++i) {
        const ChangelogLine &line = changelogLines_[size_t(i)];
        if (y + line.height > bottom) break;
        if (!line.blank) {
            Font &font = line.heading ? theme.bold() : theme.regular();
            const int size = line.heading ? theme.sizeBody() : theme.sizeSmall();
            font.draw(canvas, left, y, line.text, size, theme.textPrimary);
        }
        y += line.height;
    }
    canvas.popClip();

    const Rect up{right - arrowWidth, clip.y, arrowWidth, theme.px(24)};
    const Rect down{right - arrowWidth, bottom - theme.px(24), arrowWidth, theme.px(24)};
    if (changelogScroll_ > 0)
        theme.bold().drawCentered(canvas, up, "↑", theme.sizeSmall(), theme.focus);
    if (changelogScroll_ < changelogMaxScroll_)
        theme.bold().drawCentered(canvas, down, "↓", theme.sizeSmall(), theme.focus);
    theme.regular().draw(canvas, left, footerY, "B Close   ↑/↓ Scroll",
                         footerSize, theme.accent);
}

void SettingsScreen::startWifiSetup() {
    std::string error;
    if (!wifi_.startScan(error)) {
        context_.notify(error);
        return;
    }
    wifiNetworks_.clear();
    wifiMessage_ = "Scanning for networks…";
    wifiCursor_ = 0;
    wifiScroll_ = 0;
    wifiStep_ = WifiStep::Scanning;
}

void SettingsScreen::cancelWifiFlow(bool restoreConnection) {
    if (restoreConnection && wifi_.testing()) wifi_.rollback();
    std::fill(wifiPassword_.begin(), wifiPassword_.end(), '\0');
    wifiPassword_.clear();
    wifiSsid_.clear();
    wifiMessage_.clear();
    wifiNetworks_.clear();
    wifiKeyboard_.clear();
    wifiKeyboard_.setMasked(false);
    wifiStep_ = WifiStep::None;
}

void SettingsScreen::handleWifiFlow(Action action) {
    if (wifiStep_ == WifiStep::SsidEntry || wifiStep_ == WifiStep::PasswordEntry ||
        wifiStep_ == WifiStep::CountryEntry) {
        const bool enteringSsid = wifiStep_ == WifiStep::SsidEntry;
        const bool enteringCountry = wifiStep_ == WifiStep::CountryEntry;
        if (action == Action::Back) {
            wifiKeyboard_.clear();
            wifiKeyboard_.setMasked(false);
            wifiStep_ = enteringCountry ? WifiStep::TestReview : WifiStep::Networks;
            return;
        }
        const OnScreenKeyboard::Result result =
            action == Action::Start && wifiStep_ == WifiStep::PasswordEntry
                ? OnScreenKeyboard::Result::Submitted : wifiKeyboard_.handle(action);
        if (result != OnScreenKeyboard::Result::Submitted) return;
        if (enteringCountry) {
            const std::string country = wifiKeyboard_.value();
            wifiKeyboard_.clear();
            std::string error;
            if (!wifi_.setCountryCode(country, error)) {
                wifiMessage_ = error;
                return;
            }
            if (wifi_.saveTestedNetwork(error)) {
                context_.notify("Wi-Fi saved. Restart MiSTer to connect automatically.", 6.0f);
                cancelWifiFlow(false);
            } else {
                wifiMessage_ = error;
                wifiStep_ = WifiStep::TestReview;
            }
        } else if (enteringSsid) {
            wifiSsid_ = wifiKeyboard_.value();
            if (wifiSsid_.empty() || wifiSsid_.size() > 32) {
                wifiMessage_ = "Enter a network name with 1 to 32 characters";
                return;
            }
            wifiKeyboard_.clear();
            wifiKeyboard_.setMasked(true);
            wifiMessage_ = "Enter the password for " + wifiSsid_;
            wifiStep_ = WifiStep::PasswordEntry;
        } else {
            wifiPassword_ = wifiKeyboard_.value();
            wifiKeyboard_.clear();
            wifiKeyboard_.setMasked(false);
            if (wifiPassword_.size() < 8 || wifiPassword_.size() > 63) {
                std::fill(wifiPassword_.begin(), wifiPassword_.end(), '\0');
                wifiPassword_.clear();
                wifiKeyboard_.clear();
                wifiMessage_ = "Password must be 8 to 63 characters";
                wifiStep_ = WifiStep::PasswordEntry;
                return;
            }
            wifiStep_ = WifiStep::TestPrompt;
        }
        return;
    }

    if (action == Action::Back) {
        if (wifiStep_ == WifiStep::Testing) {
            cancelWifiFlow(true);
        } else if (wifiStep_ == WifiStep::TestReview) {
            cancelWifiFlow(true);
        } else if (wifiStep_ == WifiStep::PasswordEntry) {
            wifiStep_ = WifiStep::Networks;
        } else {
            cancelWifiFlow(false);
        }
        return;
    }

    switch (wifiStep_) {
    case WifiStep::Scanning:
        break;
    case WifiStep::Networks: {
        const int hiddenRow = int(wifiNetworks_.size());
        const int rescanRow = hiddenRow + 1;
        if (action == Action::Up && wifiCursor_ > 0) --wifiCursor_;
        else if (action == Action::Down && wifiCursor_ < rescanRow) ++wifiCursor_;
        else if (action == Action::Confirm) {
            if (wifiCursor_ == hiddenRow) {
                wifiHidden_ = true;
                wifiOpen_ = false;
                wifiSsid_.clear();
                wifiMessage_ = "Enter the hidden network name";
                wifiKeyboard_.clear();
                wifiKeyboard_.setMasked(false);
                wifiStep_ = WifiStep::SsidEntry;
            } else if (wifiCursor_ == rescanRow) {
                startWifiSetup();
            } else if (wifiCursor_ >= 0 && wifiCursor_ < hiddenRow) {
                const Wifi::Network &network = wifiNetworks_[size_t(wifiCursor_)];
                if (!network.supported) {
                    wifiMessage_ = "This network uses an authentication type not supported here";
                    return;
                }
                wifiSsid_ = network.ssid;
                wifiOpen_ = network.open;
                wifiHidden_ = false;
                if (wifiOpen_) wifiStep_ = WifiStep::TestPrompt;
                else {
                    wifiKeyboard_.clear();
                    wifiKeyboard_.setMasked(true);
                    wifiMessage_ = "Enter the password for " + wifiSsid_;
                    wifiStep_ = WifiStep::PasswordEntry;
                }
            }
        }
        break;
    }
    case WifiStep::TestPrompt:
        if (action == Action::Confirm) {
            std::string error;
            if (!wifi_.beginTest(wifiSsid_, wifiPassword_, wifiOpen_, wifiHidden_, error)) {
                wifiMessage_ = error;
                std::fill(wifiPassword_.begin(), wifiPassword_.end(), '\0');
                wifiPassword_.clear();
                wifiStep_ = WifiStep::TestFailed;
            } else {
                std::fill(wifiPassword_.begin(), wifiPassword_.end(), '\0');
                wifiPassword_.clear();
                wifiMessage_ = "Joining " + wifiSsid_ + "…";
                wifiStep_ = WifiStep::Testing;
            }
        }
        break;
    case WifiStep::Testing:
        break;
    case WifiStep::TestReview:
        if (action == Action::Confirm) {
            if (wifi_.needsCountryCode()) {
        wifiMessage_ = "MiSTer did not report your country. Enter its two-letter country code, for example US.";
                wifiKeyboard_.clear();
                wifiKeyboard_.setMasked(false);
                wifiStep_ = WifiStep::CountryEntry;
                break;
            }
            std::string error;
            if (wifi_.saveTestedNetwork(error)) {
                context_.notify("Wi-Fi saved. Restart MiSTer to connect automatically.", 6.0f);
                cancelWifiFlow(false);
            } else {
                wifiMessage_ = error;
                wifiStep_ = WifiStep::TestReview;
            }
        }
        break;
    case WifiStep::TestFailed:
        if (action == Action::Confirm) {
            if (wifiOpen_) wifiStep_ = WifiStep::TestPrompt;
            else {
                wifiKeyboard_.clear();
                wifiKeyboard_.setMasked(true);
                wifiStep_ = WifiStep::PasswordEntry;
            }
        }
        break;
    case WifiStep::SsidEntry:
    case WifiStep::PasswordEntry:
    case WifiStep::CountryEntry:
    case WifiStep::None:
        break;
    }
}

void SettingsScreen::renderWifiFlow(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(215));
    const int width = std::min(area.w - theme.px(40), theme.px(1000));
    const int height = std::min(area.h - theme.px(40), theme.px(760));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                     width, height};
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    const int left = panel.x + theme.px(28);
    const int right = panel.right() - theme.px(28);
    int y = panel.y + theme.px(24);

    std::string title;
    switch (wifiStep_) {
    case WifiStep::Scanning: title = "Connect to Wi-Fi"; break;
    case WifiStep::Networks: title = "Choose the Wi-Fi network to join"; break;
    case WifiStep::SsidEntry: title = "Hidden Wi-Fi network"; break;
    case WifiStep::PasswordEntry: title = "Wi-Fi password"; break;
    case WifiStep::CountryEntry: title = "Wi-Fi country"; break;
    case WifiStep::TestPrompt: title = "Test connection"; break;
    case WifiStep::Testing: title = "Connecting…"; break;
    case WifiStep::TestReview: title = "Connection test successful"; break;
    case WifiStep::TestFailed: title = "Connection test failed"; break;
    case WifiStep::None: return;
    }
    theme.bold().draw(canvas, left, y, title, theme.sizeHeading(), theme.textPrimary);
    y += theme.bold().lineHeight(theme.sizeHeading()) + theme.px(12);

    if (wifiStep_ == WifiStep::Scanning) {
        theme.regular().draw(canvas, left, y, wifiMessage_, theme.sizeBody(), theme.textMuted);
    } else if (wifiStep_ == WifiStep::Networks) {
        theme.regular().draw(canvas, left, y, wifiMessage_, theme.sizeBody(), theme.textMuted);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(14);
        const int rowHeight = std::min(theme.px(64),
                                       std::max(theme.px(48), (panel.h - theme.px(150)) / 7));
        const int visible = std::max(1, (panel.bottom() - y - theme.px(55)) / rowHeight);
        const int rows = int(wifiNetworks_.size()) + 2;
        if (wifiCursor_ < wifiScroll_) wifiScroll_ = wifiCursor_;
        if (wifiCursor_ >= wifiScroll_ + visible) wifiScroll_ = wifiCursor_ - visible + 1;
        wifiScroll_ = std::min(std::max(0, wifiScroll_), std::max(0, rows - visible));
        const int end = std::min(rows, wifiScroll_ + visible);
        for (int i = wifiScroll_; i < end; ++i) {
            const Rect row{left, y + (i - wifiScroll_) * rowHeight, right - left,
                           rowHeight - theme.px(8)};
            const bool selected = i == wifiCursor_;
            canvas.fillRoundedRect(row, theme.px(8),
                                   selected ? theme.surface
                                            : theme.surface.withAlpha(110));
            if (selected)
                canvas.fillRoundedRect({row.x, row.y, theme.px(4), row.h}, theme.px(2),
                                       theme.accent);
            if (i < int(wifiNetworks_.size())) {
                const Wifi::Network &network = wifiNetworks_[size_t(i)];
                const int textY = row.y + (row.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
                const int leftText = row.x + theme.px(18);
                const int rightText = row.right() - theme.px(14);
                const std::string details = (network.open ? "Open" :
                    network.supported ? "Secured" : "Unsupported") +
                    std::string("   ") + std::to_string(network.signalDbm) + " dBm";
                const int detailsWidth = theme.regular().measure(details, theme.sizeSmall());
                const int detailsX = std::max(leftText, rightText - detailsWidth);
                theme.regular().draw(canvas, leftText, textY,
                    theme.regular().elide(network.ssid, theme.sizeBody(),
                                          detailsX - leftText - theme.px(12)),
                    theme.sizeBody(), selected ? theme.textPrimary : theme.textMuted);
                theme.regular().draw(canvas, detailsX, textY,
                    theme.regular().elide(details, theme.sizeSmall(), rightText - detailsX),
                    theme.sizeSmall(), selected ? theme.textPrimary : theme.textMuted);
            } else {
                const int textY = row.y + (row.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
                theme.regular().draw(canvas, row.x + theme.px(18), textY,
                    i == int(wifiNetworks_.size()) ? "Connect to hidden Wi-Fi access point"
                                                   : "Scan again",
                    theme.sizeBody(), selected ? theme.textPrimary : theme.textMuted);
            }
        }
    } else if (wifiStep_ == WifiStep::SsidEntry || wifiStep_ == WifiStep::PasswordEntry ||
               wifiStep_ == WifiStep::CountryEntry) {
        const std::string prompt = wifiStep_ == WifiStep::SsidEntry ? "Wi-Fi network name" :
            wifiStep_ == WifiStep::CountryEntry ? "Two-letter country code (for example, US)" :
            "Network: " + wifiSsid_ + "  —  enter password";
        theme.regular().draw(canvas, left, y, prompt, theme.sizeBody(), theme.textPrimary);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(10);
        const Rect field{left, y, right - left, theme.px(54)};
        canvas.fillRoundedRect(field, theme.radius(), theme.surface);
        canvas.strokeRoundedRect(field, theme.radius(), theme.px(2), theme.accent);
        const std::string entered = wifiKeyboard_.value();
        const std::string display = wifiKeyboard_.masked()
                                        ? std::string(entered.size(), '*') : entered;
        const std::string placeholder = wifiStep_ == WifiStep::PasswordEntry
                                            ? "Enter password…" : "Enter text…";
        const std::string fieldText = display.empty() ? placeholder : display + "_";
        theme.regular().draw(canvas, field.x + theme.px(16),
            field.y + (field.h - theme.regular().lineHeight(theme.sizeBody())) / 2,
            theme.regular().elide(fieldText, theme.sizeBody(), field.w - theme.px(32)),
            theme.sizeBody(), display.empty() ? theme.textMuted : theme.textPrimary);
        y += field.h + theme.px(12);
        if (!wifiMessage_.empty()) {
            theme.regular().draw(canvas, left, y, wifiMessage_, theme.sizeSmall(), theme.textMuted);
            y += theme.px(10);
        }
        const int keyboardPadding = theme.px(12);
        const Rect keyboardArea{left + keyboardPadding, y + keyboardPadding,
                                right - left - keyboardPadding * 2,
                                panel.bottom() - y - theme.px(48) - keyboardPadding * 2};
        const Rect keys = wifiKeyboard_.bounds(theme, keyboardArea);
        const Rect keyboardPanel{keys.x - keyboardPadding, keys.y - keyboardPadding,
                                 keys.w + keyboardPadding * 2,
                                 keys.h + keyboardPadding * 2};
        canvas.fillRoundedRect(keyboardPanel, theme.radius(), theme.surface.withAlpha(235));
        wifiKeyboard_.render(canvas, theme, keyboardArea);
    } else {
        theme.regular().draw(canvas, left, y, "Network: " + wifiSsid_,
                             theme.sizeBody(), theme.textPrimary);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(16);
        const int bodyWidth = right - left;
        std::vector<std::string> lines;
        if (wifiStep_ == WifiStep::TestPrompt) {
            lines = {"The test temporarily switches the Wi-Fi connection.",
                     "If it fails or you cancel, the previous connection will be restored."};
        } else if (wifiStep_ == WifiStep::Testing) {
            lines = {wifiMessage_, "Waiting for the network and an IP address…"};
        } else if (wifiStep_ == WifiStep::TestReview) {
            lines = {wifiMessage_, "Save this network so MiSTer reconnects after restart?"};
        } else {
            lines = {wifiMessage_};
        }
        for (const std::string &line : lines) {
            theme.regular().draw(canvas, left, y, theme.regular().elide(
                line, theme.sizeBody(), bodyWidth), theme.sizeBody(), theme.textPrimary);
            y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(9);
        }
    }

    std::string hint;
    if (wifiStep_ == WifiStep::Networks) hint = "A Join selected network   B Cancel   ↑/↓ Move";
    else if (wifiStep_ == WifiStep::SsidEntry || wifiStep_ == WifiStep::PasswordEntry ||
             wifiStep_ == WifiStep::CountryEntry)
        hint = wifiStep_ == WifiStep::PasswordEntry
                   ? "D-pad Choose key   A Type   Start Done   Show/Hide   B Back"
                   : "D-pad Choose key   A Type   B Back   X Delete   Y Space";
    else if (wifiStep_ == WifiStep::Testing) hint = "B Cancel and restore the previous connection";
    else if (wifiStep_ == WifiStep::TestPrompt) hint = "A Test connection   B Cancel";
    else if (wifiStep_ == WifiStep::TestReview) hint = "A Save these settings   B Restore previous connection";
    else if (wifiStep_ == WifiStep::TestFailed) hint = "A Try again   B Cancel";
    else hint = "Searching for Wi-Fi networks…   B Cancel";
    theme.regular().draw(canvas, left, panel.bottom() - theme.px(34), formatButtonLabels(hint),
                         theme.sizeSmall(), theme.accent);
}

void SettingsScreen::handleTimeZoneChooser(Action action) {
    const int count = choosingTimeZoneCity_ ? int(timeZoneCities_.size())
                                            : int(timeZoneRegions_.size());
    switch (action) {
    case Action::Up:
        if (timeZoneCursor_ > 0) --timeZoneCursor_;
        break;
    case Action::Down:
        if (timeZoneCursor_ + 1 < count) ++timeZoneCursor_;
        break;
    case Action::Back:
        if (choosingTimeZoneCity_) {
            choosingTimeZoneCity_ = false;
            timeZoneCursor_ = 0;
            timeZoneScroll_ = 0;
        } else {
            timeZoneChooserActive_ = false;
        }
        break;
    case Action::Confirm:
        if (count <= 0 || timeZoneCursor_ < 0 || timeZoneCursor_ >= count) break;
        if (!choosingTimeZoneCity_) {
            timeZoneRegionCursor_ = timeZoneCursor_;
            timeZoneCities_ = Preferences::timeZonesIn(
                timeZoneRegions_[size_t(timeZoneCursor_)]);
            if (timeZoneCities_.empty()) {
                context_.notify("No cities found in this region. Choose another region.");
                break;
            }
            choosingTimeZoneCity_ = true;
            timeZoneCursor_ = 0;
            timeZoneScroll_ = 0;
            const std::string &currentZone = context_.preferences.timeZone();
            const size_t slash = currentZone.find('/');
            if (slash != std::string::npos &&
                currentZone.substr(0, slash) == timeZoneRegions_[size_t(timeZoneRegionCursor_)]) {
                const std::string currentCity = currentZone.substr(slash + 1);
                for (size_t i = 0; i < timeZoneCities_.size(); ++i)
                    if (timeZoneCities_[i] == currentCity) timeZoneCursor_ = int(i);
            }
            break;
        } else {
            const std::string zone = timeZoneRegions_[size_t(timeZoneRegionCursor_)] + "/" +
                                     timeZoneCities_[size_t(timeZoneCursor_)];
            if (context_.preferences.setTimeZone(zone)) {
                context_.notify("Clock time zone set to " + zone);
                timeZoneChooserActive_ = false;
            } else {
                context_.notify("Could not save the time zone. Check that MiSTer can write its system settings.");
            }
        }
        break;
    default:
        break;
    }
}

void SettingsScreen::renderTimeZoneChooser(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(205));
    const int panelWidth = std::min(area.w - theme.px(48), theme.px(900));
    const Rect panel{area.x + (area.w - panelWidth) / 2, area.y + theme.px(28),
                     panelWidth, area.h - theme.px(56)};
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);

    const std::string title = choosingTimeZoneCity_
        ? "Choose city — " + timeZoneRegions_[size_t(timeZoneRegionCursor_)]
        : "Choose continent / region";
    const int left = panel.x + theme.px(28);
    const int top = panel.y + theme.px(24);
    theme.bold().draw(canvas, left, top, title, theme.sizeHeading(), theme.textPrimary);

    const std::vector<std::string> &items = choosingTimeZoneCity_
        ? timeZoneCities_ : timeZoneRegions_;
    const int rowHeight = theme.px(44);
    const int listY = top + theme.px(54);
    const int visible = std::max(1, (panel.bottom() - listY - theme.px(54)) / rowHeight);
    if (timeZoneCursor_ < timeZoneScroll_) timeZoneScroll_ = timeZoneCursor_;
    if (timeZoneCursor_ >= timeZoneScroll_ + visible)
        timeZoneScroll_ = timeZoneCursor_ - visible + 1;
    const int maxScroll = std::max(0, int(items.size()) - visible);
    timeZoneScroll_ = std::min(std::max(0, timeZoneScroll_), maxScroll);

    const Rect clip{panel.x + theme.px(16), listY, panel.w - theme.px(32),
                    panel.bottom() - listY - theme.px(46)};
    canvas.pushClip(clip);
    for (int i = timeZoneScroll_; i < std::min(int(items.size()), timeZoneScroll_ + visible); ++i) {
        const Rect row{panel.x + theme.px(16), listY + (i - timeZoneScroll_) * rowHeight,
                       panel.w - theme.px(32), rowHeight - theme.px(4)};
        if (i == timeZoneCursor_)
            canvas.fillRoundedRect(row, theme.px(7), theme.accent.withAlpha(48));
        theme.regular().draw(canvas, row.x + theme.px(14),
                             row.y + (row.h - theme.regular().lineHeight(theme.sizeBody())) / 2,
                             items[size_t(i)], theme.sizeBody(),
                             i == timeZoneCursor_ ? theme.textPrimary : theme.textMuted);
    }
    canvas.popClip();
    theme.regular().draw(canvas, left, panel.bottom() - theme.px(34),
                         choosingTimeZoneCity_ ? "A Set Linux time zone   B Back"
                                               : "A Choose   B Cancel",
                         theme.sizeSmall(), theme.textMuted);
}

void SettingsScreen::renderCategories(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    const int rowHeight = std::min(theme.px(64), std::max(theme.px(48), area.h / 7));

    for (size_t i = 0; i < categories_.size(); ++i) {
        const Rect frame{area.x, area.y + int(i) * rowHeight, area.w, rowHeight - theme.px(8)};
        if (frame.bottom() > area.bottom()) break;

        const bool selected = int(i) == categoryCursor_;
        const bool active = selected && !optionsFocused_;
        canvas.fillRoundedRect(frame, theme.px(8),
                               active ? theme.surfaceHi :
                               selected ? theme.surface.withAlpha(190) : theme.surface.withAlpha(110));
        if (active) {
            canvas.fillRoundedRect({frame.x, frame.y, theme.px(4), frame.h}, theme.px(2),
                                   theme.accent);
            canvas.strokeRoundedRect(frame, theme.px(8), std::max(1, theme.px(2)), theme.focus);
        }

        const int textY = frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
        const int left = frame.x + theme.px(18);
        const int right = frame.right() - theme.px(12);
        const std::string label = theme.regular().elide(
            categories_[i].label, theme.sizeBody(), std::max(0, right - left));
        theme.regular().draw(canvas, left, textY, label, theme.sizeBody(),
                             selected ? theme.textPrimary : theme.textMuted);
    }
}

void SettingsScreen::renderOptions(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    const auto &rows = categories_[size_t(categoryCursor_)].rows;
    const int rowHeight = std::min(theme.px(64),
                                   std::max(theme.px(48), area.h / 7));
    const int visible = std::max(1, area.h / rowHeight);
    const int first = std::max(0, optionCursor_ - visible + 1);

    for (size_t i = size_t(first); i < rows.size(); ++i) {
        const Row &row = rows[i];
        const Rect frame{area.x, area.y + (int(i) - first) * rowHeight,
                         area.w, rowHeight - theme.px(8)};
        if (frame.bottom() > area.bottom()) break;

        const bool active = optionsFocused_ && int(i) == optionCursor_;
        if (int(i) == optionCursor_) {
            canvas.fillRoundedRect(frame, theme.px(8),
                                   active ? theme.surfaceHi : theme.surface.withAlpha(190));
        }
        if (active) {
            canvas.fillRoundedRect({frame.x, frame.y, theme.px(4), frame.h}, theme.px(2),
                                   theme.accent);
            canvas.strokeRoundedRect(frame, theme.px(8), std::max(1, theme.px(2)), theme.focus);
        }

        const int textY = frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
        const std::string value = row.value ? formatButtonLabels(row.value()) : std::string();
        const int left = frame.x + theme.px(18);
        const int right = frame.right() - theme.px(14);
        const bool toggleValue = value == "On" || value == "Off";
        const int valueWidth = value.empty() ? 0 : toggleValue
            ? std::max(theme.regular().measure("On", theme.sizeBody()),
                       theme.regular().measure("Off", theme.sizeBody()))
            : theme.regular().measure(value, theme.sizeBody());
        const int checkboxSize = toggleValue ? theme.px(20) : 0;
        const int checkboxGap = toggleValue ? theme.px(10) : 0;
        const int labelWidth = std::max(0, right - left -
            (value.empty() ? 0 : valueWidth + theme.px(10) + checkboxSize + checkboxGap));
        const std::string label = theme.regular().elide(row.label, theme.sizeBody(), labelWidth);
        theme.regular().draw(canvas, left, textY, label, theme.sizeBody(),
                             row.enabled && (active || int(i) == optionCursor_)
                                 ? theme.textPrimary : theme.textMuted);

        if (!value.empty()) {
            theme.regular().draw(canvas, right - valueWidth, textY, value,
                                 theme.sizeBody(), theme.accent);
        }
        if (toggleValue) {
            const int x = right - valueWidth - checkboxGap - checkboxSize;
            const int y = frame.y + (frame.h - checkboxSize) / 2;
            const Rect outer{x, y, checkboxSize, checkboxSize};
            const int border = std::max(1, theme.px(2));
            canvas.strokeRoundedRect(outer, checkboxSize / 2, border,
                                     Color::rgb(0x17315F));
            if (value == "On") {
                const int inset = theme.px(5);
                const Rect inner{x + inset, y + inset,
                                 checkboxSize - 2 * inset, checkboxSize - 2 * inset};
                canvas.fillRoundedRect(inner, inner.w / 2, Color::rgb(0xD6D8DE));
            }
        }
    }

}

void SettingsScreen::renderSettingHelp(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    if (area.w <= 0 || area.h <= 0) return;

    canvas.fillRoundedRect(area, theme.radius(), theme.surface.withAlpha(70));
    const int inset = theme.px(28);
    const int left = area.x + inset;
    const int right = area.right() - inset;
    int y = area.y + inset;

    const auto &rows = categories_[size_t(categoryCursor_)].rows;
    if (optionCursor_ < 0 || optionCursor_ >= int(rows.size())) return;
    const Row &row = rows[size_t(optionCursor_)];
    const int titleSize = theme.sizeHeading();
    const std::vector<std::string> titleLines = wrapHelp(
        theme.bold(), row.label, titleSize, std::max(0, right - left));
    const int titleStep = theme.bold().lineHeight(titleSize) + theme.px(4);
    for (const std::string &line : titleLines) {
        if (y + titleStep > area.bottom() - inset) return;
        theme.bold().draw(canvas, left, y, line, titleSize, theme.textPrimary);
        y += titleStep;
    }
    y += theme.px(14);

    const std::string help = settingHelp(row.label);
    if (help.empty()) return;
    const int helpSize = theme.sizeBody();
    const int lineStep = theme.regular().lineHeight(helpSize) + theme.px(8);
    const std::vector<std::string> lines = wrapHelp(
        theme.regular(), help, helpSize, std::max(0, right - left));
    for (const std::string &line : lines) {
        if (y + lineStep > area.bottom() - inset) break;
        theme.regular().draw(canvas, left, y, line, helpSize, theme.textPrimary);
        y += lineStep;
    }
}

void SettingsScreen::renderInfo(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;

    canvas.fillRoundedRect(area, theme.radius(), theme.surface.withAlpha(110));

    const Rect body = area.inset(theme.px(24));
    int y = body.y;

    const int factSize = theme.sizeBody();
    const int lineStep = theme.regular().lineHeight(factSize) + theme.px(3);
    const int headingStep = theme.bold().lineHeight(theme.sizeBody()) + theme.px(8);
    const int columnGap = theme.px(16);
    const std::vector<Fact> allFacts = facts();
    int labelColumnWidth = 0;
    for (const Fact &fact : allFacts) {
        if (fact.label.empty() || fact.value.empty()) continue;
        labelColumnWidth = std::max(labelColumnWidth,
                                    theme.regular().measure(fact.label, factSize));
    }
    labelColumnWidth = std::min(labelColumnWidth, body.w * 38 / 100);

    for (const Fact &fact : allFacts) {
        if (y + lineStep > body.bottom()) break;

        if (fact.label.empty() && fact.value.empty()) {   // spacer between groups
            y += theme.px(6);
            continue;
        }

        if (fact.value.empty()) {                          // group heading
            theme.bold().draw(canvas, body.x, y, fact.label, theme.sizeBody(),
                              theme.textPrimary);
            y += headingStep;
            continue;
        }

        const std::string label =
            theme.regular().elide(fact.label, factSize, labelColumnWidth);
        theme.regular().draw(canvas, body.x, y, label, factSize,
                             theme.textMuted.withAlpha(170));

        const int valueX = body.x + labelColumnWidth + columnGap;
        const std::string value = theme.regular().elide(
            fact.value, factSize, std::max(0, body.right() - valueX));
        theme.regular().draw(canvas, valueX, y, value, factSize,
                             theme.textPrimary);
        y += lineStep;
    }
}

void SettingsScreen::renderDiscDriveSettings(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(210));
    const int width = std::min(area.w - theme.px(48), theme.px(980));
    const int height = std::min(area.h - theme.px(48), theme.px(650));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                     width, height};
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    const int margin = theme.px(30);
    int y = panel.y + margin;
    theme.bold().draw(canvas, panel.x + margin, y, "Disc drive", theme.sizeHeading(),
                      theme.textPrimary);
    y += theme.px(58);

    const auto drive = context_.discDrive.snapshot();
    const auto cores = context_.discCores.snapshot();
    const auto line = [&](const std::string &label, const std::string &value, Color color) {
        theme.bold().draw(canvas, panel.x + margin, y, label, theme.sizeBody(), theme.textPrimary);
        theme.regular().draw(canvas, panel.x + margin + theme.px(330), y, value,
                             theme.sizeBody(), color);
        y += theme.px(38);
    };
    line("Drive", drive.device.empty() ? "Not available" : drive.device + "  " + drive.model,
         drive.device.empty() ? theme.warning : theme.accent);
    line("Disc status", DiscDriveService::stateName(drive.state), theme.textMuted);
    line("Physical-Disc Main", !cores.alternateMainInstalled ? "Missing" :
             cores.alternateMainVerified
                 ? std::string("MiSTer-disc v") + DiscCorePackage::kVersion + " verified"
                 : "Present, unverified",
         cores.alternateMainVerified ? theme.accent : theme.warning);
    line("Scoped [CD-*] setting", cores.scopedMainConfigured ? "Configured" : "Missing",
         cores.scopedMainConfigured ? theme.accent : theme.warning);
    line("PlayStation MGL", !cores.psxMglInstalled ? "Missing" :
             !cores.psxMglCompatible ? "Incompatible" :
             cores.psxMglVerified
                 ? std::string("v") + DiscCorePackage::kVersion + " verified"
                 : "Compatible, unverified",
         cores.psxMglVerified ? theme.accent : theme.warning);
    line("PlayStation RBF", cores.psxCoreInstalled ? "Installed" : "Missing",
         cores.psxCoreInstalled ? theme.accent : theme.warning);
    line("Saturn MGL", !cores.saturnMglInstalled ? "Missing" :
             !cores.saturnMglCompatible ? "Incompatible" :
             cores.saturnMglVerified
                 ? std::string("v") + DiscCorePackage::kVersion + " verified"
                 : "Compatible, unverified",
         cores.saturnMglVerified ? theme.accent : theme.warning);
    line("Saturn RBF", cores.saturnCoreInstalled ? "Installed" : "Missing",
         cores.saturnCoreInstalled ? theme.accent : theme.warning);
    y += theme.px(12);
    theme.regular().draw(canvas, panel.x + margin, y,
        "Use Install/update Disc cores in Settings to install or repair the scoped package. Global [MiSTer] main stays untouched.",
        theme.sizeSmall(), theme.textMuted);
    theme.regular().draw(canvas, panel.x + margin,
        panel.bottom() - margin - theme.regular().lineHeight(theme.sizeSmall()),
        DiscCoreManager::projectUrl(), theme.sizeSmall(), theme.accent);
}

void SettingsScreen::renderDiscCoreInstallConfirmation(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(220));
    const int width = std::min(area.w - theme.px(48), theme.px(850));
    const int height = std::min(area.h - theme.px(48), theme.px(340));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                     width, height};
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    const int margin = theme.px(30);
    const int left = panel.x + margin;
    int y = panel.y + margin;
    theme.bold().draw(canvas, left, y,
                      std::string("Install USB CDROM Loader v") + DiscCorePackage::kVersion + "?",
                      theme.sizeHeading(), theme.textPrimary);
    y += theme.bold().lineHeight(theme.sizeHeading()) + theme.px(18);
    const std::vector<std::string> lines = wrapHelp(
        theme.regular(),
        "This downloads and verifies the official package, backs up managed files and MiSTer.ini, and changes only [CD-*] main. Existing PlayStation or Saturn MGLs at the managed paths will be replaced.",
        theme.sizeBody(), panel.w - margin * 2);
    const int lineStep = theme.regular().lineHeight(theme.sizeBody()) + theme.px(8);
    for (const std::string &line : lines) {
        if (y + lineStep > panel.bottom() - margin - theme.px(32)) break;
        theme.regular().draw(canvas, left, y, line, theme.sizeBody(), theme.textPrimary);
        y += lineStep;
    }
    theme.bold().draw(canvas, left,
        panel.bottom() - margin - theme.bold().lineHeight(theme.sizeBody()),
        formatButtonLabels("A Install   B Cancel"), theme.sizeBody(), theme.accent);
}

void SettingsScreen::renderUpdateConfirmation(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(205));
    const int width = std::min(area.w - theme.px(40), theme.px(850));
    const int height = std::min(area.h - theme.px(40), theme.px(600));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                     width, height};
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    const int margin = theme.px(28);
    const int left = panel.x + margin;
    const int right = panel.right() - margin;
    int y = panel.y + margin;
    theme.bold().draw(canvas, left, y, "Install v" + updateSnapshot_.targetVersion + "?",
                      theme.sizeHeading(), theme.textPrimary);
    y += theme.bold().lineHeight(theme.sizeHeading()) + theme.px(12);
    theme.regular().draw(canvas, left, y,
                         updateSnapshot_.pinned
                             ? "The version pin stays active after download."
                             : "Restart MiSTer after installation to use the update.",
                         theme.sizeBody(), theme.textPrimary);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(8);
    if (updateSnapshot_.databaseRebuildHint) {
        theme.regular().draw(canvas, left, y,
            theme.regular().elide("The game list needs rebuilding after restart.",
                                  theme.sizeBody(), right - left),
            theme.sizeBody(), theme.accent);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(8);
    }
    y += theme.px(10);
    theme.bold().draw(canvas, left, y, "RELEASE NOTES", theme.sizeSmall(), theme.textMuted);
    y += theme.bold().lineHeight(theme.sizeSmall()) + theme.px(10);
    const int notesTop = y;
    const int noteTextRight = right - theme.px(28);
    struct NoteRun { std::string text; bool bold = false; };
    struct NoteLine { std::vector<NoteRun> runs; int size = 0; int height = 0; };
    std::vector<NoteLine> lines;
    const std::string &notes = updateSnapshot_.releaseNotes;
    auto addLogicalLine = [&](const std::string &raw) {
        size_t contentStart = 0;
        int level = 0;
        while (level < 6 && contentStart < raw.size() && raw[contentStart] == '#') {
            ++level; ++contentStart;
        }
        if (level == 0 || contentStart >= raw.size() || raw[contentStart] != ' ') {
            level = 0; contentStart = 0;
        } else {
            while (contentStart < raw.size() && raw[contentStart] == ' ') ++contentStart;
        }
        const int size = level == 1 ? theme.sizeBody()
                       : level == 2 ? theme.sizeSmall() + theme.px(2)
                       : theme.sizeSmall();
        const bool heading = level != 0;
        NoteLine current;
        current.size = size;
        current.height = (heading ? theme.bold() : theme.regular()).lineHeight(size) + theme.px(3);
        int currentWidth = 0;
        bool bold = heading;
        auto appendGlyph = [&](const std::string &glyph) {
            Font &font = bold ? theme.bold() : theme.regular();
            const int glyphWidth = font.measure(glyph, size);
            if (!current.runs.empty() && currentWidth + glyphWidth > noteTextRight - left) {
                lines.push_back(std::move(current));
                current = NoteLine{}; current.size = size; current.height =
                    (heading ? theme.bold() : theme.regular()).lineHeight(size) + theme.px(3);
                currentWidth = 0;
            }
            if (current.runs.empty() || current.runs.back().bold != bold)
                current.runs.push_back({{}, bold});
            current.runs.back().text += glyph;
            currentWidth += glyphWidth;
        };
        for (size_t pos = contentStart; pos < raw.size();) {
            if (raw.compare(pos, 2, "**") == 0) { bold = !bold; pos += 2; continue; }
            const unsigned char first = static_cast<unsigned char>(raw[pos]);
            const size_t count = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
            appendGlyph(raw.substr(pos, count));
            pos += count;
        }
        lines.push_back(std::move(current));
    };
    size_t start = 0;
    while (start <= notes.size()) {
        const size_t end = notes.find('\n', start);
        addLogicalLine(notes.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    int contentHeight = 0;
    for (const auto &noteLine : lines) contentHeight += noteLine.height;
    const int footerSize = theme.sizeSmall();
    const int footerLineHeight = theme.regular().lineHeight(footerSize) + theme.px(3);
    const int availableHeight = std::max(0, panel.bottom() - margin - footerLineHeight - theme.px(12) - y);
    int maxScroll = 0;
    int remainingHeight = contentHeight;
    while (remainingHeight > availableHeight && maxScroll < int(lines.size()))
        remainingHeight -= lines[size_t(maxScroll++)].height;
    notesScroll_ = std::min(notesScroll_, maxScroll);
    const int notesBottom = panel.bottom() - margin - footerLineHeight - theme.px(12);
    for (int i = notesScroll_; i < int(lines.size()); ++i) {
        const auto &noteLine = lines[size_t(i)];
        if (y + noteLine.height > notesBottom) break;
        int x = left;
        for (const auto &run : noteLine.runs) {
            Font &font = run.bold ? theme.bold() : theme.regular();
            font.draw(canvas, x, y, run.text, noteLine.size, theme.textPrimary);
            x += font.measure(run.text, noteLine.size);
        }
        y += noteLine.height;
    }
    const int arrowWidth = theme.px(22);
    const Rect upArrow{right - arrowWidth, notesTop, arrowWidth, theme.px(24)};
    const Rect downArrow{right - arrowWidth, notesBottom - theme.px(24),
                         arrowWidth, theme.px(24)};
    if (notesScroll_ > 0)
        theme.bold().drawCentered(canvas, upArrow, "↑", theme.sizeSmall(), theme.focus);
    if (notesScroll_ < maxScroll)
        theme.bold().drawCentered(canvas, downArrow, "↓", theme.sizeSmall(), theme.focus);
    theme.regular().draw(canvas, left, panel.bottom() - margin - footerLineHeight,
        formatButtonLabels("A Install   B Cancel   ↑/↓ Scroll"), footerSize, theme.accent);
}

void SettingsScreen::renderUpdateSuccess(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(205));
    const int width = std::min(area.w - theme.px(40), theme.px(900));
    const int height = std::min(area.h - theme.px(40),
                                theme.px(updateSnapshot_.pinned ? 390 : 330));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                     width, height};
    canvas.dropShadow(panel, theme.radius(), theme.px(24), theme.shadow.withAlpha(140));
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    canvas.strokeRoundedRect(panel, theme.radius(), theme.px(2), theme.accent.withAlpha(160));

    const int margin = theme.px(30);
    const int left = panel.x + margin;
    const int textWidth = panel.w - 2 * margin;
    int y = panel.y + margin;
    const std::string title = updateSnapshot_.pinRemoved
        ? "Version pin removed"
        : "v" + updateSnapshot_.targetVersion + " installed";
    theme.bold().draw(canvas, left, y, title,
                      theme.sizeHeading(), theme.textPrimary);
    y += theme.bold().lineHeight(theme.sizeHeading()) + theme.px(12);
    theme.regular().draw(canvas, left, y,
                         updateSnapshot_.pinRemoved
                             ? "The newest installed GUI is now selected for the next start."
                             : "The verified GUI library is ready. Choose what happens next.",
                         theme.sizeBody(), theme.textPrimary);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(22);

    const std::string startVersion = updateSnapshot_.selectedVersion;
    const std::string runningVersion = updateSnapshot_.runningVersion;
    const auto line = [&](const std::string &label, const std::string &detail) {
        theme.bold().draw(canvas, left, y, formatButtonLabels(label), theme.sizeBody(),
                          theme.accent);
        y += theme.bold().lineHeight(theme.sizeBody()) + theme.px(3);
        theme.regular().draw(canvas, left + theme.px(20), y,
                             theme.regular().elide(detail, theme.sizeBody(),
                                                   textWidth - theme.px(20)),
                             theme.sizeBody(), theme.textPrimary);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(13);
    };
    line("A  Restart MiSTer", "Starts v" + startVersion +
         (updateSnapshot_.pinned ? " (version pin stays active)." : "."));
    line("B  Not now", "Continue running v" + runningVersion + ".");
    if (updateSnapshot_.pinned)
        line("X  Remove pin and restart MiSTer",
             "Starts the newest installed library, v" +
                 updateSnapshot_.newestInstalledVersion + ".");
}

void SettingsScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    theme.bold().draw(canvas, area.x, area.y, "Settings", theme.sizeHeading(),
                      theme.textPrimary);

    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    const int gap = theme.gap();
    const int columnWidth = std::max(0, (body.w - 2 * gap) / 3);
    const Rect categories{body.x, body.y, columnWidth, body.h};
    const Rect options{categories.right() + gap, body.y, columnWidth, body.h};
    const Rect info{options.right() + gap, body.y, body.right() - options.right() - gap, body.h};
    canvas.fillRoundedRect(categories, theme.radius(), theme.surface.withAlpha(70));
    canvas.fillRoundedRect(options, theme.radius(), theme.surface.withAlpha(70));

    const int sectionHeight = theme.px(34);
    const int sectionTextY = body.y + (sectionHeight - theme.bold().lineHeight(theme.sizeSmall())) / 2;
    theme.bold().draw(canvas, categories.x + theme.px(10), sectionTextY, "CATEGORIES",
                      theme.sizeSmall(), theme.textMuted);
    theme.bold().draw(canvas, options.x + theme.px(10), sectionTextY,
                      categories_[size_t(categoryCursor_)].label, theme.sizeSmall(), theme.textMuted);
    theme.bold().draw(canvas, info.x + theme.px(24), sectionTextY, "WHAT THIS DOES",
                      theme.sizeSmall(), theme.textMuted);

    const Rect listArea{categories.x + theme.px(10), categories.y + sectionHeight,
                        categories.w - theme.px(20), categories.h - sectionHeight};
    const Rect optionArea{options.x + theme.px(10), options.y + sectionHeight,
                          options.w - theme.px(20), options.h - sectionHeight};
    renderCategories(canvas, listArea);
    renderOptions(canvas, optionArea);
    if (info.w > theme.px(200))
        renderSettingHelp(canvas, {info.x, info.y + sectionHeight, info.w,
                                  info.h - sectionHeight});
    if (updateSnapshot_.phase == UpdatePhase::Confirming)
        renderUpdateConfirmation(canvas, area);
    if (updateSnapshot_.phase == UpdatePhase::RestartRequired &&
        updateSnapshot_.successPromptVisible)
        renderUpdateSuccess(canvas, area);
    if (timeZoneChooserActive_) renderTimeZoneChooser(canvas, area);
    if (rebootConfirmationActive_) {
        canvas.fillRect(area, theme.shadow.withAlpha(205));
        const int width = std::min(area.w - theme.px(40), theme.px(560));
        const int height = theme.px(190);
        const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                         width, height};
        canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
        const int left = panel.x + theme.px(28);
        int y = panel.y + theme.px(28);
        theme.bold().draw(canvas, left, y, "Reboot MiSTer?", theme.sizeHeading(), theme.textPrimary);
        y += theme.bold().lineHeight(theme.sizeHeading()) + theme.px(12);
        theme.regular().draw(canvas, left, y, "The system will restart now.",
                             theme.sizeBody(), theme.textPrimary);
        theme.regular().draw(canvas, left, panel.bottom() - theme.px(42),
                             formatButtonLabels("A Reboot   B Cancel"), theme.sizeBody(),
                             theme.accent);
    }
    if (wifiStep_ != WifiStep::None) renderWifiFlow(canvas, area);
    if (changelogActive_) renderChangelog(canvas, area);
    if (discDriveSettingsActive_) renderDiscDriveSettings(canvas, area);
    if (discCoreInstallConfirmationActive_)
        renderDiscCoreInstallConfirmation(canvas, area);
    if (infoModalActive_) {
        canvas.fillRect(area, theme.shadow.withAlpha(205));
        const int width = std::min(area.w - theme.px(48), theme.px(960));
        const int height = std::min(area.h - theme.px(48), theme.px(680));
        const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                         width, height};
        canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
        const int margin = theme.px(28);
        const int titleY = panel.y + margin;
        theme.bold().draw(canvas, panel.x + margin, titleY, "System information",
                          theme.sizeHeading(), theme.textPrimary);
        const int footerHeight = theme.regular().lineHeight(theme.sizeSmall()) + theme.px(8);
        const int factsTop = titleY + theme.bold().lineHeight(theme.sizeHeading()) + theme.px(18);
        const Rect factsArea{panel.x + margin, factsTop, panel.w - margin * 2,
                             panel.bottom() - margin - footerHeight - factsTop};
        renderInfo(canvas, factsArea);
        theme.regular().draw(canvas, panel.x + margin,
                             panel.bottom() - margin - footerHeight,
                             "B Close", theme.sizeSmall(), theme.accent);
    }
}

std::string SettingsScreen::hints() const {
    if (discCoreInstallConfirmationActive_) return "A Install package   B Cancel";
    if (discDriveSettingsActive_) return "A/B Back";
    if (infoModalActive_) return "B Close";
    if (changelogActive_) {
        if (!updateSnapshot_.changelogError.empty()) return "A Try again   B Close";
        if (updateSnapshot_.changelogLoading) return "B Close";
        return "B Close   ↑/↓ Read more";
    }
    if (wifiStep_ == WifiStep::Networks) return "A Join selected network   B Cancel   ↑/↓ Move";
    if (wifiStep_ == WifiStep::SsidEntry || wifiStep_ == WifiStep::PasswordEntry ||
        wifiStep_ == WifiStep::CountryEntry)
        return "D-pad Choose key   A Type   B Back   X Delete   Y Space";
    if (wifiStep_ == WifiStep::Testing) return "B Cancel and restore the previous connection";
    if (wifiStep_ == WifiStep::TestPrompt) return "A Test connection   B Cancel";
    if (wifiStep_ == WifiStep::TestReview) return "A Save these settings   B Restore previous connection";
    if (wifiStep_ == WifiStep::TestFailed) return "A Try again   B Cancel";
    if (wifiStep_ == WifiStep::Scanning) return "Searching for Wi-Fi networks…   B Cancel";
    if (rebootConfirmationActive_) return "A Restart MiSTer   B Cancel";
    if (timeZoneChooserActive_)
        return choosingTimeZoneCity_ ? "A Set this time zone   B Back to regions"
                                     : "A Choose a region   B Cancel";
    if (updateSnapshot_.phase == UpdatePhase::Confirming)
        return "A Install update   B Cancel   ↑/↓ Read release notes";
    if (updateSnapshot_.phase == UpdatePhase::RestartRequired &&
        updateSnapshot_.successPromptVisible)
        return updateSnapshot_.pinned
            ? "A Restart MiSTer   B Later   X Use newest version + restart"
            : "A Restart MiSTer   B Later";
    if (updateSnapshot_.phase == UpdatePhase::Downloading ||
        updateSnapshot_.phase == UpdatePhase::Verifying)
        return "B Cancel download   ←/→ Change column   LB/RB Change tab";
    return "↑/↓ Move   A Choose   ←/→ Change column   B Back   LB/RB Change tab";
}
