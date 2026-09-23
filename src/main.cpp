#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

#include "App.h"
#include "LibraryScan.h"

namespace {

App *g_app = nullptr;

void onSignal(int) {
    if (g_app) g_app->stop();
}

void onScreenshot(int) {
    if (g_app) g_app->requestScreenshot();
}

void usage() {
    std::printf(
        "mister-gui [options]\n"
        "  --tab home|favorites|systems|games|settings  screen to open\n"
        "  --view list|large|grid|small|compact     game presentation\n"
        "  --system NAME                            preselect a system\n"
        "  --frames N                               render N frames, then exit\n"
        "  --dump PATH                              write the finished canvas to PATH\n"
        "  --launch-now                             start the preselected game at once\n"
        "  --dry-run                                print the MGL instead of loading it\n"
        "  --no-wizard                              never open the database wizard\n"
        "  --scan                                   build the game database and exit\n"
        "  --no-input                               do not open the input devices\n"
        "  --exclusive                              grab inputs (blocks the MiSTer OSD)\n"
        "  --full-redraw                            repaint everything every frame\n"
        "  --no-splash                              skip the startup splash delay\n"
        "  --splash-ms N                            splash delay in ms (default 2500)\n");
}

bool parse(int argc, char **argv, App::Options &options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;

        if (arg == "--help" || arg == "-h") { usage(); return false; }

        if (arg == "--tab" && hasValue) {
            const std::string value = argv[++i];
            if (value == "home") options.tab = Tab::Home;
            else if (value == "favorites") options.tab = Tab::Favorites;
            else if (value == "systems") options.tab = Tab::Systems;
            else if (value == "games") options.tab = Tab::Games;
            else if (value == "settings") options.tab = Tab::Settings;
        } else if (arg == "--view" && hasValue) {
            const std::string value = argv[++i];
            if (value == "list") options.view = GameView::List;
            else if (value == "large") options.view = GameView::BoxartLarge;
            else if (value == "small") options.view = GameView::BoxartSmall;
            else if (value == "grid") options.view = GameView::Grid;
            else if (value == "compact") options.view = GameView::Compact;
        } else if (arg == "--system" && hasValue) {
            options.system = argv[++i];
        } else if (arg == "--frames" && hasValue) {
            options.exitAfterFrames = std::atoi(argv[++i]);
        } else if (arg == "--dump" && hasValue) {
            options.dumpPath = argv[++i];
        } else if (arg == "--launch-now") {
            options.launchNow = true;
        } else if (arg == "--dry-run") {
            options.dryRun = true;
        } else if (arg == "--no-wizard") {
            options.wizard = false;
        } else if (arg == "--scan") {
            options.scanOnly = true;
        } else if (arg == "--full-redraw") {
            options.incremental = false;
        } else if (arg == "--stats") {
            options.stats = true;
        } else if (arg == "--no-grab" || arg == "--no-input") {
            options.readInput = false;
        } else if (arg == "--exclusive") {
            options.exclusive = true;
        } else if (arg == "--no-splash") {
            options.splashMs = 0;
        } else if (arg == "--splash-ms" && hasValue) {
            options.splashMs = std::atoi(argv[++i]);
        }
    }
    return true;
}

} // namespace

// Builds the database without opening the framebuffer, so it can be run over SSH while the
// television shows something else — which is also the only way to check a scan on a device
// that has no controller attached.
int runScanOnly() {
    LibraryScan scan;
    scan.start();

    std::string last;
    while (!scan.finished()) {
        scan.step();
        const std::string line = scan.statusLine();
        if (line != last) { std::printf("%s\n", line.c_str()); last = line; }
    }

    if (scan.state() != LibraryScan::State::Done) {
        std::fprintf(stderr, "scan failed: %s\n", scan.error().c_str());
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    App::Options options;
    options.splashMs = 2500;   // real boots show the splash unless told not to
    if (!parse(argc, argv, options)) return 0;

    if (options.scanOnly) return runScanOnly();

    App app;
    g_app = &app;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGHUP, SIG_IGN);    // survive the controlling terminal going away
    std::signal(SIGTTOU, SIG_IGN);   // writing to a console we do not own
    std::signal(SIGTTIN, SIG_IGN);
    std::signal(SIGUSR1, onScreenshot);   // photograph the running interface

    if (!app.initialize(options)) {
        std::fprintf(stderr, "could not initialise, giving up\n");
        return 1;
    }

    return app.run();
}
