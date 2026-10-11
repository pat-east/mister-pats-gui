#include "App.h"
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
    const int iconSize = theme.px(18);
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

} // namespace

bool App::initialize(const Options &options) {
    options_ = options;

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
    theme_->setHighContrast(preferences_.highContrast());
    setFaceButtonStyle(preferences_.faceButtonStyle());
    if (options_.splashMs > 0) {
        Splash::show(framebuffer_, *theme_, options_.splashMs, icons_, images_,
                     [this] { library_.load(); });
        if (preferences_.startSoundEnabled()) UiSounds::play();
    } else {
        library_.load();
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
                                        icons_, history_, hiddenSystems_, preferences_);
    context_->background = background_.get();
    context_->onThemeChanged = [this] {
        renderBackground(*background_);
        needsFullRedraw_ = true;
    };

    gamesScreen_ = std::make_unique<GamesScreen>(*context_);
    arcadeLibraryScreen_ = std::make_unique<GamesScreen>(*context_);
    allGamesScreen_ = std::make_unique<GamesScreen>(*context_);
    favoritesScreen_ = std::make_unique<GamesScreen>(*context_);

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
        [this] { openArcadeSettings(); },
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
        [this] { rebootMister(); });
    settingsScreen_->setFramebufferInfo(framebuffer_.describe());
    settingsScreen_->setUpdateSnapshot(updateSnapshot_);

    scanScreen_ = std::make_unique<ScanScreen>(
        *context_, scan_, scraper_, [this] { reloadLibrary(); }, [this] { closeScan(); });

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

    // Without a database there is nothing to show and no obvious way to fix that, so the
    // wizard opens by itself. It can be dismissed, which falls back to whatever Console Mode
    // left behind.
    if (options_.wizard && !library_.usingDatabase()) openScan(true);

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
    library_.load();
    favorites_.load();
    history_.load();
    systemsScreen_->refresh();
    homeScreen_->refresh();
    favoritesScreen_->showFavorites();
    allGamesScreen_->showAllGames();
    homeScreen_->invalidateSearchIndex();
    if (!gamesScreen_->empty()) gamesScreen_->reload();
    if (tab_ == Tab::Arcade) {
        if (const GameSystem *arcade = library_.arcadeSystem())
            arcadeLibraryScreen_->showSystem(*arcade);
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
    return TopBar::visibleTabs(preferences_.showGamesTab(), arcadeTabShown());
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
    detailActive_ = false;
    needsFullRedraw_ = true;
    if (tab == Tab::Home) homeScreen_->refresh();
    if (tab == Tab::Favorites) favoritesScreen_->showFavorites();
    if (tab == Tab::Arcade)
        if (const GameSystem *arcade = library_.arcadeSystem())
            arcadeLibraryScreen_->showSystem(*arcade);
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
    gameActionCursor_ = 0;
    gameActionWorking_ = false;
    gameActionsActive_ = true;
    input_.cancelFavoriteHold();
    favoriteHoldScreen_ = nullptr;
    favoriteHoldPath_.clear();
    needsFullRedraw_ = true;
}

void App::renderGameActions() {
    if (!gameActionsActive_) return;
    Canvas &canvas = *canvas_;
    Theme &theme = *theme_;
    const Rect bounds = canvas.bounds();
    canvas.fillRect(bounds, theme.shadow.withAlpha(175));

    const int width = std::min(theme.px(650), bounds.w - theme.px(64));
    const int height = theme.px(gameActionWorking_ ? 350 : 310);
    const Rect panel{bounds.x + (bounds.w - width) / 2, bounds.y + (bounds.h - height) / 2,
                     width, height};
    canvas.dropShadow(panel, theme.radius(), theme.px(24), theme.shadow.withAlpha(160));
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);

    const int left = panel.x + theme.px(36);
    const int textWidth = panel.w - 2 * theme.px(36);
    int y = panel.y + theme.px(32);
    theme.bold().draw(canvas, left, y, "Game options", theme.sizeHeading(), theme.textPrimary);
    y += theme.px(42);
    theme.regular().draw(canvas, left, y,
                         theme.regular().elide(gameActionGame_.name, theme.sizeBody(), textWidth),
                         theme.sizeBody(), theme.textMuted);
    y += theme.px(36);

    const std::string description = gameActionCursor_ == 0
        ? "Play starts this game."
        : "Find a missing cover online or prepare an existing cover for the game views.";
    theme.regular().draw(canvas, left, y,
                         theme.regular().elide(description, theme.sizeSmall(), textWidth),
                         theme.sizeSmall(), theme.textMuted);
    y += theme.px(34);

    if (gameActionWorking_) {
        const bool done = scraper_.finished();
        const bool succeeded = scraper_.state() == MediaScraper::State::Done;
        theme.bold().draw(canvas, left, y,
                          done ? (succeeded ? "Cover art ready" : "Could not prepare cover art")
                               : "Preparing cover art",
                          theme.sizeBody(), theme.textPrimary);
        y += theme.px(34);
        theme.regular().draw(canvas, left, y,
                             theme.regular().elide(scraper_.statusLine(), theme.sizeSmall(),
                                                   textWidth),
                             theme.sizeSmall(), theme.textMuted);
        y = panel.bottom() - theme.px(50);
        theme.regular().draw(canvas, left, y,
                             formatButtonLabels(done ? "A / B  Close" : "B  Cancel"),
                             theme.sizeSmall(), theme.textMuted);
        return;
    }

    const char *items[] = {"Play this game", "Find or prepare cover art"};
    for (int i = 0; i < 2; ++i) {
        const Rect row{left - theme.px(12), y - theme.px(7), width - theme.px(48),
                       theme.px(44)};
        if (i == gameActionCursor_)
            canvas.fillRoundedRect(row, theme.px(8), theme.accent.withAlpha(45));
        theme.bold().draw(canvas, left, y, items[i], theme.sizeBody(),
                          i == gameActionCursor_ ? theme.textPrimary : theme.textMuted);
        y += theme.px(52);
    }
    theme.regular().draw(canvas, left, panel.bottom() - theme.px(34),
                         formatButtonLabels("A  Choose     B  Close"), theme.sizeSmall(),
                         theme.textMuted);
}

void App::refreshGameActionArtwork() {
    if (gameActionSystem_) {
        const ArtworkVariant variants[] = {ArtworkVariant::Full, ArtworkVariant::Home,
                                           ArtworkVariant::Grid, ArtworkVariant::Small,
                                           ArtworkVariant::Detail, ArtworkVariant::Arcade};
        for (ArtworkVariant variant : variants) {
            const Game game = Library::makeGame(*gameActionSystem_, gameActionGame_.path, variant);
            context_->images.invalidateCandidates(game.boxartCandidates);
        }
    }
    // Repaint the current screen around the same selection. Its existing Game entries keep
    // their paths and candidate lists; only the stale decoded/missing image is discarded.
    needsFullRedraw_ = true;
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
        if (action == Action::Quit || action == Action::Back) {
            if (gameActionWorking_ && scraper_.running()) scraper_.cancel();
            gameActionsActive_ = false;
            gameActionWorking_ = false;
            needsFullRedraw_ = true;
        } else if (gameActionWorking_) {
            if ((action == Action::Confirm || action == Action::Select) && scraper_.finished()) {
                gameActionsActive_ = false;
                gameActionWorking_ = false;
                needsFullRedraw_ = true;
            }
        } else if (action == Action::Up || action == Action::Down) {
            gameActionCursor_ = 1 - gameActionCursor_;
        } else if (action == Action::Confirm) {
            if (gameActionCursor_ == 0) {
                gameActionsActive_ = false;
                needsFullRedraw_ = true;
                activeScreen()->handle(Action::Confirm);
            } else if (gameActionSystem_ && library_.usingDatabase() && !scraper_.running()) {
                MediaScraper::Options options;
                options.boxart = true;
                options.backgrounds = false;
                scraper_.startGame(library_.database(), options, gameActionSystem_->dbKey,
                                   gameActionGame_.path);
                gameActionWorking_ = true;
                gameActionScrapePendingRefresh_ = true;
            } else {
                context_->notify("First build the game list in Settings → Library.");
                gameActionsActive_ = false;
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

    // A soft band behind the top bar lifts the status row off the content.
    const Rect band{0, 0, target.width(), theme.topBarHeight()};
    target.verticalGradient(band, theme.backgroundLo.withAlpha(210),
                            theme.background.withAlpha(0));

    target.clearDamage();
}

void App::renderBottomBar(const Rect &area) {
    Theme &theme = *theme_;
    Canvas &canvas = *canvas_;

    const bool favoriteHolding = input_.favoriteHoldActive() &&
                                 favoriteHoldScreen_ == activeScreen() &&
                                 !favoriteHoldPath_.empty();
    const std::string hints = formatButtonLabels(activeScreen()->hints());
    const int iconSize = theme.px(18);
    const int iconGap = theme.px(5);
    const int y = area.y + (area.h - theme.regular().lineHeight(theme.sizeSmall())) / 2;

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
        const int textWidth = hintTextWidth(theme.bold(), hints, theme.sizeSmall(), iconSize,
                                            iconGap);
        const int paddingX = theme.px(10);
        const int paddingY = theme.px(4);
        const int lineHeight = theme.bold().lineHeight(theme.sizeSmall());
        const Rect hintFrame{x - paddingX, y - paddingY,
                             textWidth + paddingX * 2, lineHeight + paddingY * 2};
        canvas.fillRoundedRect(hintFrame, theme.px(6), theme.surfaceHi);
        canvas.strokeRoundedRect(hintFrame, theme.px(6), theme.px(2), theme.focus);
        drawHintText(canvas, theme, theme.bold(), x, y, theme.sizeSmall(), theme.textPrimary,
                     hints);
    } else {
        drawHintText(canvas, theme, theme.regular(), area.x + theme.marginX(), y,
                     theme.sizeSmall(), theme.textMuted, hints);
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
                             theme.accent.withAlpha(alpha));
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

        if (gameActionScrapePendingRefresh_ && scraper_.finished()) {
            if (scraper_.fetched() || scraper_.prepared()) refreshGameActionArtwork();
            gameActionScrapePendingRefresh_ = false;
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

        Theme &theme = *theme_;
        topBar_.update(dt, preferences_.use24HourClock());
        activeScreen()->update(dt);
        if (context_->messageTimer > 0.0f) context_->messageTimer -= dt;

        // Measured on the device against a real scraped cover: ~22ms to decode, well down
        // from the ~47ms a full-size PNG cost before the scraper started shrinking and
        // re-encoding artwork as JPEG. 32ms reliably fits two decodes into one frame instead
        // of the one the old, PNG-sized budget allowed, which is what "successive loading"
        // actually feels like while browsing a freshly opened system.
        images_.beginFrame(32);

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
                          "run: full redraw begins (requested=%d, incremental=%d, error=%d, modal=%d)",
                          needsFullRedraw_ ? 1 : 0, options_.incremental ? 1 : 0,
                          context_->errorActive ? 1 : 0, gameActionsActive_ ? 1 : 0);
            DebugLog::info(message);
        }

        canvas_->clearDamage();

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
        topBar_.render(*canvas_, theme, top, tab_, preferences_.showGamesTab(), arcadeTabShown(),
                       tabNavigationAvailable(), preferences_.tabDisplay(), tabIcons_);
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
