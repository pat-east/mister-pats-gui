#include "ScanScreen.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "Canvas.h"
#include "LoadingIndicator.h"

ScanScreen::ScanScreen(Context &context, LibraryScan &scan, MediaScraper &scraper,
                       std::function<void()> onFinished, std::function<void()> onClose,
                       std::function<void()> onOnboardingComplete)
    : context_(context), scan_(scan), scraper_(scraper), onFinished_(std::move(onFinished)),
      onClose_(std::move(onClose)),
      onOnboardingComplete_(std::move(onOnboardingComplete)) {}

void ScanScreen::reset(bool firstRun) {
    firstRun_ = firstRun;
    page_ = firstRun ? Page::Welcome : Page::Intro;
    jumpStartFlow_ = false;
    jumpStartNoGames_ = false;
    reported_ = false;
    scanRan_ = false;
    progressScroll_ = 0;
    roots_ = LibraryScan::detectRoots();
}

void ScanScreen::resetForArtwork() {
    firstRun_ = false;
    jumpStartFlow_ = false;
    jumpStartNoGames_ = false;
    page_ = Page::ArtworkOffer;
    reported_ = true;
    scanRan_ = false;
    progressScroll_ = 0;
    roots_ = LibraryScan::detectRoots();
}

void ScanScreen::handle(Action action) {
    switch (page_) {
    case Page::Welcome:
        if (action == Action::Confirm) page_ = Page::JumpStart;
        else if (action == Action::Back) {
            onOnboardingComplete_();
            onClose_();
        }
        return;

    case Page::JumpStart:
        if (action == Action::Confirm) {
            jumpStartFlow_ = true;
            scan_.startJumpStart(roots_, 20, 20);
            scanRan_ = true;
            page_ = Page::Working;
        } else if (action == Action::Back) {
            onOnboardingComplete_();
            onClose_();
        }
        return;

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
        if (jumpStartFlow_ && action == Action::Up && progressScroll_ > 0) --progressScroll_;
        else if (jumpStartFlow_ && action == Action::Down &&
                 progressScroll_ + visibleProgressRows_ < scan_.progressItems().size()) ++progressScroll_;
        else if (action == Action::Back) scan_.cancel();
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
        if (jumpStartFlow_ && action == Action::Up && progressScroll_ > 0) --progressScroll_;
        else if (jumpStartFlow_ && action == Action::Down &&
                 progressScroll_ + visibleProgressRows_ < scan_.progressItems().size()) ++progressScroll_;
        else if (action == Action::Back) scraper_.cancel();
        return;

    case Page::Overview:
        if (action == Action::Confirm) {
            onOnboardingComplete_();
            onClose_();
        }
        return;

    case Page::Done:
        if (jumpStartFlow_ && scan_.state() != LibraryScan::State::Done) {
            if (action == Action::Confirm) {
                scan_.startJumpStart(roots_, 20, 20);
                scanRan_ = true;
                reported_ = false;
                progressScroll_ = 0;
                page_ = Page::Working;
            } else if (action == Action::Back) {
                onOnboardingComplete_();
                onClose_();
            }
        } else if (action == Action::Confirm || action == Action::Back) onClose_();
        return;
    }
}

void ScanScreen::update(float /*deltaSeconds*/) {
    if (page_ == Page::Working && scan_.finished()) {
        if (!reported_) {
            reported_ = true;
            if (scan_.state() == LibraryScan::State::Done) onFinished_();
        }
        if (scan_.state() != LibraryScan::State::Done) {
            page_ = Page::Done;
        } else if (jumpStartFlow_) {
            jumpStartNoGames_ = scan_.gamesFound() == 0;
            if (jumpStartNoGames_) {
                page_ = Page::Overview;
            } else {
                MediaScraper::Options options;
                options.boxart = true;
                options.backgrounds = false;
                scraper_.start(context_.library.database(), options);
                page_ = Page::ArtworkWorking;
            }
        } else {
            // Artwork is only worth offering once there is a library to hang it on.
            page_ = Page::ArtworkOffer;
        }
        return;
    }

    if (page_ == Page::ArtworkWorking && scraper_.finished())
        page_ = jumpStartFlow_ ? Page::Overview : Page::Done;
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

void ScanScreen::renderWelcome(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(36));
    theme.bold().draw(canvas, body.x, body.y, "Welcome to MiSTer Pat's GUI",
                      theme.sizeTitle(), theme.textPrimary);

    int y = body.y + theme.px(72);
    drawParagraph(canvas, body,
                  {"Here is a quick guide to finding your way around.",
                   "",
                   "At the top, you find the main areas for browsing your games.",
                   "Bottom-left hints show what you can do on this screen and change with context.",
                   "Settings is where you read your library, configure controllers, customize",
                   "the GUI, and update cores."},
                  theme.sizeBody(), theme.textMuted, y);
}

void ScanScreen::renderJumpStart(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(36));
    theme.bold().draw(canvas, body.x, body.y, "Jump-start your GUI?",
                      theme.sizeTitle(), theme.textPrimary);

    int y = body.y + theme.px(72);
    drawParagraph(canvas, body,
                  {"Read up to 20 titles for each system on this device, including up to",
                   "20 working Arcade games, then prepare matching box art.",
                   "",
                   "Matching covers may be downloaded. Jump-start uses the internet for",
                   "artwork when available; it does not read your full library.",
                   "",
                   "Skip now and use Settings → Library to read the full library later."},
                  theme.sizeBody(), theme.textMuted, y);
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

void ScanScreen::renderJumpStartWorking(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(34));
    const bool preparingArt = page_ == Page::ArtworkWorking;
    const float progress = preparingArt ? scraper_.progress() : scan_.progress();
    const std::string status = preparingArt ? scraper_.statusLine() : scan_.statusLine();
    const std::string title = preparingArt ? "Preparing box art for your sample"
                                           : "Building your quick-start library";
    theme.bold().draw(canvas, body.x, body.y, title, theme.sizeHeading(), theme.textPrimary);

    char countText[128];
    if (preparingArt) {
        std::snprintf(countText, sizeof(countText), "%zu of %zu covers checked",
                      scraper_.fetched() + scraper_.prepared() + scraper_.skipped() + scraper_.missing(),
                      context_.library.database().totalGames());
    } else {
        std::snprintf(countText, sizeof(countText), "%zu titles found across %zu systems",
                      scan_.gamesFound(), scan_.systemsFound());
    }

    const int progressY = body.y + theme.px(58);
    theme.regular().draw(canvas, body.x, progressY, status, theme.sizeSmall(), theme.textMuted);
    const int barY = progressY + theme.px(30);
    const Rect bar{body.x, barY, body.w, theme.px(12)};
    canvas.fillRoundedRect(bar, theme.px(6), theme.surfaceHi);
    const int fill = int(float(bar.w) * std::max(0.0f, std::min(1.0f, progress)));
    if (fill > 0)
        canvas.fillRoundedRect({bar.x, bar.y, std::max(fill, theme.px(12)), bar.h},
                               theme.px(6), theme.accent);
    theme.regular().draw(canvas, body.x, barY + theme.px(24), countText,
                         theme.sizeSmall(), theme.textMuted);

    const auto &items = scan_.progressItems();
    const int headingY = barY + theme.px(64);
    theme.bold().draw(canvas, body.x, headingY, "SYSTEM", theme.sizeSmall(), theme.textMuted);
    const int listTop = headingY + theme.px(26);
    const int listBottom = body.bottom() - theme.px(20);
    const int rowHeight = theme.px(38);
    const int visible = std::max(1, (listBottom - listTop) / rowHeight);
    visibleProgressRows_ = size_t(visible);
    if (progressScroll_ + size_t(visible) > items.size())
        progressScroll_ = items.size() > size_t(visible) ? items.size() - size_t(visible) : 0;
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].active && (i < progressScroll_ || i >= progressScroll_ + size_t(visible)))
            progressScroll_ = i < progressScroll_ ? i : i - size_t(visible) + 1;
    }

    if (items.empty()) {
        theme.regular().draw(canvas, body.x, listTop + theme.px(22),
                             "Finding systems on connected storage…", theme.sizeBody(),
                             theme.textMuted);
    } else {
        const int textSize = theme.sizeSmall();
        const int statusWidth = theme.px(300);
        const Rect listClip{body.x, listTop, body.w - theme.px(14),
                            std::max(1, listBottom - listTop)};
        canvas.pushClip(listClip);
        const size_t end = std::min(items.size(), progressScroll_ + size_t(visible));
        for (size_t i = progressScroll_; i < end; ++i) {
            const int rowY = listTop + int(i - progressScroll_) * rowHeight;
            const Rect row{body.x, rowY, listClip.w, rowHeight - theme.px(2)};
            if (items[i].active) {
                canvas.fillRoundedRect(row, theme.px(6), theme.surfaceHi);
                canvas.strokeRoundedRect(row, theme.px(6), theme.px(2), theme.focus);
            }
            const int nameWidth = row.w - statusWidth - theme.px(20);
            theme.regular().draw(canvas, row.x + theme.px(10), row.y + theme.px(8),
                                 theme.regular().elide(items[i].name, textSize, nameWidth),
                                 textSize, items[i].active ? theme.textPrimary : theme.textMuted);
            const std::string statusText = theme.regular().elide(
                items[i].status, textSize, statusWidth - theme.px(10));
            theme.regular().draw(canvas, row.right() - statusWidth,
                                 row.y + theme.px(8), statusText, textSize,
                                 items[i].done ? theme.accent : theme.textMuted);
        }
        canvas.popClip();

        if (items.size() > size_t(visible)) {
            char range[64];
            std::snprintf(range, sizeof(range), "%zu–%zu of %zu", progressScroll_ + 1,
                          end, items.size());
            const int rangeWidth = theme.regular().measure(range, textSize);
            theme.regular().draw(canvas, body.right() - rangeWidth, headingY, range,
                                 textSize, theme.textMuted);
            const Rect track{body.right() - theme.px(4), listTop, theme.px(4),
                             listBottom - listTop};
            canvas.fillRoundedRect(track, theme.px(2), theme.surfaceHi);
            const int thumbHeight = std::max(theme.px(26),
                int(float(track.h) * float(visible) / float(items.size())));
            const int travel = track.h - thumbHeight;
            const int thumbY = track.y + (items.size() > size_t(visible)
                ? int(float(travel) * float(progressScroll_) /
                      float(items.size() - size_t(visible))) : 0);
            canvas.fillRoundedRect({track.x, thumbY, track.w, thumbHeight},
                                   theme.px(2), theme.accent);
        }
    }
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

void ScanScreen::renderOverview(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(36));
    const bool foundGames = !jumpStartNoGames_;
    theme.bold().draw(canvas, body.x, body.y,
                      foundGames ? "Your Jump-Start is ready" : "No games found yet",
                      theme.sizeHeading(), theme.textPrimary);

    int y = body.y + theme.px(52);
    if (foundGames) {
        drawParagraph(canvas, body,
                      {"The sample game list and matching box art are ready.",
                       "You can start playing now."},
                      theme.sizeBody(), theme.accent, y);
    } else {
        drawParagraph(canvas, body,
                      {"The scan did not find any games on the connected storage.",
                       "You can add your own games and read the full library later."},
                      theme.sizeBody(), theme.textMuted, y);
    }

    y += theme.px(16);
    theme.bold().draw(canvas, body.x, y, "It's dangerous to go alone! Take this.",
                      theme.sizeBody(), theme.textPrimary);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(4);
    theme.regular().draw(canvas, body.x, y, "— The Legend of Zelda", theme.sizeSmall(),
                         theme.textMuted);
    y += theme.regular().lineHeight(theme.sizeSmall()) + theme.px(18);

    theme.bold().draw(canvas, body.x, y, "When you're ready", theme.sizeBody(),
                      theme.textPrimary);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(8);
    drawParagraph(canvas, body,
                  {"Read your full library  ·  Settings → Library",
                   "Prepare box art for the full library  ·  Settings → Library",
                   "Configure your controller  ·  Settings → Controllers"},
                  theme.sizeSmall(), theme.textMuted, y);

    if (scraper_.state() == MediaScraper::State::Failed && !jumpStartNoGames_) {
        y += theme.px(8);
        theme.regular().draw(canvas, body.x, y,
                             "Some covers could not be prepared. You can try again later in Settings.",
                             theme.sizeSmall(), theme.textMuted);
    }
}

void ScanScreen::renderDone(Canvas &canvas, const Rect &panel) {
    Theme &theme = context_.theme;
    const Rect body = panel.inset(theme.px(44));
    int y = body.y + theme.px(96);

    // The title reflects whatever actually ran on *this* visit — not `scan_.state()`
    // unconditionally, which used to show a leftover or absent scan result ("Scan stopped")
    // on a visit that only fetched box art and never touched `scan_` at all.
    if (jumpStartFlow_ && scan_.state() != LibraryScan::State::Done) {
        theme.bold().draw(canvas, body.x, body.y, "The library could not be read",
                          theme.sizeTitle(), theme.textPrimary);
        drawParagraph(canvas, body,
                      {"No game list was saved. Check your storage and try again."},
                      theme.sizeBody(), theme.textMuted, y);
        y += theme.px(10);
        theme.regular().draw(canvas, body.x, y, scan_.error(), theme.sizeSmall(),
                             theme.textMuted.withAlpha(180));
    } else if (scanRan_) {
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

    const bool onboardingWide = (page_ == Page::Working && jumpStartFlow_) ||
                                (page_ == Page::ArtworkWorking && jumpStartFlow_) ||
                                page_ == Page::Overview;
    const int width = std::min(area.w, theme.px(onboardingWide ? 1120 : 1000));
    const int height = std::min(area.h, theme.px(onboardingWide ? 760 : 650));
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
    case Page::Welcome:         renderWelcome(canvas, panel); break;
    case Page::JumpStart:       renderJumpStart(canvas, panel); break;
    case Page::Intro:           renderIntro(canvas, panel); break;
    case Page::Working:
        if (jumpStartFlow_) renderJumpStartWorking(canvas, panel);
        else renderWorking(canvas, panel);
        break;
    case Page::ArtworkOffer:    renderArtworkOffer(canvas, panel); break;
    case Page::ArtworkWorking:
        if (jumpStartFlow_) renderJumpStartWorking(canvas, panel);
        else renderArtworkWorking(canvas, panel);
        break;
    case Page::Overview:        renderOverview(canvas, panel); break;
    case Page::Done:            renderDone(canvas, panel); break;
    }

    canvas.popClip();
}

std::string ScanScreen::hints() const {
    switch (page_) {
    case Page::Welcome:        return "A Continue   B Skip";
    case Page::JumpStart:      return "A Jump-Start   B Skip";
    case Page::Intro:          return "A Scan connected drives   B Do this later";
    case Page::Working:
        if (!jumpStartFlow_) return "B Cancel game scan";
        return scan_.progressItems().size() > visibleProgressRows_
            ? "{DPAD} Scroll   B Cancel" : "B Cancel";
    case Page::ArtworkOffer:   return "A Download missing covers   B Skip for now";
    case Page::ArtworkWorking:
        if (!jumpStartFlow_) return "B Stop cover download";
        return scan_.progressItems().size() > visibleProgressRows_
            ? "{DPAD} Scroll   B Cancel" : "B Cancel";
    case Page::Overview:       return "A Start using MiSTer Pat's GUI";
    case Page::Done:
        if (jumpStartFlow_ && scan_.state() != LibraryScan::State::Done)
            return "A Try again   B Skip";
        return "A Finish";
    default:                   return "A Finish";
    }
}
