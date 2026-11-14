#include "App.h"
#include "ArtworkIndex.h"
#include "ButtonLabels.h"
#include "DebugLog.h"
#include "ErrorModal.h"
#include "Splash.h"
#include "Version.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/reboot.h>
#include <unistd.h>

namespace {

constexpr int kTargetFrameMs = 33;   // ~30 fps; the CPU renders every pixel in software
constexpr char kDpadHintToken[] = "{DPAD}";

Color contrastingText(Color color) {
    const int luminance = (color.r * 299 + color.g * 587 + color.b * 114) / 1000;
    return luminance >= 150 ? Color::rgb(0x111318) : Color::rgb(0xFFFFFF);
}

int hintTextWidth(Font &font, const std::string &text, int size, int iconSize, int gap) {
    int width = 0;
    size_t start = 0;
    for (;;) {
        const size_t marker = text.find(kDpadHintToken, start);
        const size_t end = marker == std::string::npos ? text.size() : marker;
        width += font.measure(text.substr(start, end - start), size);
        if (marker == std::string::npos) break;
        width += iconSize + gap;
        start = marker + sizeof(kDpadHintToken) - 1;
    }
    return width;
}

void drawHintText(Canvas &canvas, Theme &theme, Font &font, int x, int y, int size,
                  Color color, const std::string &text) {
    const int iconSize = theme.hintIconSize();
    const int gap = theme.px(5);
    const int lineHeight = font.lineHeight(size);
    size_t start = 0;
    for (;;) {
        const size_t marker = text.find(kDpadHintToken, start);
        const size_t end = marker == std::string::npos ? text.size() : marker;
        const std::string part = text.substr(start, end - start);
        font.draw(canvas, x, y, part, size, color);
        x += font.measure(part, size);
        if (marker == std::string::npos) break;

        const int iconX = x;
        const int iconY = y + (lineHeight - iconSize) / 2;
        const int arm = std::max(theme.px(5), iconSize / 3);
        const int center = (iconSize - arm) / 2;
        const int radius = std::max(1, theme.px(1));
        canvas.fillRoundedRect({iconX + center, iconY, arm, center - radius}, radius, color);
        canvas.fillRoundedRect({iconX, iconY + center, center - radius, arm}, radius, color);
        canvas.fillRoundedRect({iconX + center + arm, iconY + center,
                                center - radius, arm}, radius, color);
        canvas.fillRoundedRect({iconX + center, iconY + center + arm,
                                arm, center - radius}, radius, color);
        canvas.fillRoundedRect({iconX + center, iconY + center, arm, arm}, radius, color);
        x += iconSize + gap;
        start = marker + sizeof(kDpadHintToken) - 1;
    }
}

// Diagnostic for docs/ISSUES.md ("Home is blank after a cold start"): the MiSTer_fb kernel
// module counts every mode it applies, and Main_MiSTer re-applies it at moments we do not
// control. Reading the counter is a sysfs read in kernel memory — no storage access.
long readFramebufferResCount() {
    std::FILE *file = std::fopen("/sys/module/MiSTer_fb/parameters/res_count", "r");
    if (!file) return -1;
    long value = -1;
    if (std::fscanf(file, "%ld", &value) != 1) value = -1;
    std::fclose(file);
    return value;
}

// Seconds since boot, the same clock as the [ ... ] stamps in dmesg.
double uptimeSeconds() {
    std::FILE *file = std::fopen("/proc/uptime", "r");
    if (!file) return -1.0;
    double value = -1.0;
    if (std::fscanf(file, "%lf", &value) != 1) value = -1.0;
    std::fclose(file);
    return value;
}

// Every box-art directory of every system, once each, in library order.
std::vector<std::string> artworkDirectories(const std::vector<GameSystem> &systems) {
    std::vector<std::string> directories;
    for (const GameSystem &system : systems)
        for (const std::string &directory : system.mediaDirs)
            if (std::find(directories.begin(), directories.end(), directory) == directories.end())
                directories.push_back(directory);
    return directories;
}

} // namespace

App::App() : discArtwork_(theGamesDB_), gameScreenshots_(theGamesDB_) {}

bool App::initialize(const Options &options) {
    options_ = options;
    screensaverSeed_ = unsigned(nowMs()) ^ unsigned(getpid());
    discDrive_.start();

    DebugLog::info(std::string("starting mister-gui v") + kAppVersion);

    if (!framebuffer_.open()) {
        DebugLog::error("could not open the framebuffer, giving up");
        return false;
    }
    DebugLog::info("framebuffer: " + framebuffer_.describe());

    canvas_ = std::make_unique<Canvas>(framebuffer_.width(), framebuffer_.height());
    background_ = std::make_unique<Canvas>(framebuffer_.width(), framebuffer_.height());
    theme_ = std::make_unique<Theme>(framebuffer_.width(), framebuffer_.height());

    // Grab pads before the splash can show anything they type into the MiSTer terminal or
    // OSD. This is released automatically on exit, and explicitly on the core handoff below.
    if (options_.readInput) {
        if (options_.dumpPath.empty()) console_.acquire();
        input_.setExclusive(options_.exclusive);
        input_.rescan();
    }

    // Listing icon filenames is cheap; their PNG data is decoded only after the initial splash
    // wait, and is cached at the Systems tile size before the screen can be opened.
    icons_.load();
    tabIcons_.refresh();
    preferences_.load();
    theme_->setFontSizeScale(preferences_.fontSizeScale());
    theme_->setHintSizeScale(preferences_.hintSizeScale());
    theme_->setColorTheme(preferences_.colorTheme());
    theme_->setDarkMode(preferences_.darkMode());
    theme_->setHighContrastColor(preferences_.highContrastColor());
    theme_->setHighContrast(preferences_.highContrast());
    setFaceButtonStyle(preferences_.faceButtonStyle());
    if (options_.splashMs > 0) {
        Splash::show(framebuffer_, *theme_, options_.splashMs, icons_, images_,
                     [this] {
                         library_.load();
                         if (preferences_.organizeGameVersions())
                             library_.prepareGameCache();
                         // Load the saved box-art index, or scan the folders while the splash
                         // is still up when no usable index exists.
                         const auto directories = artworkDirectories(library_.systems());
                         if (!ArtworkIndex::instance().loadPersistent(directories))
                             ArtworkIndex::instance().build(directories, true);
                     });
        if (preferences_.startSoundEnabled()) UiSounds::play();
    } else {
        library_.load();
        if (preferences_.organizeGameVersions()) library_.prepareGameCache();
        const auto directories = artworkDirectories(library_.systems());
        if (!ArtworkIndex::instance().loadPersistent(directories))
            ArtworkIndex::instance().build(directories, true);
    }

    systemIconDownload_ = std::make_unique<SystemIconDownload>(icons_);
    tabIconDownload_ = std::make_unique<TabIconDownload>(tabIcons_);

    renderBackground(*background_);

    favorites_.load();
    history_.load();
    hiddenSystems_.load();
    theme_->setSpaceGroteskEnabled(preferences_.useSpaceGrotesk());
    updateService_.initializeLocalState();
    updateSnapshot_ = updateService_.snapshot();

    context_ = std::make_unique<Context>(*theme_, library_, favorites_, launcher_, images_,
                                        icons_, history_, hiddenSystems_, preferences_, discDrive_,
                                        discCores_, discArtwork_, theGamesDB_, discCoreInstaller_);
    context_->background = background_.get();
    context_->onChooseGameVersion = [this](const GameSystem &system, const Game &game) {
        openGameVersionChooser(system, game);
    };
    context_->onGameVersionSettingChanged = [this] {
        homeScreen_->refresh();
        homeScreen_->invalidateSearchIndex();
        favoritesScreen_->showFavorites();
        allGamesScreen_->showAllGames();
        if (gamesScreen_->title() != "") gamesScreen_->reload();
        if (const GameSystem *arcade = library_.arcadeSystem()) {
            arcadeLibraryScreen_->showSystem(*arcade);
            arcadeScreenDirty_ = false;
        }
        needsFullRedraw_ = true;
    };
    context_->onThemeChanged = [this] {
        renderBackground(*background_);
        needsFullRedraw_ = true;
    };

    gamesScreen_ = std::make_unique<GamesScreen>(*context_);
    arcadeLibraryScreen_ = std::make_unique<GamesScreen>(*context_);
    allGamesScreen_ = std::make_unique<GamesScreen>(*context_);
    favoritesScreen_ = std::make_unique<GamesScreen>(*context_);

    if (!options_.tabExplicit) {
        const std::string &start = preferences_.startTab();
        if (start == "favorites") options_.tab = Tab::Favorites;
        else if (start == "systems") options_.tab = Tab::Systems;
        else if (start == "games" && preferences_.showGamesTab()) options_.tab = Tab::Games;
        else if (start == "arcade" && arcadeTabShown()) options_.tab = Tab::Arcade;
    }

    const auto initialView = [this](const std::string &individualView) {
        if (options_.viewExplicit) return options_.view;
        return gameViewFromName(preferences_.defaultView() == "individual"
                                    ? individualView : preferences_.defaultView());
    };
    gamesScreen_->setView(initialView(options_.tab == Tab::Games
                                         ? preferences_.gamesView()
                                         : preferences_.consolesView()));
    arcadeLibraryScreen_->setView(options_.viewExplicit
        ? options_.view : gameViewFromName(preferences_.defaultView()));
    allGamesScreen_->setView(initialView(preferences_.gamesView()));
    favoritesScreen_->setView(initialView(preferences_.favoritesView()));
    favoritesScreen_->showFavorites();

    homeScreen_ = std::make_unique<HomeScreen>(*context_);
    homeScreen_->setSearchView(initialView(preferences_.homeView()));
    const auto rememberView = [this](void (Preferences::*save)(const std::string &), GameView view) {
        if (preferences_.defaultView() == "individual")
            (preferences_.*save)(nameForGameView(view));
    };
    gamesScreen_->setViewChangedCallback([this, rememberView](GameView view) {
        rememberView(tab_ == Tab::Games ? &Preferences::setGamesView
                                        : &Preferences::setConsolesView, view);
    });
    favoritesScreen_->setViewChangedCallback([rememberView](GameView view) {
        rememberView(&Preferences::setFavoritesView, view);
    });
    allGamesScreen_->setViewChangedCallback([rememberView](GameView view) {
        rememberView(&Preferences::setGamesView, view);
    });
    homeScreen_->setSearchViewChangedCallback([rememberView](GameView view) {
        rememberView(&Preferences::setHomeView, view);
    });

    systemsScreen_ = std::make_unique<SystemsScreen>(*context_, [this](const GameSystem &system) {
        gamesScreen_->showSystem(system);
        detailActive_ = true;
        needsFullRedraw_ = true;
    });

    settingsScreen_ = std::make_unique<SettingsScreen>(
        *context_, [this] { reloadLibrary(); }, [this] { openScan(false); },
        [this] { openArtwork(); },
        [this] { startAssetDownload(); }, [this] { return assetDownloadStatus(); },
        [this] { openVisibility(); }, [this] { openControllers(); },
        [this] { openArcadeGames(); },
        [this] { toggleGamesTab(); }, [this] { toggleArcadeTab(); }, [this] { cycleDefaultView(); },
        [this] { startMisterCore(); },
        [this] {
            const bool enabled = !preferences_.checkForUpdates();
            preferences_.setCheckForUpdates(enabled);
            if (!enabled) updateService_.cancelAutomaticCheck();
        },
        [this] { autoCheckDecisionMade_ = true; updateService_.startCheck(false); },
        [this] { updateService_.beginConfirmation(); },
        [this] { updateService_.startInstall(); },
        [this] { updateService_.cancelBeforeCommit(); },
        [this] { updateService_.dismissInstallSuccess(); },
        [this] { updateService_.showInstallSuccess(); },
        [this] { removePinnedVersion(); },
        [this](bool removePin) { restartMisterAfterUpdate(removePin); },
        [this] { return spaceGroteskStatus(); }, [this] { activateSpaceGrotesk(); },
        [this] { return updateService_.startChangelog(); },
        [this] { rebootMister(); }, [this] {
            if (backupTask_.start()) {
                backupStateReported_ = BackupTask::State::Idle;
                context_->notify("Creating settings backup…", 30.0f);
            }
        });
    settingsScreen_->setFramebufferInfo(framebuffer_.describe());
    settingsScreen_->setUpdateSnapshot(updateSnapshot_);

    discScreen_ = std::make_unique<DiscScreen>(*context_, discCores_, [this](const std::string &mgl) {
        if (!launcher_.launchPhysicalDisc(mgl)) {
            context_->showError("Could not start Physical Disc", launcher_.lastError());
            return;
        }
        context_->standDown = true;
    });
    discCoreStatus_ = discCores_.snapshot();

    scanScreen_ = std::make_unique<ScanScreen>(
        *context_, scan_, scraper_, [this] { reloadLibrary(); }, [this] { closeScan(); },
        [this] { preferences_.setOnboardingComplete(true); });

    visibilityScreen_ =
        std::make_unique<SystemVisibilityScreen>(*context_, [this] { closeVisibility(); });

    controllersScreen_ = std::make_unique<ControllersScreen>(
        *context_, input_, [this] { closeControllers(); });

    arcadeGamesScreen_ =
        std::make_unique<ArcadeGamesScreen>(*context_, [this] { closeArcadeGames(); });

    arcadeSettingsScreen_ = std::make_unique<ArcadeSettingsScreen>(
        *context_, [this] { openArcadeGames(); }, [this] { closeArcadeSettings(); });

    const GameSystem *wanted = options_.system.empty() ? nullptr
                                                       : library_.findSystem(options_.system);
    if (!wanted) wanted = systemsScreen_->current();
    if (wanted) gamesScreen_->showSystem(*wanted);

    selectTab(options_.tab);
    tabSoundsReady_ = true;

    // --system together with the games tab means "show that system's list".
    if (options_.tab == Tab::Games && !options_.system.empty()) detailActive_ = true;
    else if (options_.tab == Tab::Games) allGamesScreen_->showAllGames();

    launcher_.setDryRun(options_.dryRun);
    if (options_.launchNow) gamesScreen_->launchCurrent();

    // Offer first-use guidance once. Skipping it is remembered, so an empty library does not
    // trap the user in the wizard on every later boot; the full scan remains in Settings.
    if (options_.wizard && !preferences_.onboardingComplete()) {
        if (library_.usingDatabase() && !preferences_.onboardingStarted()) {
            // A library already present on an older installation is evidence that this is
            // not its first setup. Do not interrupt existing users with a new welcome flow.
            preferences_.setOnboardingComplete(true);
        } else {
            preferences_.setOnboardingStarted(true);
            openScan(true);
        }
    }

    {
        std::string token;
        for (char c : options_.script + ",") {
            if (c == ',') {
                if (!token.empty()) script_.push_back(token);
                token.clear();
            } else if (c != ' ') {
                token.push_back(c);
            }
        }
    }

    return true;
}

void App::reloadLibrary() {
    library_.clearPersistentGameCache();
    library_.load();
    // The folders may have changed: rebuild in the background and save the finished index.
    ArtworkIndex::instance().buildAsync(artworkDirectories(library_.systems()));
    if (preferences_.organizeGameVersions()) library_.prepareGameCache();
    favorites_.load();
    history_.load();
    systemsScreen_->refresh();
    homeScreen_->refresh();
    favoritesScreen_->showFavorites();
    allGamesScreen_->showAllGames();
    homeScreen_->invalidateSearchIndex();
    if (!gamesScreen_->empty()) gamesScreen_->reload();
    arcadeScreenDirty_ = true;
    if (tab_ == Tab::Arcade) {
        if (const GameSystem *arcade = library_.arcadeSystem()) {
            arcadeLibraryScreen_->showSystem(*arcade);
            arcadeScreenDirty_ = false;
        }
    }
    if (tab_ == Tab::Arcade && !arcadeTabShown()) selectTab(Tab::Home);
}

void App::openScan(bool firstRun) {
    scanScreen_->reset(firstRun);
    scanActive_ = true;
    needsFullRedraw_ = true;
}

void App::openArtwork() {
    scanScreen_->resetForArtwork();
    scanActive_ = true;
    needsFullRedraw_ = true;
}

void App::startAssetDownload() {
    if (assetsDownloadActive_) {
        context_->notify("An icon download is already running.");
        return;
    }

    const bool systemStarted = systemIconDownload_ && systemIconDownload_->start();
    const bool tabsStarted = tabIconDownload_ && tabIconDownload_->start();
    assetsDownloadActive_ = (systemIconDownload_ && systemIconDownload_->running()) ||
                            (tabIconDownload_ && tabIconDownload_->running());
    if (assetsDownloadActive_) {
        context_->notify("Downloading missing console and navigation icons…", 4.0f);
    } else if (!systemStarted || !tabsStarted) {
        const std::string error = !systemStarted && systemIconDownload_
            ? systemIconDownload_->error() : tabIconDownload_->error();
        context_->notify(error, 8.0f);
    } else {
        context_->notify("All console and navigation icons are ready.");
    }
}

std::string App::assetDownloadStatus() const {
    if (assetsDownloadActive_) return "Downloading icons…";
    if ((systemIconDownload_ && systemIconDownload_->state() == SystemIconDownload::State::Failed) ||
        (tabIconDownload_ && tabIconDownload_->state() == TabIconDownload::State::Failed))
        return "Try download again";
    const size_t missing = icons_.missingBitmaps().size() + tabIcons_.missingFiles().size();
    return missing ? std::to_string(missing) + " icons missing" : "All icons ready";
}

std::string App::spaceGroteskStatus() const {
    if (spaceGroteskDownload_.running()) return spaceGroteskDownload_.status();
    if (!Theme::spaceGroteskAvailable())
        return spaceGroteskDownload_.state() == SpaceGroteskDownload::State::Failed
                   ? std::string("Try font download again") : std::string("Download font");
    return preferences_.useSpaceGrotesk() ? "On" : "Off";
}

void App::activateSpaceGrotesk() {
    if (spaceGroteskDownload_.running()) {
        context_->notify(spaceGroteskDownload_.status());
        return;
    }
    if (Theme::spaceGroteskAvailable()) {
        const bool enabled = !preferences_.useSpaceGrotesk();
        preferences_.setUseSpaceGrotesk(enabled);
        theme_->setSpaceGroteskEnabled(enabled);
        needsFullRedraw_ = true;
        context_->notify(enabled ? "Space Grotesk font enabled" : "Space Grotesk font disabled");
        return;
    }
    if (!spaceGroteskDownload_.start()) {
        context_->notify(spaceGroteskDownload_.error(), 6.0f);
        return;
    }
    if (spaceGroteskDownload_.state() == SpaceGroteskDownload::State::Done) {
        preferences_.setUseSpaceGrotesk(true);
        theme_->setSpaceGroteskEnabled(true);
        needsFullRedraw_ = true;
    } else {
        context_->notify("Downloading the Space Grotesk font…", 4.0f);
    }
}

void App::closeScan() {
    // Artwork that arrived while the wizard ran is on disk but not in the image cache yet.
    if (scraper_.fetched() || scraper_.prepared()) reloadLibrary();

    scanActive_ = false;
    needsFullRedraw_ = true;
}

void App::openVisibility() {
    visibilityScreen_->refresh();
    visibilityActive_ = true;
    needsFullRedraw_ = true;
}

void App::closeVisibility() {
    // Hidden state may have just changed; the Systems tab needs to drop or restore rows now,
    // not the next time something else happens to rebuild it.
    systemsScreen_->refresh();
    visibilityActive_ = false;
    needsFullRedraw_ = true;
}

void App::openControllers() {
    controllersScreen_->refresh();
    controllersActive_ = true;
    needsFullRedraw_ = true;
}

void App::closeControllers() {
    controllersActive_ = false;
    needsFullRedraw_ = true;
}

void App::openArcadeSettings() {
    arcadeSettingsActive_ = true;
    needsFullRedraw_ = true;
}

void App::closeArcadeSettings() {
    arcadeSettingsActive_ = false;
    needsFullRedraw_ = true;
}

void App::openArcadeGames() {
    arcadeGamesScreen_->refresh();
    arcadeGamesActive_ = true;
    needsFullRedraw_ = true;
}

void App::closeArcadeGames() {
    arcadeGamesActive_ = false;
    needsFullRedraw_ = true;
}

void App::toggleGamesTab() {
    preferences_.setShowGamesTab(!preferences_.showGamesTab());

    // Turning it off while it is the current tab would otherwise leave the top bar
    // highlighting a tab that no longer exists.
    if (!preferences_.showGamesTab() && tab_ == Tab::Games) selectTab(Tab::Home);
    needsFullRedraw_ = true;
}

bool App::arcadeTabShown() const {
    return preferences_.showArcadeTab() && library_.arcadeSystem() != nullptr;
}

bool App::tabNavigationAvailable() const {
    // These screens consume input before the global tab actions in dispatch().
    return !context_->errorActive && !gameActionsActive_ && !scanActive_ && !visibilityActive_ &&
           !controllersActive_ && !arcadeGamesActive_ && !arcadeSettingsActive_;
}

std::vector<Tab> App::visibleTabs() const {
    const auto drive = discDrive_.snapshot();
    const bool hasDrive = drive.state != DiscDriveService::MediaState::NoDrive;
    const auto &cores = discCoreStatus_;
    return TopBar::visibleTabs(preferences_.showGamesTab(), arcadeTabShown(),
                               hasDrive && cores.alternateMainInstalled &&
                               cores.scopedMainConfigured && cores.hasAnyCompatibleMgl());
}

void App::toggleArcadeTab() {
    preferences_.setShowArcadeTab(!preferences_.showArcadeTab());
    if (!arcadeTabShown() && tab_ == Tab::Arcade) selectTab(Tab::Home);
    needsFullRedraw_ = true;
}

void App::cycleDefaultView() {
    const std::string &current = preferences_.defaultView();
    const std::string next = current == "grid" ? "small" :
                             current == "small" ? "list" :
                             current == "list" ? "individual" : "grid";
    preferences_.setDefaultView(next);

    const auto viewFor = [this, &next](const std::string &individualView) {
        return gameViewFromName(next == "individual" ? individualView : next);
    };
    gamesScreen_->setView(viewFor(preferences_.consolesView()));
    favoritesScreen_->setView(viewFor(preferences_.favoritesView()));
    allGamesScreen_->setView(viewFor(preferences_.gamesView()));
    homeScreen_->setSearchView(viewFor(preferences_.homeView()));
    arcadeLibraryScreen_->setView(next == "individual"
        ? gameViewFromName(preferences_.defaultView()) : gameViewFromName(next));
    context_->notify("Game view: " + (next == "individual"
        ? std::string("Individual") : std::string(displayNameForGameView(gameViewFromName(next)))));
    needsFullRedraw_ = true;
}

void App::startMisterCore() {
    if (updateService_.commitRunning()) return;
    // The patched Main_MiSTer relaunches this GUI a few seconds after it exits; the marker
    // tells it to leave the stock menu on screen instead. See App.h for the path.
    FILE *marker = std::fopen(kSuspendMarkerPath, "w");
    if (marker) std::fclose(marker);
    stop();
}

void App::rebootMister() {
    if (updateService_.commitRunning()) return;
    ::sync();
    if (::reboot(RB_AUTOBOOT) != 0)
        context_->showError("Cannot restart MiSTer", std::strerror(errno));
}

void App::restartMisterAfterUpdate(bool removePin) {
    std::string error;
    if (!updateService_.prepareRestart(removePin, error)) {
        context_->showError("Cannot restart MiSTer", error);
        return;
    }
    ::sync();
    if (::reboot(RB_AUTOBOOT) != 0)
        context_->showError("Cannot restart MiSTer", std::strerror(errno));
}

void App::removePinnedVersion() {
    std::string error;
    if (!updateService_.removePinnedVersion(error))
        context_->showError("Cannot remove version pin", error);
}

Screen *App::activeScreen() {
    if (scanActive_) return scanScreen_.get();
    if (visibilityActive_) return visibilityScreen_.get();
    if (controllersActive_) return controllersScreen_.get();
    if (arcadeGamesActive_) return arcadeGamesScreen_.get();
    if (arcadeSettingsActive_) return arcadeSettingsScreen_.get();
    if (detailActive_) return gamesScreen_.get();

    switch (tab_) {
    case Tab::Disc:     return discScreen_.get();
    case Tab::Home:      return homeScreen_.get();
    case Tab::Favorites: return favoritesScreen_.get();
    case Tab::Systems:   return systemsScreen_.get();
    case Tab::Arcade:    return arcadeLibraryScreen_.get();
    case Tab::Games:     return allGamesScreen_.get();
    case Tab::Settings:  return settingsScreen_.get();
    }
    return systemsScreen_.get();
}

void App::selectTab(Tab tab) {
    const bool changed = tab_ != tab;
    if (changed && tab_ == Tab::Disc && tab != Tab::Disc)
        discScreen_->cancelPendingLaunch();
    detailActive_ = false;
    needsFullRedraw_ = true;
    if (tab == Tab::Home) homeScreen_->refresh();
    if (tab == Tab::Favorites) favoritesScreen_->showFavorites();
    if (tab == Tab::Arcade) {
        if (const GameSystem *arcade = library_.arcadeSystem()) {
            if (arcadeScreenDirty_ || arcadeLibraryScreen_->title() != arcade->name) {
                arcadeLibraryScreen_->showSystem(*arcade);
                arcadeScreenDirty_ = false;
            }
        }
    }
    // The whole library is gathered when the tab is first opened and then kept; a library
    // reload is what throws it away again. Building the list itself is no longer the
    // expensive part — see GamesScreen::reload() — so there is no longer a mid-build state
    // to avoid restarting here.
    if (tab == Tab::Games && allGamesScreen_->empty()) allGamesScreen_->showAllGames();
    tab_ = tab;
    if (changed && tabSoundsReady_ && preferences_.menuSoundsEnabled())
        UiSounds::playTabChange();
}

void App::runScript() {
    if (scriptWait_ > 0) { --scriptWait_; return; }

    while (scriptPosition_ < script_.size()) {
        const std::string &token = script_[scriptPosition_++];

        if (token.compare(0, 5, "wait:") == 0) {
            scriptWait_ = std::atoi(token.c_str() + 5);
            return;
        }

        static const struct { const char *name; Action action; } kNames[] = {
            {"up", Action::Up},           {"down", Action::Down},
            {"left", Action::Left},       {"right", Action::Right},
            {"confirm", Action::Confirm}, {"back", Action::Back},
            {"select", Action::Select},
            {"fav", Action::ToggleFavorite}, {"view", Action::CycleView},
            {"prev", Action::TabPrev},    {"next", Action::TabNext},
            {"jumpprev", Action::JumpPrev}, {"jumpnext", Action::JumpNext},
        };
        for (const auto &entry : kNames)
            if (token == entry.name) { dispatch(entry.action); break; }

        // One press per frame, so each has its effect applied before the next arrives.
        return;
    }
}

void App::openGameActions() {
    const GameSystem *selectedSystem = nullptr;
    Game selectedGame;
    Screen *screen = activeScreen();
    if (!screen || !screen->selectedGame(selectedSystem, selectedGame)) return;

    // A group view may carry a filtered database key. Resolve by path so the scraper always
    // receives the canonical system key used by the library database.
    const GameSystem *canonical = library_.systemForPath(selectedGame.path);
    if (!canonical) canonical = selectedSystem;
    gameActionSystem_ = canonical;
    gameActionGame_ = std::move(selectedGame);
    // The action panel uses the same fixed-size detail artwork as the List view.
    // Keep the organized ROM choices while resolving the artwork for this larger slot.
    {
        const auto versions = gameActionGame_.versions;
        gameActionGame_ = Library::makeGame(*canonical, gameActionGame_.path,
                                             ArtworkVariant::Detail);
        gameActionGame_.versions = versions;
    }
    std::stable_sort(gameActionGame_.versions.begin(), gameActionGame_.versions.end(),
        [](const Game::Variant &a, const Game::Variant &b) { return a.label < b.label; });
    gameActionSelectedVersion_ = 0;
    for (size_t i = 0; i < gameActionGame_.versions.size(); ++i)
        if (gameActionGame_.versions[i].path == gameActionGame_.path)
            gameActionSelectedVersion_ = int(i);
    gameActionCursor_ = 0;
    gameVersionScroll_ = 0;
    gameActionScreenshotCursor_ = 0;
    gameActionScreenshotReady_ = false;
    gameActionScreenshotOverlay_ = false;
    gameActionScreenshotPaths_.clear();
    gameScreenshots_.request(canonical->dbKey.empty() ? canonical->name : canonical->dbKey,
                             gameActionGame_.name, gameActionGame_.backgroundCandidates);
    gameActionsActive_ = true;
    input_.cancelFavoriteHold();
    favoriteHoldScreen_ = nullptr;
    favoriteHoldPath_.clear();
    needsFullRedraw_ = true;
}

void App::openGameVersionChooser(const GameSystem &system, const Game &game) {
    gameActionSystem_ = &system;
    gameActionGame_ = game;
    {
        const auto versions = gameActionGame_.versions;
        gameActionGame_ = Library::makeGame(system, gameActionGame_.path,
                                             ArtworkVariant::Detail);
        gameActionGame_.versions = versions;
    }
    gameActionCursor_ = 0;
    gameVersionScroll_ = 0;
    gameActionScreenshotCursor_ = 0;
    gameActionScreenshotReady_ = false;
    gameActionScreenshotOverlay_ = false;
    gameActionScreenshotPaths_.clear();
    gameScreenshots_.request(system.dbKey.empty() ? system.name : system.dbKey,
                             gameActionGame_.name, gameActionGame_.backgroundCandidates);
    gameActionSelectedVersion_ = 0;
    std::stable_sort(gameActionGame_.versions.begin(), gameActionGame_.versions.end(),
        [](const Game::Variant &a, const Game::Variant &b) { return a.label < b.label; });
    for (size_t i = 0; i < gameActionGame_.versions.size(); ++i)
        if (gameActionGame_.versions[i].path == gameActionGame_.path)
            gameActionSelectedVersion_ = int(i);
    gameActionsActive_ = true;
    needsFullRedraw_ = true;
}

void App::renderGameActions() {
    if (!gameActionsActive_) return;
    Canvas &canvas = *canvas_;
    Theme &theme = *theme_;
    const Rect bounds = canvas.bounds();
    canvas.fillRect(bounds, theme.shadow.withAlpha(175));

    const int width = std::min(theme.px(1776), bounds.w - theme.px(64));
    const int height = std::min(theme.px(800), bounds.h - theme.px(120));
    const Rect panel{bounds.x + (bounds.w - width) / 2, bounds.y + (bounds.h - height) / 2,
                     width, height};
    canvas.dropShadow(panel, theme.radius(), theme.px(24), theme.shadow.withAlpha(160));
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);

    const int pad = theme.px(28);
    const int left = panel.x + pad;
    int y = panel.y + pad;
    theme.bold().draw(canvas, left, y, "Game options", theme.sizeHeading(), theme.textPrimary);
    y += theme.px(48);

    const int artW = theme.px(689);
    const int gap = theme.px(36);
    const Rect artBox{left, y, artW, theme.px(624)};
    canvas.fillRoundedRect(artBox, theme.px(8), theme.surface);
    ImagePtr boxart = context_->images.requestAsyncCandidates(gameActionGame_.boxartCandidates,
                                                               artBox.w, artBox.h, true);
    if (boxart) {
        const int x = artBox.x + (artBox.w - boxart->width()) / 2;
        const int imageY = artBox.y + (artBox.h - boxart->height()) / 2;
        canvas.drawImage(*boxart, {x, imageY, boxart->width(), boxart->height()}, 255,
                         theme.px(8));
    } else {
        const bool pending = context_->images.isPendingCandidates(
            gameActionGame_.boxartCandidates, artBox.w, artBox.h);
        theme.regular().drawCentered(canvas, artBox,
            pending ? "Loading box art…" : "No box art available",
            theme.sizeBody(), theme.textMuted);
    }

    const int rightX = artBox.right() + gap;
    const int rightW = panel.right() - pad - rightX;
    const int titleY = y + theme.px(4);
    theme.bold().draw(canvas, rightX, titleY,
        theme.regular().elide(gameActionGame_.name, theme.sizeTitle(), rightW),
        theme.sizeTitle(), theme.textPrimary);
    int contentY = titleY + theme.px(66);

    theme.bold().draw(canvas, rightX, contentY, "AVAILABLE ROMS", theme.sizeSmall(), theme.textMuted);
    contentY += theme.px(28);
    const int shotsY = artBox.bottom() - theme.px(128);
    const int startH = theme.px(62);
    const int startY = shotsY - theme.px(20) - startH;
    std::vector<Game::Variant> versions = gameActionGame_.versions;
    if (versions.empty()) versions.push_back({gameActionGame_.path, "Standard"});
    const int listBottom = startY - theme.px(16);
    const int rowH = theme.px(44);
    const int rowGap = theme.px(5);
    const int visibleRows = std::max(1, (listBottom - contentY) / (rowH + rowGap));
    const int lastVisible = std::min(int(versions.size()), gameVersionScroll_ + visibleRows);
    for (int rowIndex = gameVersionScroll_; rowIndex < lastVisible; ++rowIndex) {
        const auto &version = versions[size_t(rowIndex)];
        const Rect row{rightX, contentY, rightW, rowH};
        if (gameActionCursor_ == rowIndex + 1)
            canvas.fillRoundedRect(row, theme.px(8), theme.surfaceHi);
        const bool selected = gameActionGame_.versions.empty()
            ? version.path == gameActionGame_.path
            : rowIndex == gameActionSelectedVersion_;
        theme.regular().draw(canvas, rightX + theme.px(14), contentY + theme.px(9),
            theme.regular().elide(version.label, theme.sizeBody(), rightW - theme.px(160)),
            theme.sizeBody(), gameActionCursor_ == rowIndex + 1 ? theme.textPrimary : theme.textMuted);
        if (selected)
            theme.regular().draw(canvas, row.right() - theme.px(138), contentY + theme.px(12),
                "Selected", theme.sizeSmall(), theme.accent);
        contentY += rowH + rowGap;
    }

    const Rect start{rightX, startY, rightW, startH};
    const bool startFocused = gameActionCursor_ == 0;
    if (startFocused) canvas.fillRoundedRect(start, theme.radius(), theme.accent);
    else canvas.fillRoundedRect(start, theme.radius(), theme.surface);
    const bool silverTheme = std::string(Theme::colorThemeName(theme.colorTheme())) == "Silver";
    const Color startText = theme.highContrast() ? contrastingText(theme.accent) :
        theme.darkMode() ? (silverTheme ? Color::rgb(0x111318) : Color::rgb(0xFFFFFF)) :
        contrastingText(theme.accent);
    if (startFocused) {
        const int inset = std::max(1, theme.px(2));
        canvas.strokeRoundedRect({start.x + inset, start.y + inset, start.w - 2 * inset,
                                  start.h - 2 * inset}, std::max(0, theme.radius() - inset),
                                 std::max(1, theme.px(1)), startText);
    }
    theme.bold().draw(canvas, start.x + theme.px(22),
                      start.y + (start.h - theme.sizeBody()) / 2,
                      "Start game", theme.sizeBody(), startText);

    theme.bold().draw(canvas, rightX, shotsY, "SCREENSHOTS", theme.sizeSmall(), theme.textMuted);
    const int shotY = shotsY + theme.px(24);
    const int shotGap = theme.px(10);
    const int shotW = (rightW - shotGap * 3) / 4;
    const int shotH = theme.px(104);
    const auto screenshots = gameScreenshots_.snapshot();
    if (!gameActionScreenshotOverlay_ &&
        screenshots.state == GameScreenshotService::State::Ready)
        gameActionScreenshotPaths_ = screenshots.screenshots;
    gameActionScreenshotReady_ = false;
    for (int i = 0; i < 4; ++i) {
        Rect thumb{rightX + i * (shotW + shotGap), shotY, shotW, shotH};
        canvas.fillRoundedRect(thumb, theme.px(5), theme.surface);
        if (i < int(gameActionScreenshotPaths_.size()) ||
            (i == 0 && gameActionScreenshotPaths_.empty() &&
             !gameActionGame_.backgroundCandidates.empty())) {
            ImagePtr shot = gameActionScreenshotPaths_.empty()
                ? context_->images.requestAsyncCandidates(
                    gameActionGame_.backgroundCandidates, thumb.w, thumb.h, true)
                : context_->images.requestAsync(gameActionScreenshotPaths_[size_t(i)], "",
                                                thumb.w, thumb.h, true);
            if (shot) {
                canvas.drawImage(*shot, thumb, 255, theme.px(5));
                gameActionScreenshotReady_ = true;
            }
        }
        const bool focused = gameActionScreenshotReady_ && i == gameActionScreenshotCursor_ &&
                             gameActionCursor_ == 100 + i;
        canvas.strokeRoundedRect(thumb, theme.px(5), std::max(1, theme.px(2)),
                                 focused ? theme.focus : theme.surfaceHi);
    }
    if (!gameActionScreenshotReady_)
        theme.regular().drawCentered(canvas, {rightX, shotY, rightW, shotH},
                                     (screenshots.state == GameScreenshotService::State::Searching ||
                                      !gameActionScreenshotPaths_.empty())
                                         ? "Loading screenshots…" : "No screenshots available",
                                     theme.sizeSmall(),
                                     theme.textMuted);

    if (gameActionScreenshotOverlay_ && gameActionScreenshotReady_) {
        canvas.fillRect(bounds, theme.shadow.withAlpha(230));
        const Rect imageArea{bounds.x + theme.px(70), bounds.y + theme.px(36),
                             bounds.w - theme.px(140), bounds.h - theme.px(100)};
        ImagePtr shot = gameActionScreenshotPaths_.empty()
            ? context_->images.requestAsyncCandidates(
                gameActionGame_.backgroundCandidates, imageArea.w, imageArea.h, true)
            : context_->images.requestAsync(
                gameActionScreenshotPaths_[size_t(gameActionScreenshotCursor_)], "",
                imageArea.w, imageArea.h, true);
        if (shot) {
            const int imageX = imageArea.x + (imageArea.w - shot->width()) / 2;
            const int imageY = imageArea.y + (imageArea.h - shot->height()) / 2;
            canvas.drawImage(*shot, {imageX, imageY, shot->width(), shot->height()}, 255, 0);
        } else theme.regular().drawCentered(canvas, imageArea, "Loading screenshot…",
                                            theme.sizeBody(), theme.textMuted);
        theme.bold().drawCentered(canvas,
            {bounds.x, bounds.bottom() - theme.px(60), bounds.w, theme.px(32)},
            std::to_string(gameActionScreenshotCursor_ + 1) + " / " +
                std::to_string(std::max<size_t>(1, gameActionScreenshotPaths_.size())),
            theme.sizeBody(), theme.textPrimary);
    }
    renderBottomBar({0, canvas.height() - theme.bottomBarHeight(), canvas.width(),
                     theme.bottomBarHeight()});
}

void App::dispatch(Action action) {
    switch (action) {
    case Action::Up: case Action::Down: case Action::Left: case Action::Right:
    case Action::JumpPrev: case Action::JumpNext:
        if (preferences_.menuSoundsEnabled()) UiSounds::playNavigation();
        break;
    default:
        break;
    }
    // Start usually shares Confirm's action. Search fields reserve it for submitting queries.
    if (action == Action::Start &&
        !(tab_ == Tab::Settings && settingsScreen_->wantsStartForWifiPassword()) &&
        !activeScreen()->usesStartAction())
        action = Action::Confirm;
    if (action == Action::Quit) DebugLog::warn("input: quit action received");
    if (updateService_.commitRunning() &&
        (action == Action::Quit || action == Action::Confirm)) return;
    const UpdatePhase updatePhase = updateService_.snapshot().phase;
    if (updatePhase == UpdatePhase::Confirming) {
        settingsScreen_->setUpdateSnapshot(updateService_.snapshot());
        if (action == Action::Quit) {
            updateService_.cancelBeforeCommit();
            stop();
        } else {
            settingsScreen_->handle(action);
        }
        return;
    }
    if (updatePhase == UpdatePhase::RestartRequired &&
        updateService_.snapshot().successPromptVisible && !context_->errorActive &&
        activeScreen() == settingsScreen_.get()) {
        settingsScreen_->setUpdateSnapshot(updateService_.snapshot());
        if (action == Action::Quit) {
            updateService_.dismissInstallSuccess();
            stop();
        } else {
            settingsScreen_->handle(action);
        }
        return;
    }
    if ((updatePhase == UpdatePhase::Downloading || updatePhase == UpdatePhase::Verifying) &&
        action == Action::Back) {
        updateService_.cancelBeforeCommit();
        return;
    }
    const std::vector<Tab> tabs = visibleTabs();
    const int tabCount = int(tabs.size());
    const int tabIndex = int(std::find(tabs.begin(), tabs.end(), tab_) - tabs.begin());

    // Above even the wizard: an error is raised specifically because something needs the
    // user's attention before anything else proceeds, including a scan already in progress.
    // Quit is the one action that still goes through — it should never be the one thing an
    // error dialog blocks a way out of.
    if (context_->errorActive) {
        if (action == Action::Quit) { stop(); return; }
        if (action == Action::Confirm || action == Action::Back) {
            context_->errorActive = false;
            needsFullRedraw_ = true;
        }
        return;
    }

    if (gameActionsActive_) {
        if (gameActionScreenshotOverlay_) {
            const int screenshotCount = gameActionScreenshotReady_ ?
                int(std::max<size_t>(1, gameActionScreenshotPaths_.size())) : 0;
            if (action == Action::Quit) stop();
            else if (action == Action::Back || action == Action::Confirm)
                gameActionScreenshotOverlay_ = false;
            else if (action == Action::Left && gameActionScreenshotCursor_ > 0)
                --gameActionScreenshotCursor_;
            else if (action == Action::Right &&
                     gameActionScreenshotCursor_ + 1 < screenshotCount)
                ++gameActionScreenshotCursor_;
            needsFullRedraw_ = true;
            return;
        }
        if (action == Action::Quit || action == Action::Back) {
            gameActionsActive_ = false;
            needsFullRedraw_ = true;
        } else if (action == Action::Up || action == Action::Down) {
            const int displayedVersions = int(std::max<size_t>(1, gameActionGame_.versions.size()));
            if (gameActionCursor_ == 0) {
                if (action == Action::Up) gameActionCursor_ = displayedVersions;
                else if (gameActionScreenshotReady_) {
                    gameActionScreenshotCursor_ = 0;
                    gameActionCursor_ = 100;
                }
            } else if (gameActionCursor_ >= 100) {
                if (action == Action::Up) gameActionCursor_ = 0;
            } else {
                if (action == Action::Up)
                    gameActionCursor_ = std::max(1, gameActionCursor_ - 1);
                else
                    gameActionCursor_ = gameActionCursor_ < displayedVersions
                        ? gameActionCursor_ + 1 : 0;
            }
            if (gameActionCursor_ > 0 && gameActionCursor_ < 100) {
                const int visibleRows = std::max(1, theme_->px(300) / theme_->px(49));
                const int row = gameActionCursor_ - 1;
                if (row < gameVersionScroll_) gameVersionScroll_ = row;
                if (row >= gameVersionScroll_ + visibleRows)
                    gameVersionScroll_ = row - visibleRows + 1;
            }
        } else if ((action == Action::Left || action == Action::Right) &&
                   gameActionScreenshotReady_ &&
                   gameActionCursor_ >= 100) {
            if (action == Action::Left && gameActionScreenshotCursor_ > 0)
                --gameActionScreenshotCursor_;
            else if (action == Action::Right &&
                     gameActionScreenshotCursor_ + 1 < int(std::max<size_t>(1,
                         gameActionScreenshotPaths_.size())))
                ++gameActionScreenshotCursor_;
            gameActionCursor_ = 100 + gameActionScreenshotCursor_;
        } else if (action == Action::Confirm) {
            if (gameActionScreenshotReady_ && gameActionCursor_ >= 100) {
                gameActionScreenshotOverlay_ = true;
            } else if (gameActionCursor_ == 0) {
                const std::string path = gameActionGame_.versions.empty()
                    ? gameActionGame_.path
                    : gameActionGame_.versions[size_t(gameActionSelectedVersion_)].path;
                Game selected = Library::makeGame(*gameActionSystem_, path);
                if (context_->launcher.launchGame(*gameActionSystem_, selected)) {
                    history_.remember(gameActionSystem_->name, selected.path, gameActionGame_.name);
                    gameActionsActive_ = false;
                    context_->standDown = true;
                } else context_->showError("Could not start " + gameActionGame_.name,
                                           context_->launcher.lastError());
            } else {
                if (!gameActionGame_.versions.empty())
                    gameActionSelectedVersion_ = gameActionCursor_ - 1;
                gameActionCursor_ = 0;
                needsFullRedraw_ = true;
            }
        }
        return;
    }

    // The wizard is modal: nothing behind it is reachable while it is up.
    if (scanActive_) {
        if (action == Action::Quit) stop();
        else scanScreen_->handle(action);
        return;
    }

    // Likewise the show/hide list: it owns the screen until Back closes it.
    if (visibilityActive_) {
        if (action == Action::Quit) stop();
        else visibilityScreen_->handle(action);
        return;
    }

    // And the controller-management flow — modal the same way, for the same reason.
    if (controllersActive_) {
        if (action == Action::Quit) stop();
        else controllersScreen_->handle(action);
        return;
    }

    // Arcade Games sits on top of Manage Arcade, which sits on top of Settings — same modal
    // rule, checked in that nesting order.
    if (arcadeGamesActive_) {
        if (action == Action::Quit) stop();
        else arcadeGamesScreen_->handle(action);
        return;
    }
    if (arcadeSettingsActive_) {
        if (action == Action::Quit) stop();
        else arcadeSettingsScreen_->handle(action);
        return;
    }

    switch (action) {
    case Action::Quit:
        stop();
        return;
    case Action::TabPrev:
        selectTab(tabs[size_t((tabIndex + tabCount - 1) % tabCount)]);
        return;
    case Action::TabNext:
        selectTab(tabs[size_t((tabIndex + 1) % tabCount)]);
        return;
    case Action::Back:
        if (activeScreen()->hasBackOverlay()) {
            activeScreen()->handle(Action::Back);
            return;
        }
        if (detailActive_) { detailActive_ = false; needsFullRedraw_ = true; return; }
        if (tab_ == Tab::Settings && settingsScreen_->wantsBack()) {
            settingsScreen_->handle(Action::Back);
            return;
        }
        // On the Arcade tab, Back first steps from a tile to its row's title.
        if (tab_ != Tab::Home) { selectTab(Tab::Home); return; }
        break;
    case Action::Select:
        if (activeScreen()->usesStartAction()) {
            activeScreen()->handle(action);
            return;
        }
        openGameActions();
        return;
    default:
        break;
    }

    activeScreen()->handle(action);
}

void App::renderBackground(Canvas &target) {
    Theme &theme = *theme_;

    target.verticalGradient(target.bounds(), theme.background, theme.backgroundLo);

    if (theme.darkMode() || theme.highContrast()) {
        // A soft band behind the top bar lifts the status row off the content.
        const Rect band{0, 0, target.width(), theme.topBarHeight()};
        target.verticalGradient(band, theme.backgroundLo.withAlpha(210),
                                theme.background.withAlpha(0));
    } else {
        // Keep the existing light icons legible, then fade the black bar into the theme tint.
        const Color bar = Color::rgb(0x07090D);
        const Rect barRect{0, 0, target.width(), theme.topBarHeight()};
        target.fillRect(barRect, bar);
        const Rect fade{0, barRect.bottom(), target.width(), theme.px(112)};
        target.verticalGradient(fade, bar.withAlpha(238), theme.background.withAlpha(0));
    }

    target.clearDamage();
}

void App::renderBottomBar(const Rect &area) {
    Theme &theme = *theme_;
    Canvas &canvas = *canvas_;

    const bool favoriteHolding = input_.favoriteHoldActive() &&
                                 favoriteHoldScreen_ == activeScreen() &&
                                 !favoriteHoldPath_.empty();
    std::string activeHints = activeScreen()->hints();
    if (gameActionsActive_) {
        if (gameActionScreenshotOverlay_)
            activeHints = "←/→ Browse screenshots   A/B Close";
        else if (gameActionScreenshotReady_ && gameActionCursor_ >= 100)
            activeHints = "↑ Start game   ←/→ Browse screenshots   A View screenshot   B Close";
        else if (gameActionCursor_ == 0)
            activeHints = gameActionScreenshotReady_
                ? "↑ Choose ROM   ↓ View screenshots   A Start game   B Close"
                : "↑ Choose ROM   A Start game   B Close";
        else activeHints = "↑/↓ Choose ROM   A Select version   B Close";
    }
    const std::string hints = formatButtonLabels(activeHints);
    const int iconSize = theme.hintIconSize();
    const int iconGap = theme.px(5);
    // The hints use their own size setting. If a long hint line would run into the version text
    // at the right, it steps down until it fits (never below the normal small size).
    std::string versionText = std::string("v") + kAppVersion;
    if (updateSnapshot_.phase == UpdatePhase::Available && updateSnapshot_.canInstall)
        versionText += "  ·  v" + updateSnapshot_.targetVersion + " available";
    const int room = area.w - 2 * theme.marginX() -
                     theme.regular().measure(versionText, theme.sizeSmall()) - theme.gap();
    int hintSize = theme.sizeHint();
    while (hintSize > theme.sizeSmall() &&
           hintTextWidth(theme.bold(), hints, hintSize, iconSize, iconGap) > room)
        --hintSize;
    const int y = area.y + (area.h - theme.regular().lineHeight(hintSize)) / 2;

    if (favoriteHolding) {
        const bool removing = favorites_.contains(favoriteHoldPath_);
        const std::string label = formatButtonLabels(
            removing ? "Hold X for 2s to remove favorite" :
                      "Hold X for 2s to add favorite");
        const int x = area.x + theme.marginX();
        theme.bold().draw(canvas, x, area.y + theme.px(8), "*", theme.sizeBody(),
                          theme.favorite);
        theme.regular().draw(canvas, x + theme.px(24), area.y + theme.px(9), label,
                             theme.sizeSmall(), theme.textPrimary);

        const int barWidth = std::min(theme.px(480), area.w / 2);
        const Rect track{x, area.bottom() - theme.px(16), barWidth, theme.px(6)};
        canvas.fillRoundedRect(track, theme.px(3), theme.surfaceHi);
        const int filled = std::max(1, int(barWidth * input_.favoriteHoldProgress()));
        canvas.fillRoundedRect({track.x, track.y, filled, track.h}, theme.px(3),
                               theme.favorite);
    } else if (theme.highContrast()) {
        const int x = area.x + theme.marginX();
        const int textWidth = hintTextWidth(theme.bold(), hints, hintSize, iconSize,
                                            iconGap);
        const int paddingX = theme.px(10);
        const int paddingY = theme.px(4);
        const int lineHeight = theme.bold().lineHeight(hintSize);
        const Rect hintFrame{x - paddingX, y - paddingY,
                             textWidth + paddingX * 2, lineHeight + paddingY * 2};
        canvas.fillRoundedRect(hintFrame, theme.px(6), theme.surfaceHi);
        canvas.strokeRoundedRect(hintFrame, theme.px(6), theme.px(2), theme.focus);
        drawHintText(canvas, theme, theme.bold(), x, y, hintSize, theme.textPrimary,
                     hints);
    } else {
        drawHintText(canvas, theme, theme.regular(), area.x + theme.marginX(), y,
                     hintSize, theme.textMuted, hints);
    }

    // Small and out of the way in the corner — a build identifier for bug reports, not
    // something meant to draw the eye. A newer release found on GitHub (see UpdateService,
    // Settings → Check for updates) rides along in the one place a version number already
    // draws attention, rather than a notification competing with everything else.
    std::string version = std::string("v") + kAppVersion;
    const bool updateAvailable = updateSnapshot_.phase == UpdatePhase::Available &&
                                 updateSnapshot_.canInstall;
    if (updateAvailable) version += "  ·  v" + updateSnapshot_.targetVersion + " available";
    const int versionWidth = theme.regular().measure(version, theme.sizeSmall());
    theme.regular().draw(canvas, area.right() - theme.marginX() - versionWidth, y, version,
                         theme.sizeSmall(),
                         updateAvailable ? theme.accent.withAlpha(200)
                                         : theme.textMuted.withAlpha(155));

    if (!favoriteHolding && context_->messageTimer > 0.0f && !context_->message.empty()) {
        // Sits to the left of the version string so the two never overlap.
        const int width = theme.regular().measure(context_->message, theme.sizeSmall());
        const uint8_t alpha = uint8_t(std::min(1.0f, context_->messageTimer) * 235);
        const int reserved = versionWidth + theme.gap();
        theme.regular().draw(canvas, area.right() - theme.marginX() - reserved - width, y,
                             context_->message, theme.sizeSmall(),
                             theme.success.withAlpha(alpha));
    }
}

void App::writeCanvas(const std::string &path) {
    FILE *out = std::fopen(path.c_str(), "wb");
    if (!out) {
        std::printf("app: cannot write %s\n", path.c_str());
        return;
    }

    for (int y = 0; y < canvas_->height(); ++y)
        std::fwrite(canvas_->row(y), 4, size_t(canvas_->width()), out);
    std::fclose(out);

    std::printf("app: canvas written to %s (%dx%d)\n", path.c_str(), canvas_->width(),
                canvas_->height());
    std::fflush(stdout);
}

int App::run() {
    running_ = true;
    DebugLog::info("run: entering frame loop");
    int64_t previous = nowMs();
    int64_t rescanAt = previous + 1000;
    const int64_t startupHomeRefreshAt = previous + 2000;
    int frame = 0;
    bool firstFrameLogged = false;
    bool startupHomeRefreshDone = false;
    bool previousFrameWasFull = false;

    while (running_) {
        if (signalStopRequested_) { running_ = false; break; }
        const int64_t frameStart = nowMs();
        const float dt = float(frameStart - previous) / 1000.0f;
        previous = frameStart;

        if (options_.readInput && frameStart >= rescanAt) {
            input_.rescan();
            rescanAt = frameStart + 1000;
        }

        // The button-mapping wizard's capture step reads raw evdev state directly (see
        // ControllersScreen::update) and must be the only thing that gets to interpret a
        // press while it is doing that — otherwise the very button being captured as this
        // wizard's answer would also fire through the ordinary Action pipeline below and,
        // say, back out of the wizard entirely. poll() itself still has to run every frame
        // regardless (it is what notices a disconnect and re-opens a replacement device);
        // only dispatching what it returns is what gets skipped.
        runScript();

        const bool suppressActions = controllersActive_ && controllersScreen_->wantsRawInput();
        std::vector<Action> actions = input_.poll(kTargetFrameMs);
        if (!actions.empty()) {
            idleSeconds_ = 0.0f;
            if (screensaverActive_) {
                screensaverActive_ = false;
                screensaverDrawn_ = false;
                screensaverArt_.reset();
                needsFullRedraw_ = true;
                actions.clear();
            }
        } else {
            idleSeconds_ += dt;
        }
        if (!suppressActions) {
            if (input_.favoriteHoldActive()) {
                Screen *screen = activeScreen();
                const std::string path = screen->favoritePath();
                if (path.empty() ||
                    (favoriteHoldScreen_ &&
                     (favoriteHoldScreen_ != screen || favoriteHoldPath_ != path))) {
                    input_.cancelFavoriteHold();
                } else if (!favoriteHoldScreen_) {
                    favoriteHoldScreen_ = screen;
                    favoriteHoldPath_ = path;
                }
            }

            for (Action action : actions) {
                if (action == Action::ToggleFavorite) {
                    // The input layer emits this only after 2 seconds. Check the exact
                    // selected path again before a removal can happen.
                    if (favoriteHoldScreen_ == activeScreen() &&
                        !favoriteHoldPath_.empty() &&
                        favoriteHoldPath_ == activeScreen()->favoritePath())
                        dispatch(action);
                } else {
                    dispatch(action);
                    if (favoriteHoldScreen_ &&
                        (favoriteHoldScreen_ != activeScreen() ||
                         favoriteHoldPath_ != activeScreen()->favoritePath()))
                        input_.cancelFavoriteHold();
                }
            }
        } else {
            input_.cancelFavoriteHold();
        }
        if (!input_.favoriteHoldActive()) {
            favoriteHoldScreen_ = nullptr;
            favoriteHoldPath_.clear();
        }
        if (!running_) break;
        if (signalStopRequested_) { running_ = false; break; }

        // A core was started: hand the devices and the console straight back.
        if (context_->standDown) {
            if (preferences_.menuSoundsEnabled()) UiSounds::playGameLaunch();
            input_.releaseAll();
            console_.release();
            DebugLog::info("run: handed control to a core");
            std::printf("app: standing down, a core was started\n");
            break;
        }

        // One system per frame while scanning, one game per frame while fetching artwork.
        // Pacing the work this way is what keeps the interface alive and stops the drive
        // being hammered flat out.
        if (scan_.running()) scan_.step();
        else if (scraper_.running()) scraper_.step();
        else if (assetsDownloadActive_) {
            if (systemIconDownload_ && systemIconDownload_->running())
                systemIconDownload_->step();
            else if (tabIconDownload_ && tabIconDownload_->running())
                tabIconDownload_->step();
            if ((!systemIconDownload_ || !systemIconDownload_->running()) &&
                (!tabIconDownload_ || !tabIconDownload_->running())) {
                assetsDownloadActive_ = false;
                needsFullRedraw_ = true;
                const bool failed =
                    (systemIconDownload_ && systemIconDownload_->state() == SystemIconDownload::State::Failed) ||
                    (tabIconDownload_ && tabIconDownload_->state() == TabIconDownload::State::Failed);
                if (failed) {
                    const std::string error = systemIconDownload_ &&
                        systemIconDownload_->state() == SystemIconDownload::State::Failed
                        ? systemIconDownload_->error() : tabIconDownload_->error();
                    context_->notify(error, 8.0f);
                } else {
                    context_->notify("All console and navigation icons are ready.", 6.0f);
                }
            }
        }
        else if (spaceGroteskDownload_.running()) {
            spaceGroteskDownload_.step();
            if (!spaceGroteskDownload_.running()) {
                needsFullRedraw_ = true;
                if (spaceGroteskDownload_.state() == SpaceGroteskDownload::State::Done) {
                    preferences_.setUseSpaceGrotesk(true);
                    theme_->setSpaceGroteskEnabled(true);
                    context_->notify("Space Grotesk font installed and enabled", 6.0f);
                } else {
                    context_->notify(spaceGroteskDownload_.error(), 8.0f);
                }
            }
        }


        if (!autoCheckDecisionMade_) {
            updateCheckDelay_ -= dt;
            if (updateCheckDelay_ <= 0.0f) {
                autoCheckDecisionMade_ = true;
                if (preferences_.checkForUpdates()) updateService_.startCheck(true);
            }
        }
        updateSnapshot_ = updateService_.snapshot();
        settingsScreen_->setUpdateSnapshot(updateSnapshot_);
        if (updateSnapshot_.phase == UpdatePhase::RestartRequired &&
            updateSnapshot_.successPromptVisible && !context_->errorActive &&
            !scanActive_ && !visibilityActive_ && !controllersActive_ &&
            !arcadeGamesActive_ && !arcadeSettingsActive_ && tab_ != Tab::Settings)
            selectTab(Tab::Settings);

        discCoreRefreshTimer_ -= dt;
        if (discCoreRefreshTimer_ <= 0.0f) {
            discCoreRefreshTimer_ = 3.0f;
            const auto current = discCores_.snapshot();
            if (current.alternateMainInstalled != discCoreStatus_.alternateMainInstalled ||
                current.alternateMainVerified != discCoreStatus_.alternateMainVerified ||
                current.psxMglInstalled != discCoreStatus_.psxMglInstalled ||
                current.saturnMglInstalled != discCoreStatus_.saturnMglInstalled ||
                current.psxMglCompatible != discCoreStatus_.psxMglCompatible ||
                current.saturnMglCompatible != discCoreStatus_.saturnMglCompatible ||
                current.psxMglVerified != discCoreStatus_.psxMglVerified ||
                current.saturnMglVerified != discCoreStatus_.saturnMglVerified ||
                current.psxCoreInstalled != discCoreStatus_.psxCoreInstalled ||
                current.saturnCoreInstalled != discCoreStatus_.saturnCoreInstalled ||
                current.scopedMainConfigured != discCoreStatus_.scopedMainConfigured) {
                discCoreStatus_ = current;
                needsFullRedraw_ = true;
            }
        }
        // Check for framebuffer mode changes every two seconds. MiSTer_fb can clear the
        // framebuffer when Main_MiSTer reapplies the mode, so request one full redraw for
        // each observed counter change. Updating the saved count below prevents repeated
        // redraws while the counter stays unchanged. Diagnostic logging remains bounded.
        framebufferWatchTimer_ -= dt;
        if (framebufferWatchTimer_ <= 0.0f) {
            framebufferWatchTimer_ = 2.0f;
            const long count = readFramebufferResCount();
            if (count >= 0 && count != framebufferResCount_) {
                if (framebufferResCount_ >= 0) needsFullRedraw_ = true;
                if (framebufferResLogged_ < 20) {
                    ++framebufferResLogged_;
                    char message[160];
                    std::snprintf(message, sizeof(message),
                                  "framebuffer: MiSTer_fb res_count %ld -> %ld (kernel uptime %.1f s, tab=%d)",
                                  framebufferResCount_, count, uptimeSeconds(), int(tab_));
                    DebugLog::info(message);
                }
                framebufferResCount_ = count;
            }
        }
        if (tab_ == Tab::Disc) {
            const std::vector<Tab> tabs = visibleTabs();
            if (std::find(tabs.begin(), tabs.end(), Tab::Disc) == tabs.end())
                selectTab(Tab::Home);
        }

        Theme &theme = *theme_;
        if (!screensaverActive_ && preferences_.screensaverDelay() > 0 &&
            idleSeconds_ >= preferences_.screensaverDelay()) {
            screensaverActive_ = true;
            screensaverDrawn_ = false;
            screensaverArt_.reset();
            screensaverArtTimer_ = 0.0f;
            screensaverArtPending_ = false;
            screensaverNeedsBlackout_ = true;
        }
        topBar_.update(dt, preferences_.use24HourClock());
        activeScreen()->update(dt);
        if (context_->messageTimer > 0.0f) context_->messageTimer -= dt;
        const auto backup = backupTask_.snapshot();
        if (backup.state != backupStateReported_ && backup.state != BackupTask::State::Running) {
            backupStateReported_ = backup.state;
            if (backup.state == BackupTask::State::Complete) {
                const size_t slash = backup.path.find_last_of('/');
                const std::string folder = slash == std::string::npos
                                               ? std::string("USB backup folder")
                                               : backup.path.substr(0, slash);
                context_->notify("Backup saved to " + folder, 10.0f);
            }
            else if (backup.state == BackupTask::State::Failed)
                context_->showError("Backup not created",
                                    "No settings were changed. Connect one writable USB drive and try again.");
        }

        // Measured on the device against a real scraped cover: ~22ms to decode, well down
        // from the ~47ms a full-size PNG cost before the scraper started shrinking and
        // re-encoding artwork as JPEG. 32ms reliably fits two decodes into one frame instead
        // of the one the old, PNG-sized budget allowed, which is what "successive loading"
        // actually feels like while browsing a freshly opened system.
        images_.beginFrame(32);

        if (screensaverActive_ && preferences_.screensaverStyle() == "boxart") {
            screensaverArtTimer_ -= dt;
            if (screensaverArtPending_) {
                screensaverArtLoadTimer_ += dt;
                screensaverArt_ = images_.requestAsyncCandidates(
                    screensaverArtCandidates_, theme.px(250), theme.px(350));
                if (screensaverArt_) {
                    screensaverArtPending_ = false;
                    screensaverArtTimer_ = 6.0f;
                } else if (screensaverArtLoadTimer_ >= 3.0f) {
                    screensaverArtPending_ = false;
                    screensaverArtTimer_ = 10.0f;
                }
            } else if (screensaverArtTimer_ <= 0.0f) {
                screensaverArt_.reset();
                screensaverNeedsBlackout_ = true;
                screensaverArtTimer_ = 10.0f;
                const auto &systems = library_.systems();
                if (!systems.empty()) {
                    screensaverSeed_ = screensaverSeed_ * 1664525u + 1013904223u;
                    const GameSystem &system = systems[screensaverSeed_ % systems.size()];
                    const auto paths = library_.pathsOf(system);
                    if (!paths.empty()) {
                        screensaverSeed_ = screensaverSeed_ * 1664525u + 1013904223u;
                        Game game = Library::makeGame(system, paths[screensaverSeed_ % paths.size()],
                                                      ArtworkVariant::Grid);
                        screensaverArtCandidates_ = game.boxartCandidates;
                        screensaverArt_ = images_.requestAsyncCandidates(
                            screensaverArtCandidates_, theme.px(250), theme.px(350));
                        if (screensaverArt_) screensaverArtTimer_ = 6.0f;
                        else {
                            screensaverArtPending_ = true;
                            screensaverArtLoadTimer_ = 0.0f;
                        }
                        const int margin = theme.px(50);
                        const int maxX = std::max(margin, canvas_->width() - theme.px(300));
                        const int maxY = std::max(margin, canvas_->height() - theme.px(400));
                        screensaverSeed_ = screensaverSeed_ * 1664525u + 1013904223u;
                        screensaverArtRect_ = {margin + int(screensaverSeed_ % unsigned(maxX - margin + 1)),
                                               margin + int((screensaverSeed_ >> 8) % unsigned(maxY - margin + 1)),
                                               theme.px(250), theme.px(350)};
                    }
                }
            }
        }

        const Rect top{0, 0, canvas_->width(), theme.topBarHeight()};
        const Rect bottom{0, canvas_->height() - theme.bottomBarHeight(), canvas_->width(),
                          theme.bottomBarHeight()};
        const Rect content{theme.marginX(), top.bottom() + theme.marginY(),
                           canvas_->width() - 2 * theme.marginX(),
                           bottom.y - top.bottom() - 2 * theme.marginY()};

        Screen *screen = activeScreen();
        // MiSTer can clear the mapped framebuffer shortly after the splash on cold boot.
        // Give Home one delayed full repaint to recover, then leave the resting screen alone.
        if (!startupHomeRefreshDone && frameStart >= startupHomeRefreshAt) {
            startupHomeRefreshDone = true;
            if (screen == homeScreen_.get()) needsFullRedraw_ = true;
        }
        // An error modal draws over whatever screen is underneath rather than replacing it,
        // so that screen must be fully repainted every frame it is up — there is no damage
        // tracking for "the same dialog is still sitting on top of you".
        const bool full = needsFullRedraw_ || !options_.incremental || context_->errorActive ||
                          gameActionsActive_;
        if (full && !previousFrameWasFull) {
            char message[160];
            std::snprintf(message, sizeof(message),
                          "run: full redraw begins (requested=%d, incremental=%d, error=%d, modal=%d, "
                          "kernel uptime %.1f s)",
                          needsFullRedraw_ ? 1 : 0, options_.incremental ? 1 : 0,
                          context_->errorActive ? 1 : 0, gameActionsActive_ ? 1 : 0,
                          uptimeSeconds());
            DebugLog::info(message);
        }

        canvas_->clearDamage();

        if (screensaverActive_) {
            const bool fullSaver = !screensaverDrawn_;
            if ((!screensaverDrawn_ || screensaverNeedsBlackout_) &&
                preferences_.screensaverStyle() == "boxart") {
                canvas_->clear(Color::rgb(0x000000));
                screensaverNeedsBlackout_ = false;
            }
            if (preferences_.screensaverStyle() == "dimmed") {
                if (!screensaverDrawn_) {
                    if (needsFullRedraw_ || !options_.incremental || context_->errorActive) {
                        canvas_->restoreFrom(*background_, canvas_->bounds());
                        topBar_.render(*canvas_, theme, {0, 0, canvas_->width(), theme.topBarHeight()},
                                       tab_, preferences_.showGamesTab(), arcadeTabShown(), false,
                                       tabNavigationAvailable(), preferences_.tabDisplay(), tabIcons_);
                        renderBottomBar(bottom);
                        screen->render(*canvas_, content, true);
                        if (context_->errorActive)
                            ErrorModal::draw(*canvas_, theme, canvas_->bounds(),
                                             context_->errorTitle, context_->errorMessage);
                        else if (gameActionsActive_)
                            renderGameActions();
                    }
                    canvas_->fillRect(canvas_->bounds(), Color::rgb(0x000000).withAlpha(190));
                }
            } else if (screensaverArt_) {
                const float elapsed = 6.0f - screensaverArtTimer_;
                const float reveal = elapsed < 0.8f ? elapsed / 0.8f * 100.0f
                                     : elapsed > 5.2f ? (6.0f - elapsed) / 0.8f * 100.0f
                                                      : 100.0f;
                canvas_->fillRect(screensaverArtRect_, Color::rgb(0x000000));
                canvas_->drawImageReveal(*screensaverArt_, screensaverArtRect_, 255,
                                         theme.px(12), int(std::max(0.0f, std::min(100.0f, reveal))));
            }
            if (fullSaver || !canvas_->damage().empty()) framebuffer_.present(*canvas_);
            screensaverDrawn_ = true;
            needsFullRedraw_ = false;
            if (options_.exitAfterFrames > 0 && ++frame >= options_.exitAfterFrames) break;
            continue;
        }

        const int64_t t0 = nowMs();
        // Instead of repainting the gradient every frame, the prepared background is copied
        // back only where something is about to be drawn.
        if (full) canvas_->restoreFrom(*background_, canvas_->bounds());
        else if (!screen->incremental()) canvas_->restoreFrom(*background_, content);
        const int64_t t1 = nowMs();

        if (!full) {
            canvas_->restoreFrom(*background_, top);
            canvas_->restoreFrom(*background_, bottom);
        }
        const auto drive = discDrive_.snapshot();
        const auto &cores = discCoreStatus_;
        const bool showDisc = drive.state != DiscDriveService::MediaState::NoDrive &&
                              cores.alternateMainInstalled && cores.scopedMainConfigured &&
                              cores.hasAnyCompatibleMgl();
        topBar_.render(*canvas_, theme, top, tab_, preferences_.showGamesTab(), arcadeTabShown(),
                       showDisc, tabNavigationAvailable(), preferences_.tabDisplay(), tabIcons_);
        renderBottomBar(bottom);
        const int64_t t2 = nowMs();

        screen->render(*canvas_, content, full);

        if (context_->errorActive)
            ErrorModal::draw(*canvas_, theme, canvas_->bounds(), context_->errorTitle,
                             context_->errorMessage);
        else if (gameActionsActive_)
            renderGameActions();
        const int64_t t3 = nowMs();

        if (full) framebuffer_.present(*canvas_);
        else framebuffer_.present(*canvas_, canvas_->damage());
        needsFullRedraw_ = false;
        previousFrameWasFull = full;
        if (!firstFrameLogged) {
            DebugLog::info("run: first frame presented");
            firstFrameLogged = true;
        }
        const int64_t t4 = nowMs();

        backgroundMs_ += t1 - t0;
        chromeMs_ += t2 - t1;
        screenMs_ += t3 - t2;
        presentMs_ += t4 - t3;

        if (screenshotRequested_) {
            screenshotRequested_ = 0;
            writeCanvas(kScreenshotPath);
        }

        if (options_.exitAfterFrames > 0 && ++frame >= options_.exitAfterFrames) break;
    }

    if (options_.stats && frame > 0) {
        std::printf("stats over %d frames (ms/frame): background %.1f  chrome %.1f  "
                    "screen %.1f  present %.1f\n",
                    frame, double(backgroundMs_) / frame, double(chromeMs_) / frame,
                    double(screenMs_) / frame, double(presentMs_) / frame);
    }

    if (!options_.dumpPath.empty()) writeCanvas(options_.dumpPath);

    updateService_.shutdown();
    console_.release();
    DebugLog::warn("run: exited without a fatal signal");
    return 0;
}
