#include "SettingsScreen.h"

#include <algorithm>
#include <cstdio>

#include "Canvas.h"
#include "GamesScreen.h"

SettingsScreen::SettingsScreen(Context &context, std::function<void()> onReload,
                               std::function<void()> onBuildDatabase,
                               std::function<void()> onFetchArtwork,
                               std::function<void()> onDownloadSystemIcons,
                               std::function<std::string()> systemIconDownloadStatus,
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
                               std::function<void()> onReboot)
    : context_(context), onReload_(std::move(onReload)),
      onBuildDatabase_(std::move(onBuildDatabase)),
      onFetchArtwork_(std::move(onFetchArtwork)),
      onDownloadSystemIcons_(std::move(onDownloadSystemIcons)),
      systemIconDownloadStatus_(std::move(systemIconDownloadStatus)),
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
      onSpaceGrotesk_(std::move(onSpaceGrotesk)), onReboot_(std::move(onReboot)) {
    buildRows();
}

void SettingsScreen::buildRows() {
    categories_.clear();

    categories_.push_back({"Library", {}});
    categories_.push_back({"Controllers", {}});
    categories_.push_back({"Interface", {}});
    categories_.push_back({"Updates", {}});
    categories_.push_back({"System", {}});
    auto &library = categories_[0].rows;

    // Library and content — building/refreshing it, then managing what it shows. Grouped
    // together and in this order because that is the order they actually depend on each
    // other: a database has to exist before its art can be fetched/prepared or systems can be
    // hidden, and "Reload" only ever re-reads what the other two just produced.
    library.push_back({"Build game database", [] { return std::string("A"); },
                     [this] { onBuildDatabase_(); }});

    library.push_back({"Prepare box art", [] { return std::string("A"); },
                     [this] { onFetchArtwork_(); }});

    library.push_back({"Download System-Icons",
                     [this] { return systemIconDownloadStatus_(); },
                     [this] { onDownloadSystemIcons_(); }});

    library.push_back({"Reload library", [] { return std::string("A"); },
                     [this] {
                         // The only path that walks the game volume, and only on request.
                         context_.notify("Scanning directories - this puts load on the drive", 6.0f);
                         context_.library.setScanningAllowed(true);
                         onReload_();
                         context_.library.setScanningAllowed(false);
                         context_.notify("Library reloaded");
                     }});

    library.push_back({"Manage systems", [] { return std::string("A"); },
                     [this] { onManageSystems_(); }});

    library.push_back({"Manage Arcade", [] { return std::string("A"); },
                     [this] { onManageArcade_(); }});

    // Hardware.
    categories_[1].rows.push_back({"Controllers", [] { return std::string("A"); },
                     [this] { onManageControllers_(); }});

    // How the interface itself looks and navigates.
    auto &interface = categories_[2].rows;
    interface.push_back({"Show Games menu item",
                     [this] { return std::string(context_.preferences.showGamesTab() ? "On" : "Off"); },
                     [this] { onToggleGamesTab_(); }});

    interface.push_back({"Show Arcade menu item",
                     [this] { return std::string(context_.preferences.showArcadeTab() ? "On" : "Off"); },
                     [this] { onToggleArcadeTab_(); }});

    interface.push_back({"Show box art",
                     [this] { return std::string(context_.preferences.showBoxArt() ? "On" : "Off"); },
                     [this] {
                         context_.preferences.setShowBoxArt(!context_.preferences.showBoxArt());
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

    interface.push_back({"Default view",
                     [this] {
                         return std::string(
                             displayNameForGameView(gameViewFromName(context_.preferences.defaultView())));
                     },
                     [this] { onCycleDefaultView_(); }});

    interface.push_back({"Time format",
                     [this] { return std::string(context_.preferences.use24HourClock()
                                                   ? "24-hour" : "12-hour AM/PM"); },
                     [this] {
                         context_.preferences.setUse24HourClock(
                             !context_.preferences.use24HourClock());
                     }});

    interface.push_back({"Time zone",
                     [this] {
                         return context_.preferences.timeZone().empty()
                             ? std::string("System default")
                             : context_.preferences.timeZone();
                     },
                     [this] {
                         timeZoneRegions_ = Preferences::timeZoneRegions();
                         if (timeZoneRegions_.empty()) {
                             context_.notify("Cannot read /usr/share/zoneinfo/posix");
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

    interface.push_back({"Space Grotesk font",
                     [this] { return spaceGroteskStatus_(); },
                     [this] { onSpaceGrotesk_(); }});

    // This project itself.
    categories_[3].rows.push_back({"Check for updates on GitHub",
                     [this] { return std::string(context_.preferences.checkForUpdates() ? "On" : "Off"); },
                     [this] { onToggleAutomaticCheck_(); }});
    buildUpdateRows();

    categories_[4].rows.push_back({"Connect to Wi-Fi…",
                     [this] { return wifi_.available() ? std::string("A")
                                                       : std::string("No adapter"); },
                     [this] { startWifiSetup(); }, wifi_.refreshInterface()});
    categories_[4].rows.push_back({"Start MiSTer Core", [] { return std::string("A"); },
                     [this] { onStartMisterCore_(); }});
    categories_[4].rows.push_back({"Reboot MiSTer", [] { return std::string("A"); },
                     [this] { rebootConfirmationActive_ = true; }});
}

void SettingsScreen::buildUpdateRows() {
    auto &rows = categories_[3].rows;
    rows.resize(1);
    rows.push_back({"Running GUI", [this] {
        return std::string("v") + updateSnapshot_.runningVersion;
    }, {}, false});
    rows.push_back({"Startup selection", [this] {
        return updateSnapshot_.pinned
            ? std::string("Pinned v") + updateSnapshot_.selectedVersion
            : std::string("Newest v") + updateSnapshot_.selectedVersion;
    }, {}, false});
    if (updateSnapshot_.pinned &&
        updateSnapshot_.newestInstalledVersion != updateSnapshot_.selectedVersion)
        rows.push_back({"Newest installed", [this] {
            return std::string("v") + updateSnapshot_.newestInstalledVersion;
        }, {}, false});
    const UpdatePhase phase = updateSnapshot_.phase;
    if (updateSnapshot_.pinned) {
        const bool enabled = phase == UpdatePhase::Idle || phase == UpdatePhase::Current ||
                             phase == UpdatePhase::Available || phase == UpdatePhase::Blocked ||
                             phase == UpdatePhase::Error || phase == UpdatePhase::RestartRequired;
        rows.push_back({"Remove pinned version", [] { return std::string("A"); },
                        [this] { onRemovePinnedVersion_(); }, enabled});
    }
    if (phase == UpdatePhase::WaitingForInternet || phase == UpdatePhase::Checking) {
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
    } else if (phase == UpdatePhase::Downloading || phase == UpdatePhase::Verifying ||
               phase == UpdatePhase::Installing) {
        rows.push_back({"Update progress", [this] {
            const auto &s = updateSnapshot_;
            if (s.phase == UpdatePhase::Verifying) return std::string("Verifying…");
            if (s.phase == UpdatePhase::Installing) return std::string("Installing…");
            if (s.bytesTotal) return std::to_string(s.bytesDone * 100 / s.bytesTotal) + "%";
            return SystemInfo::formatBytes(s.bytesDone);
        }, {}, false});
        if (phase != UpdatePhase::Installing)
            rows.push_back({"B Cancel download", {}, {}, false});
    } else if (phase == UpdatePhase::RestartRequired) {
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
        if (!updateSnapshot_.successPromptVisible)
            rows.push_back({"Restart options", [] { return std::string("A"); },
                            [this] { onShowInstallSuccess_(); }});
    } else {
        rows.push_back({"Check now", [] { return std::string("A"); },
                        [this] { onCheckUpdates_(); }});
    }
    if (phase == UpdatePhase::Available && updateSnapshot_.canInstall)
        rows.push_back({"Update now — v" + updateSnapshot_.targetVersion + " available",
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
    if (!rebuild || categories_.size() < 4) return;
    buildUpdateRows();
    if (categoryCursor_ == 3) {
        const int count = int(categories_[3].rows.size());
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
    if (indexed) out.push_back({"Console Mode index", std::to_string(indexed) + " games"});

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
    if (categories_.size() > 4 && !categories_[4].rows.empty())
        categories_[4].rows[0].enabled = wifi_.available();

    if (wifiStep_ == WifiStep::Scanning && wifi_.scanComplete()) {
        std::string error;
        wifiNetworks_ = wifi_.scanResults(error);
        if (!error.empty()) wifiMessage_ = error;
        else if (wifiNetworks_.empty()) wifiMessage_ = "No networks found. Try again or enter a hidden network.";
        else wifiMessage_ = "Choose a network";
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
                context_.notify("Wi-Fi settings saved; restart MiSTer to load them automatically", 6.0f);
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
                wifiMessage_ = "No regulatory country code was found. Enter the country where MiSTer is used.";
                wifiKeyboard_.clear();
                wifiKeyboard_.setMasked(false);
                wifiStep_ = WifiStep::CountryEntry;
                break;
            }
            std::string error;
            if (wifi_.saveTestedNetwork(error)) {
                context_.notify("Wi-Fi settings saved; restart MiSTer to load them automatically", 6.0f);
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
    case WifiStep::Networks: title = "Choose a network"; break;
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
        const std::string prompt = wifiStep_ == WifiStep::SsidEntry ? "Network name (SSID)" :
            wifiStep_ == WifiStep::CountryEntry ? "Two-letter country code (ISO 3166-1 alpha-2)" :
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
            lines = {wifiMessage_, "Save this network to MiSTer?",
                     "This writes /media/fat/linux/wpa_supplicant.conf."};
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
    if (wifiStep_ == WifiStep::Networks) hint = "A Select   B Cancel   ↑/↓ Move";
    else if (wifiStep_ == WifiStep::SsidEntry || wifiStep_ == WifiStep::PasswordEntry ||
             wifiStep_ == WifiStep::CountryEntry)
        hint = wifiStep_ == WifiStep::PasswordEntry
                   ? "D-pad Move   A Type   Start Confirm   Show/Hide key   B Back"
                   : "D-pad Move   A Type   B Back   X Delete   Y Space";
    else if (wifiStep_ == WifiStep::Testing) hint = "B Cancel and restore previous Wi-Fi";
    else if (wifiStep_ == WifiStep::TestPrompt) hint = "A Test connection   B Cancel";
    else if (wifiStep_ == WifiStep::TestReview) hint = "A Save Wi-Fi settings   B Restore previous Wi-Fi";
    else if (wifiStep_ == WifiStep::TestFailed) hint = "A Retry   B Cancel";
    else hint = "Scanning…   B Cancel";
    theme.regular().draw(canvas, left, panel.bottom() - theme.px(34), hint,
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
                context_.notify("No time zones found in this region");
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
                context_.notify("Linux time zone set to " + zone);
                timeZoneChooserActive_ = false;
            } else {
                context_.notify("Could not set Linux time zone. Check write access to /media/fat/linux");
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
        }

        const int textY = frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
        const std::string value = row.value ? row.value() : std::string();
        const int left = frame.x + theme.px(18);
        const int right = frame.right() - theme.px(14);
        const int valueWidth = value.empty() ? 0 : theme.regular().measure(value, theme.sizeBody());
        const int labelWidth = std::max(0, right - left -
                                             (value.empty() ? 0 : valueWidth + theme.px(10)));
        const std::string label = theme.regular().elide(row.label, theme.sizeBody(), labelWidth);
        theme.regular().draw(canvas, left, textY, label, theme.sizeBody(),
                             row.enabled && (active || int(i) == optionCursor_)
                                 ? theme.textPrimary : theme.textMuted);

        if (!value.empty()) {
            theme.regular().draw(canvas, right - valueWidth, textY, value,
                                 theme.sizeBody(), theme.accent);
        }
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
            theme.regular().elide("Game database format changed: rebuild it after restart.",
                                  theme.sizeBody(), right - left),
            theme.sizeBody(), theme.accent);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(8);
    }
    y += theme.px(10);
    theme.bold().draw(canvas, left, y, "RELEASE NOTES", theme.sizeSmall(), theme.textMuted);
    y += theme.bold().lineHeight(theme.sizeSmall()) + theme.px(10);
    const int lineHeight = theme.regular().lineHeight(theme.sizeBody()) + theme.px(3);
    std::vector<std::string> lines;
    std::string line;
    const std::string &notes = updateSnapshot_.releaseNotes;
    for (size_t pos = 0; pos < notes.size();) {
        const unsigned char first = (unsigned char)notes[pos];
        const size_t count = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
        const std::string glyph = notes.substr(pos, count);
        pos += count;
        if (glyph == "\n") { lines.push_back(line); line.clear(); continue; }
        std::string next = line + glyph;
        if (!line.empty() && theme.regular().measure(next, theme.sizeBody()) > right - left) {
            lines.push_back(line);
            line = glyph;
        } else line = next;
    }
    lines.push_back(line);
    const int maxLines = std::max(0, (panel.bottom() - margin - theme.px(48) - y) / lineHeight);
    notesScroll_ = std::min(notesScroll_, std::max(0, int(lines.size()) - maxLines));
    for (int i = notesScroll_; i < int(lines.size()) && i < notesScroll_ + maxLines; ++i) {
        theme.regular().draw(canvas, left, y, lines[size_t(i)], theme.sizeBody(), theme.textPrimary);
        y += lineHeight;
    }
    theme.regular().draw(canvas, left, panel.bottom() - margin - lineHeight,
        "A Install   B Cancel   ↑/↓ Scroll", theme.sizeBody(), theme.accent);
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
        theme.bold().draw(canvas, left, y, label, theme.sizeBody(), theme.accent);
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
    theme.bold().draw(canvas, info.x + theme.px(24), sectionTextY, "SYSTEM",
                      theme.sizeSmall(), theme.textMuted);

    const Rect listArea{categories.x + theme.px(10), categories.y + sectionHeight,
                        categories.w - theme.px(20), categories.h - sectionHeight};
    const Rect optionArea{options.x + theme.px(10), options.y + sectionHeight,
                          options.w - theme.px(20), options.h - sectionHeight};
    renderCategories(canvas, listArea);
    renderOptions(canvas, optionArea);
    if (info.w > theme.px(200)) {
        renderInfo(canvas, {info.x, info.y + sectionHeight, info.w, info.h - sectionHeight});
    }
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
                             "A Reboot   B Cancel", theme.sizeBody(), theme.accent);
    }
    if (wifiStep_ != WifiStep::None) renderWifiFlow(canvas, area);
}

std::string SettingsScreen::hints() const {
    if (wifiStep_ == WifiStep::Networks) return "A Select   B Cancel   ↑/↓ Move";
    if (wifiStep_ == WifiStep::SsidEntry || wifiStep_ == WifiStep::PasswordEntry ||
        wifiStep_ == WifiStep::CountryEntry)
        return "D-pad Move   A Type   B Back   X Delete   Y Space";
    if (wifiStep_ == WifiStep::Testing) return "B Cancel and restore previous Wi-Fi";
    if (wifiStep_ == WifiStep::TestPrompt) return "A Test connection   B Cancel";
    if (wifiStep_ == WifiStep::TestReview) return "A Save Wi-Fi settings   B Restore previous Wi-Fi";
    if (wifiStep_ == WifiStep::TestFailed) return "A Retry   B Cancel";
    if (wifiStep_ == WifiStep::Scanning) return "Scanning…   B Cancel";
    if (rebootConfirmationActive_) return "A Reboot   B Cancel";
    if (timeZoneChooserActive_)
        return choosingTimeZoneCity_ ? "A Set timezone   B Regions" : "A Choose   B Cancel";
    if (updateSnapshot_.phase == UpdatePhase::Confirming)
        return "A Install   B Cancel   ↑/↓ Scroll";
    if (updateSnapshot_.phase == UpdatePhase::RestartRequired &&
        updateSnapshot_.successPromptVisible)
        return updateSnapshot_.pinned
            ? "A Restart   B Not now   X Remove pin + restart"
            : "A Restart   B Not now";
    if (updateSnapshot_.phase == UpdatePhase::Downloading ||
        updateSnapshot_.phase == UpdatePhase::Verifying)
        return "B Cancel download   ←/→ Columns   LB/RB Tabs";
    return "A Select   ←/→ Columns   B Back   LB/RB Tabs";
}
