#include "ScanScreen.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

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
            "The game list has not been created yet.",
            "",
            "MiSTer will look through connected drives for games it can run.",
            "It saves a list so menus open faster next time.",
        };
    } else {
        text = {
            "Search the connected drives again and replace the saved game list.",
            "",
            "Do this after adding or removing games.",
        };
    }
    drawParagraph(canvas, body, text, theme.sizeBody(), theme.textMuted, y);

    y += theme.px(16);
    theme.bold().draw(canvas, body.x, y, "Drives to search", theme.sizeBody(), theme.textPrimary);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(10);

    if (roots_.empty()) {
        theme.regular().draw(canvas, body.x, y,
                             "No connected drives found. Connect a game drive and try again.",
                             theme.sizeBody(), theme.warning);
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
        "A large collection can take a few minutes to scan.",
    };
    drawParagraph(canvas, body, footer, theme.sizeSmall(), theme.textMuted.withAlpha(170), y);
}

void ScanScreen::renderWorking(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    char counts[96];
    std::snprintf(counts, sizeof(counts), "%zu systems found  ·  %zu games found", scan_.systemsFound(),
                  scan_.gamesFound());
    LoadingIndicator::draw(canvas, theme, panel.inset(theme.px(44)), "Searching your drives for games",
                           scan_.progress(), scan_.statusLine(), counts);
}

void ScanScreen::renderArtworkOffer(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(44));

    theme.bold().draw(canvas, body.x, body.y, "Prepare box art", theme.sizeTitle(),
                      theme.textPrimary);

    int y = body.y + theme.px(78);
    drawParagraph(canvas, body,
                  {"Download missing covers from the libretro artwork library. Existing",
                   "covers on your drives are prepared too. No account is needed.",
                   "",
                   "The GUI saves cover sizes for each game view so pictures open",
                   "quickly. Original covers are kept.",
                   "",
                   "An Internet connection is needed for missing covers. Existing",
                   "covers can be prepared offline. You can stop and continue later."},
                  theme.sizeBody(), theme.textMuted, y);

    y += theme.px(14);
    char scope[128];
    std::snprintf(scope, sizeof(scope), "Covers checked for %zu games",
                  context_.library.database().totalGames());
    theme.regular().draw(canvas, body.x, y, scope, theme.sizeSmall(),
                         theme.textMuted.withAlpha(170));
}

void ScanScreen::renderArtworkWorking(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    char counts[128];
    std::snprintf(counts, sizeof(counts),
                  "Covers downloaded %zu  ·  Prepared %zu  ·  Already ready %zu  ·  Not found %zu",
                  scraper_.fetched(), scraper_.prepared(), scraper_.skipped(),
                  scraper_.missing());
    char eta[64];
    const int64_t remaining = scraper_.estimatedRemainingSeconds();
    if (remaining < 0) {
        std::snprintf(eta, sizeof(eta), "Finish time calculating…");
    } else {
        const std::time_t finish = std::time(nullptr) + remaining;
        std::tm local{};
        char clock[32]{};
        if (localtime_r(&finish, &local) &&
            std::strftime(clock, sizeof(clock), "%H:%M %Z", &local)) {
            std::snprintf(eta, sizeof(eta), "Finishes around %s", clock);
        } else {
            std::snprintf(eta, sizeof(eta), "Finish time unavailable");
        }
    }
    LoadingIndicator::draw(canvas, theme, panel.inset(theme.px(44)), "Downloading and preparing covers",
                           scraper_.progress(), scraper_.statusLine(), counts, eta);
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
            std::snprintf(summary, sizeof(summary), "%zu systems and %zu games added to the game list.",
                          scan_.systemsFound(), scan_.gamesFound());
            drawParagraph(canvas, body, {summary, "",
                                         "The saved list is kept on the MiSTer. Your game files",
                                         "are not changed."},
                          theme.sizeBody(), theme.textMuted, y);
        } else {
            drawParagraph(canvas, body, {scan_.error(), "",
                                         "If you stopped the scan, no new game list was saved.",
                                         "You can start another scan in Settings → Library."},
                          theme.sizeBody(), theme.textMuted, y);
        }
    } else if (scraper_.state() != MediaScraper::State::Idle) {
        const bool ok = scraper_.state() == MediaScraper::State::Done;
        theme.bold().draw(canvas, body.x, body.y, ok ? "Cover art ready" : "Cover search stopped",
                          theme.sizeTitle(), ok ? theme.textPrimary : theme.warning);

        if (!ok) {
            drawParagraph(canvas, body, {scraper_.error(), ""}, theme.sizeBody(),
                          theme.textMuted, y);
        }
    }

    if (scraper_.state() != MediaScraper::State::Idle) {
        char art[160];
        std::snprintf(art, sizeof(art),
                      "Downloaded %zu covers, prepared %zu, %zu already ready, %zu not found.",
                      scraper_.fetched(), scraper_.prepared(), scraper_.skipped(),
                      scraper_.missing());
        y += theme.px(10);

        std::vector<std::string> lines = {art};
        if (scraper_.missing())
            lines.push_back(std::string("Games with no matching cover are listed in ") +
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
    case Page::Intro:          return "A Scan connected drives   B Do this later";
    case Page::Working:        return "B Cancel game scan";
    case Page::ArtworkOffer:   return "A Download missing covers   B Skip for now";
    case Page::ArtworkWorking: return "B Stop cover download";
    case Page::Done:
    default:                   return "A Finish";
    }
}
