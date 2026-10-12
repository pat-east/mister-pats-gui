#include "DiscScreen.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include "Canvas.h"
#include "DebugLog.h"

namespace {

std::vector<std::string> wrapText(Font &font, const std::string &text, int size, int maxWidth) {
    std::istringstream words(text);
    std::vector<std::string> lines;
    std::string word;
    std::string line;
    while (words >> word) {
        if (font.measure(word, size) > maxWidth) word = font.elide(word, size, maxWidth);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && font.measure(candidate, size) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else line = candidate;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void drawRoundedGradient(Canvas &canvas, const Rect &rect, int radius, Color top, Color bottom) {
    if (rect.empty()) return;
    const int r = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
    for (int y = rect.y; y < rect.bottom(); ++y) {
        int inset = 0;
        const int fromTop = y - rect.y;
        const int fromBottom = rect.bottom() - 1 - y;
        const int edgeDepth = std::min(fromTop, fromBottom);
        if (edgeDepth < r) {
            const float dy = float(r - edgeDepth) - 0.5f;
            inset = std::max(0, int(std::ceil(float(r) -
                std::sqrt(std::max(0.0f, float(r * r) - dy * dy)))));
        }
        const int width = rect.w - inset * 2;
        if (width <= 0) continue;
        const float t = rect.h > 1 ? float(y - rect.y) / float(rect.h - 1) : 0.0f;
        canvas.fillRect({rect.x + inset, y, width, 1}, Color::lerp(top, bottom, t));
    }
}

} // namespace

DiscScreen::DiscScreen(Context &context, DiscCoreManager &cores,
                       std::function<void(const std::string &)> launch)
    : context_(context), cores_(cores), launch_(std::move(launch)) {
    coreStatus_ = cores_.snapshot();
}

void DiscScreen::update(float deltaSeconds) {
    (void)deltaSeconds;
    const auto disc = context_.discDrive.snapshot();
    if (!hasDiscGeneration_ || viewedDiscGeneration_ != disc.generation) {
        viewedDiscGeneration_ = disc.generation;
        hasDiscGeneration_ = true;
        cursor_ = 0;
        overlayIndex_ = 0;
        screenshotOverlay_ = false;
    }
    context_.discArtwork.request(disc);
    coreStatus_ = cores_.snapshot();
    artStatus_ = context_.discArtwork.snapshot();

    if (!launchProbePending_) return;
    if (std::chrono::steady_clock::now() - launchProbeStarted_ > std::chrono::seconds(15)) {
        launchProbePending_ = false;
        DebugLog::warn("disc launch: pre-launch recheck timed out");
        context_.notify("Disc/core recheck timed out. Press Start game to try again.", 5.0f);
        return;
    }
    if (context_.discInstaller.busy()) {
        launchProbePending_ = false;
        DebugLog::warn("disc launch: pre-launch recheck cancelled because package update started");
        context_.notify("Physical Disc package update started. Press Start game when it finishes.", 5.0f);
        return;
    }
    if (disc.state == DiscDriveService::MediaState::Checking ||
        coreStatus_.inspectionRevision < launchCoreRevision_) return;
    launchProbePending_ = false;
    if (disc.generation != launchProbeGeneration_) {
        DebugLog::warn("disc launch: media generation changed during pre-launch recheck");
        context_.notify("Disc changed during the check. Press Start game again.", 5.0f);
        return;
    }
    std::string mgl;
    if (disc.system == DiscDriveService::System::PlayStation) mgl = coreStatus_.psxMgl;
    if (disc.system == DiscDriveService::System::Saturn) mgl = coreStatus_.saturnMgl;
    if (disc.state == DiscDriveService::MediaState::Readable &&
        coreStatus_.readyFor(disc.system) && !mgl.empty() && launch_) {
        DebugLog::info(std::string("disc launch: preconditions passed for ") +
                       DiscDriveService::systemName(disc.system));
        launch_(mgl);
    } else {
        DebugLog::warn(std::string("disc launch: preconditions failed; media-state=") +
                       DiscDriveService::stateName(disc.state) +
                       "; core-ready=" + (coreStatus_.readyFor(disc.system) ? "yes" : "no"));
        context_.notify("Disc or Physical Disc core is no longer ready. Check the status and retry.", 6.0f);
    }
}

void DiscScreen::handle(Action action) {
    if (screenshotOverlay_) {
        if (action == Action::Back || action == Action::Confirm) screenshotOverlay_ = false;
        else if (action == Action::Left && overlayIndex_ > 0) --overlayIndex_;
        else if (action == Action::Right && overlayIndex_ + 1 < int(artStatus_.screenshots.size())) ++overlayIndex_;
        return;
    }
    // CycleView is the physical top face button; the hint formatter adapts its label to the
    // selected Xbox, PlayStation, or Nintendo face-button style.
    if (action == Action::CycleView) {
        const auto disc = context_.discDrive.snapshot();
        if (disc.state != DiscDriveService::MediaState::Readable ||
            disc.system == DiscDriveService::System::Unknown) {
            context_.notify("Insert a supported disc before reloading artwork.", 4.0f);
            return;
        }
        if (!artStatus_.boxart.empty()) {
            const size_t slash = artStatus_.boxart.find_last_of('/');
            const std::string directory = slash == std::string::npos
                ? std::string() : artStatus_.boxart.substr(0, slash + 1);
            std::vector<std::string> paths{artStatus_.boxart};
            for (int i = 1; i <= 4; ++i)
                paths.push_back(directory + "screenshot-" + std::to_string(i) + ".jpg");
            context_.images.invalidateCandidates(paths);
        }
        context_.discArtwork.reload(disc);
        context_.notify("Clearing cached artwork and reloading…", 5.0f);
        return;
    }
    if (action == Action::Left && cursor_ > 1) --cursor_;
    else if (action == Action::Right && cursor_ > 0 &&
             cursor_ < int(artStatus_.screenshots.size())) ++cursor_;
    else if (action == Action::Down && cursor_ == 0 && !artStatus_.screenshots.empty()) cursor_ = 1;
    else if (action == Action::Up && cursor_ > 0) cursor_ = 0;
    else if (action == Action::Confirm) {
        if (cursor_ > 0 && cursor_ <= int(artStatus_.screenshots.size())) {
            overlayIndex_ = cursor_ - 1;
            screenshotOverlay_ = true;
            return;
        }
        const auto disc = context_.discDrive.snapshot();
        if (disc.state == DiscDriveService::MediaState::Checking) {
            context_.notify("Reading disc. Wait for the check to finish.", 4.0f);
            return;
        }
        if (disc.state == DiscDriveService::MediaState::Readable &&
            context_.discInstaller.busy()) {
            context_.notify("Wait for the Physical Disc package update to finish.", 5.0f);
            return;
        }
        std::string mgl;
        if (disc.system == DiscDriveService::System::PlayStation) mgl = coreStatus_.psxMgl;
        if (disc.system == DiscDriveService::System::Saturn) mgl = coreStatus_.saturnMgl;
        if (disc.state == DiscDriveService::MediaState::Readable &&
            coreStatus_.readyFor(disc.system) && !mgl.empty() && launch_) {
            if (launchProbePending_) {
                context_.notify("Checking disc and core before launch…", 4.0f);
                return;
            }
            launchProbePending_ = true;
            launchProbeGeneration_ = disc.generation;
            launchCoreRevision_ = cores_.requestInspection();
            launchProbeStarted_ = std::chrono::steady_clock::now();
            context_.discDrive.requestProbe();
            DebugLog::info(std::string("disc launch: fresh disc/core recheck started for ") +
                           DiscDriveService::systemName(disc.system));
            context_.notify("Rechecking disc and Physical Disc core…", 4.0f);
        }
        else if (disc.state != DiscDriveService::MediaState::Readable ||
                 disc.system == DiscDriveService::System::Unknown)
            context_.notify("Insert a supported PlayStation or Saturn disc.", 4.0f);
        else if (!coreStatus_.readyFor(disc.system)) {
            const bool missingRbf = disc.system == DiscDriveService::System::PlayStation
                ? !coreStatus_.psxCoreInstalled
                : disc.system == DiscDriveService::System::Saturn && !coreStatus_.saturnCoreInstalled;
            context_.notify(missingRbf
                ? "Install the matching PlayStation or Saturn core from the Downloader."
                : "Install the scoped Physical Disc package in Settings.", 6.0f);
        } else context_.notify("The Physical Disc MGL is unavailable.", 5.0f);
    }
}

std::string DiscScreen::hints() const {
    const auto disc = context_.discDrive.snapshot();
    if (disc.state == DiscDriveService::MediaState::NoDrive) return "No optical drive detected";
    if (disc.state == DiscDriveService::MediaState::NoDisc) return "Insert disc";
    if (screenshotOverlay_) return "←/→ Browse screenshots   A/B Close";
    if (cursor_ > 0) return "A View screenshot   ↑ Start game   ←/→ Choose   Y Reload artwork";
    return "A Start game   ↓ View screenshots   Y Reload artwork";
}

void DiscScreen::render(Canvas &canvas, const Rect &area, bool) {
    Theme &theme = context_.theme;
    theme.bold().draw(canvas, area.x, area.y, "Disc Drive", theme.sizeHeading(), theme.textPrimary);
    const auto disc = context_.discDrive.snapshot();
    if (disc.state == DiscDriveService::MediaState::NoDrive ||
        disc.state == DiscDriveService::MediaState::NoDisc) {
        const int centerY = area.y + area.h / 2;
        const Rect headingArea{area.x, centerY - theme.px(56), area.w, theme.px(58)};
        const Rect detailArea{area.x, centerY + theme.px(4), area.w, theme.px(34)};
        const bool noDrive = disc.state == DiscDriveService::MediaState::NoDrive;
        theme.bold().drawCentered(canvas, headingArea,
            noDrive ? "No optical drive detected" : "Please insert disc",
            theme.sizeTitle(), theme.textPrimary);
        theme.regular().drawCentered(canvas, detailArea,
            noDrive ? "Connect an optical drive to use Disc Drive."
                    : "Insert a PlayStation or Sega Saturn disc.",
            theme.sizeBody(), theme.textMuted);
        return;
    }
    const int gap = theme.px(38);
    const int top = area.y + theme.px(62);
    const int columnW = std::max(0, (area.w - gap) / 2);
    const Rect artBox{area.x, top, columnW, area.bottom() - top - theme.px(8)};
    const int rightX = artBox.right() + gap;
    const int rightW = std::max(0, area.right() - rightX);
    const bool hasDisc = disc.state != DiscDriveService::MediaState::NoDrive &&
                         disc.state != DiscDriveService::MediaState::NoDisc;
    const int rowBottom = area.bottom() - theme.px(8);
    const int shotGap = theme.px(12);
    const int thumbW = std::max(1, (rightW - shotGap * 3) / 4);
    const int thumbH = thumbW * 9 / 16;
    const int thumbsY = rowBottom - thumbH;
    const int buttonY = thumbsY - theme.px(124);
    const int detailsBottom = buttonY - theme.px(20);

    ImagePtr boxart;
    if (!artStatus_.boxart.empty())
        boxart = context_.images.requestAsync(artStatus_.boxart, std::string(), artBox.w, artBox.h, true);
    if (boxart) {
        const int x = artBox.x + (artBox.w - boxart->width()) / 2;
        const int y = artBox.bottom() - boxart->height();
        canvas.drawImage(*boxart, {x, y, boxart->width(), boxart->height()}, 255, theme.px(8));
    } else {
        canvas.fillRoundedRect(artBox, theme.px(8), theme.surface);
        canvas.strokeRoundedRect(artBox, theme.px(8), std::max(1, theme.px(2)), theme.surfaceHi);
        const bool imagePending = !artStatus_.boxart.empty() &&
            context_.images.isPendingCandidates({artStatus_.boxart}, artBox.w, artBox.h);
        const bool loading = artStatus_.state == DiscArtworkService::State::Searching ||
            (disc.state == DiscDriveService::MediaState::Readable &&
             artStatus_.state == DiscArtworkService::State::Idle) || imagePending;
        const Rect statusArea{artBox.x + theme.px(16), artBox.y + theme.px(16),
                              artBox.w - theme.px(32), artBox.h - theme.px(32)};
        theme.regular().drawCentered(canvas, statusArea,
            loading ? "Boxart loading…" : "No Boxart found",
            theme.sizeBody(), theme.textMuted);
    }

    std::string heading;
    Color tone = theme.textPrimary;
    switch (disc.state) {
    case DiscDriveService::MediaState::NoDrive: heading = "No optical drive detected"; tone = theme.textMuted; break;
    case DiscDriveService::MediaState::NoDisc: heading = "Please insert disc"; tone = theme.textMuted; break;
    case DiscDriveService::MediaState::Checking: heading = "Reading disc…"; break;
    case DiscDriveService::MediaState::Readable:
        if (!disc.title.empty()) heading = disc.title;
        else if (disc.system == DiscDriveService::System::Saturn) heading = "Sega Saturn disc detected";
        else if (disc.system == DiscDriveService::System::PlayStation) heading = "PlayStation disc detected";
        else heading = "Supported disc detected";
        break;
    case DiscDriveService::MediaState::Unsupported: heading = "No supported game detected"; tone = theme.warning; break;
    case DiscDriveService::MediaState::Unreadable: heading = "Disc could not be read"; tone = theme.warning; break;
    }
    theme.bold().draw(canvas, rightX, top + theme.px(4), heading, theme.sizeTitle(), tone);
    int y = top + theme.px(72);
    const int metaGap = theme.px(28);
    const int metaColumnW = (rightW - metaGap) / 2;
    const auto drawMeta = [&](int x, int yy, const std::string &label, const std::string &value) {
        theme.bold().draw(canvas, x, yy, label, theme.sizeSmall(), theme.textMuted);
        theme.regular().draw(canvas, x, yy + theme.px(23), value, theme.sizeBody(), theme.textPrimary);
    };
    const int secondColumnX = rightX + metaColumnW + metaGap;
    using MetaField = std::pair<std::string, std::string>;
    std::vector<std::pair<MetaField, MetaField>> metaRows = {
        {{disc.system == DiscDriveService::System::Unknown ? "" : "SYSTEM",
          disc.system == DiscDriveService::System::Unknown
              ? "" : DiscDriveService::systemName(disc.system)},
         {artStatus_.releaseYear.empty() ? "" : "YEAR", artStatus_.releaseYear}},
        {{disc.region.empty() ? "" : "REGION", disc.region},
         {disc.identifier.empty() ? "" : "DISC ID", disc.identifier}},
        {{artStatus_.players.empty() ? "" : "PLAYERS", artStatus_.players},
         {artStatus_.coop.empty() ? "" : "CO-OP", artStatus_.coop}}
    };
    const int rowStep = theme.px(62);
    for (const auto &row : metaRows) {
        if (!row.first.first.empty())
            drawMeta(rightX, y, row.first.first, row.first.second);
        if (!row.second.first.empty())
            drawMeta(secondColumnX, y, row.second.first, row.second.second);
        if (!row.first.first.empty() || !row.second.first.empty()) y += rowStep;
    }
    if (!artStatus_.overview.empty() && y < detailsBottom) {
        theme.bold().draw(canvas, rightX, y, "OVERVIEW", theme.sizeSmall(), theme.textMuted);
        y += theme.regular().lineHeight(theme.sizeSmall()) + theme.px(6);
        const int lineHeight = theme.regular().lineHeight(theme.sizeSmall());
        const int maxLines = std::max(0, (detailsBottom - y) / std::max(1, lineHeight));
        if (maxLines > 0) {
            std::vector<std::string> lines = wrapText(theme.regular(), artStatus_.overview,
                                                       theme.sizeSmall(), rightW);
            const bool truncated = int(lines.size()) > maxLines;
            if (truncated) lines.resize(size_t(maxLines));
            if (truncated && !lines.empty())
                lines.back() = theme.regular().elide(lines.back() + "…", theme.sizeSmall(), rightW);
            for (const std::string &line : lines) {
                theme.regular().draw(canvas, rightX, y, line, theme.sizeSmall(), theme.textPrimary);
                y += lineHeight;
            }
        }
    }
    if (!disc.error.empty()) {
        theme.regular().draw(canvas, rightX, y, disc.error, theme.sizeSmall(), theme.warning);
        y += theme.px(38);
    }

    if (hasDisc) {
    const Rect button{rightX, buttonY, rightW, theme.px(64)};
    const bool installerBusy = context_.discInstaller.busy();
    const bool canLaunch = !installerBusy && !launchProbePending_ &&
                           coreStatus_.readyFor(disc.system) &&
                           disc.state == DiscDriveService::MediaState::Readable;
    const bool rbfMissing = disc.system == DiscDriveService::System::PlayStation
        ? !coreStatus_.psxCoreInstalled
        : disc.system == DiscDriveService::System::Saturn && !coreStatus_.saturnCoreInstalled;
    const std::string label = canLaunch ? "Start game" :
        installerBusy ? "Physical Disc package is updating" :
        launchProbePending_ ? "Rechecking disc and core…" :
        disc.state != DiscDriveService::MediaState::Readable ? "Waiting for supported disc" :
        rbfMissing ? "Install matching FPGA core" : "Configure Physical Disc package";
    const float buttonFocus = cursor_ == 0 ? 1.0f : 0.0f;
    const int growX = 0;
    const int growY = int(theme.px(5) * buttonFocus);
    const Rect buttonFrame{button.x - growX, button.y - growY,
                           button.w + growX * 2, button.h + growY * 2};
    const Color buttonTop = Color::lerp(theme.surface, theme.surfaceHi,
                                        0.35f + 0.65f * buttonFocus);
    const Color buttonBottom = Color::lerp(theme.backgroundLo, theme.surface,
                                           0.25f + 0.5f * buttonFocus);
    canvas.pushClip(area);
    if (buttonFocus > 0.0f)
        canvas.dropShadow(buttonFrame, theme.px(8), theme.px(12),
                          theme.shadow.withAlpha(uint8_t(150 * buttonFocus)));
    const int buttonRadius = theme.radius();
    drawRoundedGradient(canvas, buttonFrame, buttonRadius, buttonTop, buttonBottom);
    canvas.strokeRoundedRect(buttonFrame, buttonRadius, std::max(1, theme.px(1)),
                             theme.textMuted.withAlpha(150));
    if (buttonFocus > 0.0f)
        canvas.strokeRoundedRect(buttonFrame, buttonRadius, std::max(1, theme.px(2)),
                                 Color::lerp(theme.accent, theme.focus, 0.65f));
    const int labelWidth = theme.bold().measure(label, theme.sizeBody());
    theme.bold().draw(canvas, buttonFrame.x + (buttonFrame.w - labelWidth) / 2,
                      buttonFrame.y + (buttonFrame.h - theme.sizeBody()) / 2,
                      label, theme.sizeBody(), canLaunch ? theme.textPrimary : theme.textMuted);
    canvas.popClip();

    for (size_t i = 0; i < 4; ++i) {
        const int x = rightX + int(i) * (thumbW + shotGap);
        const Rect thumb{x, thumbsY, thumbW, thumbH};
        canvas.fillRoundedRect(thumb, theme.px(5), theme.surface);
        if (i < artStatus_.screenshots.size()) {
        ImagePtr shot = context_.images.requestAsync(artStatus_.screenshots[i], std::string(),
                                                     thumb.w, thumb.h, true);
        if (shot) canvas.drawImage(*shot, thumb, 255, theme.px(5));
        }
        canvas.strokeRoundedRect(thumb, theme.px(5), std::max(1, theme.px(2)),
                                 i < artStatus_.screenshots.size() && cursor_ == int(i + 1)
                                     ? theme.focus : theme.surfaceHi);
    }

    if (screenshotOverlay_ && overlayIndex_ < int(artStatus_.screenshots.size())) {
        canvas.fillRect(area, theme.shadow.withAlpha(230));
        const Rect imageArea{area.x + theme.px(70), area.y + theme.px(36),
                             area.w - theme.px(140), area.h - theme.px(100)};
        ImagePtr shot = context_.images.requestAsync(artStatus_.screenshots[size_t(overlayIndex_)],
                                                      std::string(), imageArea.w, imageArea.h, true);
        if (shot) {
            const int x = imageArea.x + (imageArea.w - shot->width()) / 2;
            const int yy = imageArea.y + (imageArea.h - shot->height()) / 2;
            canvas.drawImage(*shot, {x, yy, shot->width(), shot->height()}, 255, 0);
        }
        const std::string count = std::to_string(overlayIndex_ + 1) + " / " +
                                  std::to_string(artStatus_.screenshots.size());
        theme.bold().draw(canvas, area.x + area.w / 2, area.bottom() - theme.px(34),
                          count, theme.sizeBody(), theme.textPrimary);
    }
    }
}
