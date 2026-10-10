#include "App.h"
#include "DebugLog.h"
#include "ErrorModal.h"
#include "Splash.h"
#include "Version.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kTargetFrameMs = 33;   // ~30 fps; the CPU renders every pixel in software

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

    // Listing icon filenames is cheap; their PNG data is decoded only after the initial splash
    // wait, and is cached at the Systems tile size before the screen can be opened.
    icons_.load();
    if (options_.splashMs > 0)
        Splash::show(framebuffer_, *theme_, options_.splashMs, icons_, images_);

    systemIconDownload_ = std::make_unique<SystemIconDownload>(icons_);

    renderBackground(*background_);

    library_.load();
    favorites_.load();
    history_.load();
    hiddenSystems_.load();
    preferences_.load();
    updateService_.initializeLocalState();
    updateSnapshot_ = updateService_.snapshot();

    context_ = std::make_unique<Context>(*theme_, library_, favorites_, launcher_, images_,
                                        icons_, history_, hiddenSystems_, preferences_);
    context_->background = background_.get();

    gamesScreen_ = std::make_unique<GamesScreen>(*context_);
    allGamesScreen_ = std::make_unique<GamesScreen>(*context_);
    favoritesScreen_ = std::make_unique<GamesScreen>(*context_);

    const GameView initialView =
        options_.viewExplicit ? options_.view : gameViewFromName(preferences_.defaultView());
    gamesScreen_->setView(initialView);
    allGamesScreen_->setView(initialView);
    favoritesScreen_->setView(initialView);
    favoritesScreen_->showFavorites();

    homeScreen_ = std::make_unique<HomeScreen>(*context_);

    systemsScreen_ = std::make_unique<SystemsScreen>(*context_, [this](const GameSystem &system) {
        gamesScreen_->showSystem(system);
        detailActive_ = true;
        needsFullRedraw_ = true;
    });

    settingsScreen_ = std::make_unique<SettingsScreen>(
        *context_, [this] { reloadLibrary(); }, [this] { openScan(false); },
        [this] { openArtwork(); },
        [this] { startSystemIconDownload(); },
        [this] {
            return systemIconDownload_ ? systemIconDownload_->statusLine() : std::string();
        },
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
        [this] { updateService_.cancelBeforeCommit(); });
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

    arcadeScreen_ = std::make_unique<ArcadeScreen>(
        *context_, [this](ArcadeScreen::Dimension d) { openArcadeFull(d); },
        [this](ArcadeScreen::Dimension d, const DatabaseGroup &g) { openArcadeGroup(d, g); });
    arcadeScreen_->refresh();

    arcadeGroupsScreen_ = std::make_unique<ArcadeGroupsScreen>(
        *context_, [this](const DatabaseGroup &group) {
            openArcadeGroup(arcadeGroupsScreen_->manufacturers()
                                ? ArcadeScreen::Dimension::Manufacturers
                                : ArcadeScreen::Dimension::Categories,
                            group);
        });

    arcadeSettingsScreen_ = std::make_unique<ArcadeSettingsScreen>(
        *context_, [this] { openArcadeGames(); }, [this] { closeArcadeSettings(); });

    const GameSystem *wanted = options_.system.empty() ? nullptr
                                                       : library_.findSystem(options_.system);
    if (!wanted) wanted = systemsScreen_->current();
    if (wanted) gamesScreen_->showSystem(*wanted);

    selectTab(options_.tab);

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

    if (options_.readInput) {
        // A capture (--dump) is often taken next to a GUI that is already running. It must not
        // take the console from that one, nor hand it back on the way out.
        if (options_.dumpPath.empty()) console_.acquire();   // stop the kernel console drawing over us
        input_.setExclusive(options_.exclusive);
        input_.rescan();
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
    if (!gamesScreen_->empty()) gamesScreen_->reload();
    arcadeScreen_->refresh();

    // The Arcade system may have gone or changed, which would leave an open Arcade view, or
    // the copy of the system a group's games point into, dangling.
    arcadeGroupsActive_ = false;
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

void App::startSystemIconDownload() {
    if (!systemIconDownload_) return;
    if (systemIconDownload_->running()) {
        context_->notify("System icon download is already running");
        return;
    }

    if (!systemIconDownload_->start()) {
        context_->notify(systemIconDownload_->error(), 6.0f);
        return;
    }

    if (systemIconDownload_->state() == SystemIconDownload::State::Done) {
        context_->notify(systemIconDownload_->statusLine());
        return;
    }
    context_->notify("Downloading system icons", 4.0f);
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
    return !context_->errorActive && !scanActive_ && !visibilityActive_ &&
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

void App::openArcadeFull(ArcadeScreen::Dimension dimension) {
    const GameSystem *arcade = library_.arcadeSystem();
    if (!arcade) return;

    if (dimension == ArcadeScreen::Dimension::Games) {
        // Exactly what Systems -> Arcade shows: one list, reached from two places.
        gamesScreen_->showSystem(*arcade);
        detailActive_ = true;
    } else {
        arcadeGroupsScreen_->show(dimension == ArcadeScreen::Dimension::Manufacturers);
        arcadeGroupsActive_ = true;
    }
    needsFullRedraw_ = true;
}

void App::openArcadeGroup(ArcadeScreen::Dimension dimension, const DatabaseGroup &group) {
    const GameSystem *arcade = library_.arcadeSystem();
    if (!arcade) return;

    arcadeGroupSystem_ = *arcade;
    arcadeGroupSystem_.dbKey = group.key;

    const char *kind = dimension == ArcadeScreen::Dimension::Manufacturers ? "Manufacturers"
                                                                           : "Categories";
    gamesScreen_->showSystem(arcadeGroupSystem_,
                             std::string("Arcade > ") + kind + " > " +
                                 group.name);
    detailActive_ = true;
    needsFullRedraw_ = true;
}

void App::cycleDefaultView() {
    const GameView current = gameViewFromName(preferences_.defaultView());
    const GameView next = GameView(((int)current + 1) % (int)GameView::kCount);
    preferences_.setDefaultView(nameForGameView(next));

    // Applied immediately, not just on the next screen shown — a setting that visibly does
    // nothing until some later, unrelated action looks broken.
    gamesScreen_->setView(next);
    allGamesScreen_->setView(next);
    favoritesScreen_->setView(next);
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

Screen *App::activeScreen() {
    if (scanActive_) return scanScreen_.get();
    if (visibilityActive_) return visibilityScreen_.get();
    if (controllersActive_) return controllersScreen_.get();
    if (arcadeGamesActive_) return arcadeGamesScreen_.get();
    if (arcadeSettingsActive_) return arcadeSettingsScreen_.get();
    if (detailActive_) return gamesScreen_.get();
    if (arcadeGroupsActive_) return arcadeGroupsScreen_.get();

    switch (tab_) {
    case Tab::Home:      return homeScreen_.get();
    case Tab::Favorites: return favoritesScreen_.get();
    case Tab::Systems:   return systemsScreen_.get();
    case Tab::Arcade:    return arcadeScreen_.get();
    case Tab::Games:     return allGamesScreen_.get();
    case Tab::Settings:  return settingsScreen_.get();
    }
    return systemsScreen_.get();
}

void App::selectTab(Tab tab) {
    detailActive_ = false;
    arcadeGroupsActive_ = false;
    needsFullRedraw_ = true;
    if (tab == Tab::Home) homeScreen_->refresh();
    if (tab == Tab::Favorites) favoritesScreen_->showFavorites();
    if (tab == Tab::Arcade) arcadeScreen_->refresh();
    // The whole library is gathered when the tab is first opened and then kept; a library
    // reload is what throws it away again. Building the list itself is no longer the
    // expensive part — see GamesScreen::reload() — so there is no longer a mid-build state
    // to avoid restarting here.
    if (tab == Tab::Games && allGamesScreen_->empty()) allGamesScreen_->showAllGames();
    tab_ = tab;
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

void App::dispatch(Action action) {
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
        if (detailActive_) { detailActive_ = false; needsFullRedraw_ = true; return; }
        if (arcadeGroupsActive_) { arcadeGroupsActive_ = false; needsFullRedraw_ = true; return; }
        if (tab_ == Tab::Settings && settingsScreen_->wantsBack()) {
            settingsScreen_->handle(Action::Back);
            return;
        }
        // On the Arcade tab, Back first steps from a tile to its row's title.
        if (tab_ == Tab::Arcade && arcadeScreen_->wantsBack()) {
            arcadeScreen_->handle(Action::Back);
            return;
        }
        if (tab_ != Tab::Home) { selectTab(Tab::Home); return; }
        break;
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
    const std::string hints = activeScreen()->hints();
    const int y = area.y + (area.h - theme.regular().lineHeight(theme.sizeSmall())) / 2;

    if (favoriteHolding) {
        const bool removing = favorites_.contains(favoriteHoldPath_);
        const std::string label = removing ? "Hold X for 2s to remove favorite" :
                                             "Hold X for 2s to add favorite";
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
    } else {
        theme.regular().draw(canvas, area.x + theme.marginX(), y, hints, theme.sizeSmall(),
                             theme.textMuted.withAlpha(110));
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
                                         : theme.textMuted.withAlpha(90));

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
    int frame = 0;
    bool firstFrameLogged = false;

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
        else if (systemIconDownload_ && systemIconDownload_->running()) {
            systemIconDownload_->step();
            if (!systemIconDownload_->running()) {
                needsFullRedraw_ = true;
                if (systemIconDownload_->state() == SystemIconDownload::State::Done)
                    context_->notify(systemIconDownload_->statusLine(), 6.0f);
                else
                    context_->notify(systemIconDownload_->error(), 8.0f);
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

        Theme &theme = *theme_;
        topBar_.update(dt);
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
        // An error modal draws over whatever screen is underneath rather than replacing it,
        // so that screen must be fully repainted every frame it is up — there is no damage
        // tracking for "the same dialog is still sitting on top of you".
        const bool full = needsFullRedraw_ || !options_.incremental || context_->errorActive;

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
                       tabNavigationAvailable());
        renderBottomBar(bottom);
        const int64_t t2 = nowMs();

        screen->render(*canvas_, content, full);

        if (context_->errorActive)
            ErrorModal::draw(*canvas_, theme, canvas_->bounds(), context_->errorTitle,
                             context_->errorMessage);
        const int64_t t3 = nowMs();

        if (full) framebuffer_.present(*canvas_);
        else framebuffer_.present(*canvas_, canvas_->damage());
        needsFullRedraw_ = false;
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
