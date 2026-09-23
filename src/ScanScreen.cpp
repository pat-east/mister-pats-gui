#include "ScanScreen.h"

#include <algorithm>
#include <cstdio>

#include "Canvas.h"
#include "LoadingIndicator.h"

ScanScreen::ScanScreen(Context &context, LibraryScan &scan, MediaScraper &scraper,
                       std::function<void()> onFinished, std::function<void()> onClose)
    : context_(context), scan_(scan), scraper_(scraper), onFinished_(std::move(onFinished)),
      onClose_(std::move(onClose)) {}

void ScanScreen::reset(bool firstRun) {
    firstRun_ = firstRun;
    page_ = Page::Intro;
    reported_ = false;
    scanRan_ = false;
    roots_ = LibraryScan::detectRoots();
}

void ScanScreen::resetForArtwork() {
    firstRun_ = false;
    page_ = Page::ArtworkOffer;
    reported_ = true;
    scanRan_ = false;
    roots_ = LibraryScan::detectRoots();
}

void ScanScreen::handle(Action action) {
    switch (page_) {
    case Page::Intro:
        if (action == Action::Confirm) {
            scan_.start(roots_);
            scanRan_ = true;
            page_ = Page::Working;
        } else if (action == Action::Back) {
            onClose_();
        }
        return;

    case Page::Working:
        if (action == Action::Back) scan_.cancel();
        return;

    case Page::ArtworkOffer:
        if (action == Action::Confirm) {
            MediaScraper::Options options;
            scraper_.start(context_.library.database(), options);
            page_ = Page::ArtworkWorking;
        } else if (action == Action::Back) {
            page_ = Page::Done;
        }
        return;

    case Page::ArtworkWorking:
        // Stopping is harmless: whatever arrived stays, and the next run fills the rest.
        if (action == Action::Back) scraper_.cancel();
        return;

    case Page::Done:
        if (action == Action::Confirm || action == Action::Back) onClose_();
        return;
    }
}

void ScanScreen::update(float /*deltaSeconds*/) {
    if (page_ == Page::Working && scan_.finished()) {
        if (!reported_) {
            reported_ = true;
            if (scan_.state() == LibraryScan::State::Done) onFinished_();
        }
        // Artwork is only worth offering once there is a library to hang it on.
        page_ = scan_.state() == LibraryScan::State::Done ? Page::ArtworkOffer : Page::Done;
        return;
    }

    if (page_ == Page::ArtworkWorking && scraper_.finished()) page_ = Page::Done;
}

void ScanScreen::drawParagraph(Canvas &canvas, const Rect &area,
                               const std::vector<std::string> &lines, int size, Color color,
                               int &y) const {
    Theme &theme = context_.theme;
    const int step = theme.regular().lineHeight(size) + theme.px(6);

    for (const std::string &line : lines) {
        theme.regular().draw(canvas, area.x, y, line, size, color);
        y += step;
    }
}

void ScanScreen::renderIntro(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(44));

    theme.bold().draw(canvas, body.x, body.y,
                      firstRun_ ? "Set up your game library" : "Rebuild the game database",
                      theme.sizeTitle(), theme.textPrimary);

    int y = body.y + theme.px(78);

    std::vector<std::string> text;
    if (firstRun_) {
        text = {
            "This GUI has no record of your games yet.",
            "",
            "It will look through the volumes below for game directories that",
            "match an installed core, and write one index file per system to",
            MISTER_PAT_ROOT "/gamesdb.",
        };
    } else {
        text = {
            "This reads your drives again and replaces the current index.",
            "",
            "Do it after adding or removing games.",
        };
    }
    drawParagraph(canvas, body, text, theme.sizeBody(), theme.textMuted, y);

    y += theme.px(16);
    theme.bold().draw(canvas, body.x, y, "Volumes", theme.sizeBody(), theme.textPrimary);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(10);

    if (roots_.empty()) {
        theme.regular().draw(canvas, body.x, y, "none found", theme.sizeBody(), theme.warning);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(6);
    } else {
        for (const std::string &root : roots_) {
            theme.regular().draw(canvas, body.x + theme.px(14), y, root, theme.sizeBody(),
                                 theme.accent);
            y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(6);
        }
    }

    y += theme.px(20);
    std::vector<std::string> footer = {
        "Nothing on your drives is changed — this only reads.",
        "It can take a few minutes with a large library.",
    };
    drawParagraph(canvas, body, footer, theme.sizeSmall(), theme.textMuted.withAlpha(170), y);
}

void ScanScreen::renderWorking(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    char counts[96];
    std::snprintf(counts, sizeof(counts), "%zu systems  ·  %zu games", scan_.systemsFound(),
                  scan_.gamesFound());
    LoadingIndicator::draw(canvas, theme, panel.inset(theme.px(44)), "Reading your library",
                           scan_.progress(), scan_.statusLine(), counts);
}

void ScanScreen::renderArtworkOffer(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(44));

    theme.bold().draw(canvas, body.x, body.y, "Fetch box art", theme.sizeTitle(),
                      theme.textPrimary);

    int y = body.y + theme.px(78);
    drawParagraph(canvas, body,
                  {"Covers and background images come from the libretro thumbnail",
                   "server. No account, no key.",
                   "",
                   "Each picture is shrunk and re-encoded before it is written, so the",
                   "drive only ever sees the finished file — about 45 KB instead of 300.",
                   "",
                   "This takes a while: roughly a second per game, and it needs the",
                   "network. Stopping is safe. Whatever arrived stays, and running it",
                   "again fills in the rest."},
                  theme.sizeBody(), theme.textMuted, y);

    y += theme.px(14);
    char scope[128];
    std::snprintf(scope, sizeof(scope), "%zu games in the library",
                  context_.library.database().totalGames());
    theme.regular().draw(canvas, body.x, y, scope, theme.sizeSmall(),
                         theme.textMuted.withAlpha(170));
}

void ScanScreen::renderArtworkWorking(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    char counts[128];
    std::snprintf(counts, sizeof(counts), "%zu fetched  ·  %zu already there  ·  %zu not found",
                  scraper_.fetched(), scraper_.skipped(), scraper_.missing());
    LoadingIndicator::draw(canvas, theme, panel.inset(theme.px(44)), "Fetching box art",
                           scraper_.progress(), scraper_.statusLine(), counts);
}

void ScanScreen::renderDone(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(44));
    int y = body.y + theme.px(96);

    // The title reflects whatever actually ran on *this* visit — not `scan_.state()`
    // unconditionally, which used to show a leftover or absent scan result ("Scan stopped")
    // on a visit that only fetched box art and never touched `scan_` at all.
    if (scanRan_) {
        const bool ok = scan_.state() == LibraryScan::State::Done;
        theme.bold().draw(canvas, body.x, body.y, ok ? "Library ready" : "Scan stopped",
                          theme.sizeTitle(), ok ? theme.textPrimary : theme.warning);

        if (ok) {
            char summary[128];
            std::snprintf(summary, sizeof(summary), "%zu systems, %zu games indexed.",
                          scan_.systemsFound(), scan_.gamesFound());
            drawParagraph(canvas, body, {summary, "",
                                         "The index lives beside the application, so the drives",
                                         "are not touched again until you rebuild it."},
                          theme.sizeBody(), theme.textMuted, y);
        } else {
            drawParagraph(canvas, body, {scan_.error(), "",
                                         "The database may be incomplete. Run it again from the",
                                         "settings when you are ready."},
                          theme.sizeBody(), theme.textMuted, y);
        }
    } else if (scraper_.state() != MediaScraper::State::Idle) {
        const bool ok = scraper_.state() == MediaScraper::State::Done;
        theme.bold().draw(canvas, body.x, body.y, ok ? "Box art fetched" : "Box art fetch stopped",
                          theme.sizeTitle(), ok ? theme.textPrimary : theme.warning);

        if (!ok) {
            drawParagraph(canvas, body, {scraper_.error(), ""}, theme.sizeBody(),
                          theme.textMuted, y);
        }
    }

    if (scraper_.state() != MediaScraper::State::Idle) {
        char art[160];
        std::snprintf(art, sizeof(art), "%zu fetched, %zu already there, %zu missing.",
                      scraper_.fetched(), scraper_.skipped(), scraper_.missing());
        y += theme.px(10);

        std::vector<std::string> lines = {art};
        if (scraper_.missing())
            lines.push_back(std::string("Titles nothing was found for are listed in ") +
                            MediaScraper::kMissesFile);
        drawParagraph(canvas, body, lines, theme.sizeSmall(), theme.textMuted.withAlpha(180), y);
    }
}

void ScanScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int width = std::min(area.w, theme.px(900));
    const int height = std::min(area.h, theme.px(600));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2, width,
                     height};

    canvas.dropShadow(panel, theme.px(10), theme.px(26), theme.shadow.withAlpha(180));
    canvas.fillRoundedRect(panel, theme.radius(), theme.surface);
    const bool busy = page_ == Page::Working || page_ == Page::ArtworkWorking;
    canvas.strokeRoundedRect(panel, theme.radius(), theme.px(2),
                             theme.accent.withAlpha(busy ? 220 : 90));

    // Clip to the panel: a long list of volumes must be cut off at the edge rather than
    // drawn over the screen behind it.
    canvas.pushClip(panel);

    switch (page_) {
    case Page::Intro:           renderIntro(canvas, panel); break;
    case Page::Working:         renderWorking(canvas, panel); break;
    case Page::ArtworkOffer:    renderArtworkOffer(canvas, panel); break;
    case Page::ArtworkWorking:  renderArtworkWorking(canvas, panel); break;
    case Page::Done:            renderDone(canvas, panel); break;
    }

    canvas.popClip();
}

std::string ScanScreen::hints() const {
    switch (page_) {
    case Page::Intro:          return "A Start   B Later";
    case Page::Working:        return "B Cancel";
    case Page::ArtworkOffer:   return "A Fetch box art   B Skip";
    case Page::ArtworkWorking: return "B Stop";
    case Page::Done:
    default:                   return "A Continue";
    }
}
