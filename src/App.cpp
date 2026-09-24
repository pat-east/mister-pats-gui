#include "App.h"
#include "DebugLog.h"
#include "ErrorModal.h"
#include "Splash.h"
#include "Version.h"

#include <algorithm>
#include <cstdio>

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
    std::printf("framebuffer: %s\n", framebuffer_.describe().c_str());
    DebugLog::info("framebuffer: " + framebuffer_.describe());

    canvas_ = std::make_unique<Canvas>(framebuffer_.width(), framebuffer_.height());
    background_ = std::make_unique<Canvas>(framebuffer_.width(), framebuffer_.height());
    theme_ = std::make_unique<Theme>(framebuffer_.width(), framebuffer_.height());

    // Nothing here has touched a game drive yet — everything below this point does. See
    // Splash.h for why this exists.
    if (options_.splashMs > 0) Splash::show(framebuffer_, *theme_, options_.splashMs);

    renderBackground(*background_);

    library_.load();
    favorites_.load();
    icons_.load();
    history_.load();
    hiddenSystems_.load();
    preferences_.load();

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
        *context_, [this] { reloadLibrary(); }, [this] { stop(); },
        [this] { openScan(false); }, [this] { openArtwork(); },
        [this] { openVisibility(); }, [this] { toggleGamesTab(); },
        [this] { cycleDefaultView(); });
    settingsScreen_->setFramebufferInfo(framebuffer_.describe());

    scanScreen_ = std::make_unique<ScanScreen>(
        *context_, scan_, scraper_, [this] { reloadLibrary(); }, [this] { closeScan(); });

    visibilityScreen_ =
        std::make_unique<SystemVisibilityScreen>(*context_, [this] { closeVisibility(); });

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

    if (options_.readInput) {
        console_.acquire();   // stop the kernel console drawing over us
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

void App::closeScan() {
    // Artwork that arrived while the wizard ran is on disk but not in the image cache yet.
    if (scraper_.fetched()) reloadLibrary();

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

void App::toggleGamesTab() {
    preferences_.setShowGamesTab(!preferences_.showGamesTab());

    // Turning it off while it is the current tab would otherwise leave the top bar
    // highlighting a tab that no longer exists.
    if (!preferences_.showGamesTab() && tab_ == Tab::Games) selectTab(Tab::Home);
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

Screen *App::activeScreen() {
    if (scanActive_) return scanScreen_.get();
    if (visibilityActive_) return visibilityScreen_.get();
    if (detailActive_) return gamesScreen_.get();

    switch (tab_) {
    case Tab::Home:      return homeScreen_.get();
    case Tab::Favorites: return favoritesScreen_.get();
    case Tab::Systems:   return systemsScreen_.get();
    case Tab::Games:     return allGamesScreen_.get();
    case Tab::Settings:  return settingsScreen_.get();
    }
    return systemsScreen_.get();
}

void App::selectTab(Tab tab) {
    detailActive_ = false;
    needsFullRedraw_ = true;
    if (tab == Tab::Home) homeScreen_->refresh();
    if (tab == Tab::Favorites) favoritesScreen_->showFavorites();
    // The whole library is gathered when the tab is first opened and then kept; a library
    // reload is what throws it away again. Building the list itself is no longer the
    // expensive part — see GamesScreen::reload() — so there is no longer a mid-build state
    // to avoid restarting here.
    if (tab == Tab::Games && allGamesScreen_->empty()) allGamesScreen_->showAllGames();
    tab_ = tab;
}

void App::dispatch(Action action) {
    const std::vector<Tab> tabs = TopBar::visibleTabs(preferences_.showGamesTab());
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

    const std::string hints = activeScreen()->hints();
    const int y = area.y + (area.h - theme.regular().lineHeight(theme.sizeSmall())) / 2;

    theme.regular().draw(canvas, area.x + theme.marginX(), y, hints, theme.sizeSmall(),
                         theme.textMuted.withAlpha(110));

    // Small and out of the way in the corner — a build identifier for bug reports, not
    // something meant to draw the eye. A newer release found on GitHub (see UpdateCheck,
    // Settings → Check for updates) rides along in the one place a version number already
    // draws attention, rather than a notification competing with everything else.
    std::string version = std::string("v") + kAppVersion;
    if (updateCheck_.available()) version += "  ·  v" + updateCheck_.latestVersion() + " available";
    const int versionWidth = theme.regular().measure(version, theme.sizeSmall());
    theme.regular().draw(canvas, area.right() - theme.marginX() - versionWidth, y, version,
                         theme.sizeSmall(),
                         updateCheck_.available() ? theme.accent.withAlpha(200)
                                                   : theme.textMuted.withAlpha(90));

    if (context_->messageTimer > 0.0f && !context_->message.empty()) {
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
    int64_t previous = nowMs();
    int64_t rescanAt = previous + 1000;
    int frame = 0;

    while (running_) {
        const int64_t frameStart = nowMs();
        const float dt = float(frameStart - previous) / 1000.0f;
        previous = frameStart;

        if (options_.readInput && frameStart >= rescanAt) {
            input_.rescan();
            rescanAt = frameStart + 1000;
        }

        for (Action action : input_.poll(kTargetFrameMs)) dispatch(action);
        if (!running_) break;

        // A core was started: hand the devices and the console straight back.
        if (context_->standDown) {
            input_.releaseAll();
            console_.release();
            std::printf("app: standing down, a core was started\n");
            break;
        }

        // One system per frame while scanning, one game per frame while fetching artwork.
        // Pacing the work this way is what keeps the interface alive and stops the drive
        // being hammered flat out.
        if (scan_.running()) scan_.step();
        else if (scraper_.running()) scraper_.step();

        Theme &theme = *theme_;
        topBar_.update(dt);
        activeScreen()->update(dt);
        if (context_->messageTimer > 0.0f) context_->messageTimer -= dt;

        // A quiet moment well after boot, not the frame loop's problem to pace — this either
        // does nothing (off by default) or blocks once, briefly, bounded by UpdateCheck's own
        // short timeout. Never runs a second time in the same session.
        if (preferences_.checkForUpdates() && !updateCheck_.checked()) {
            updateCheckDelay_ -= dt;
            if (updateCheckDelay_ <= 0.0f) updateCheck_.run(kAppVersion);
        }

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
        topBar_.render(*canvas_, theme, top, tab_, preferences_.showGamesTab());
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

    console_.release();
    std::printf("app: stopped\n");
    return 0;
}
