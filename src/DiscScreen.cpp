#include "DiscScreen.h"

#include <algorithm>

#include "Canvas.h"

DiscScreen::DiscScreen(Context &context, DiscCoreManager &cores,
                       std::function<void(const std::string &)> launch)
    : context_(context), cores_(cores), launch_(std::move(launch)) {
    coreStatus_ = cores_.snapshot();
}

void DiscScreen::update(float deltaSeconds) {
    (void)deltaSeconds;
    const auto disc = context_.discDrive.snapshot();
    context_.discArtwork.request(disc);
    coreStatus_ = cores_.snapshot();
    artStatus_ = context_.discArtwork.snapshot();
}

void DiscScreen::handle(Action action) {
    if (screenshotOverlay_) {
        if (action == Action::Back || action == Action::Confirm) screenshotOverlay_ = false;
        else if (action == Action::Left && overlayIndex_ > 0) --overlayIndex_;
        else if (action == Action::Right && overlayIndex_ + 1 < int(artStatus_.screenshots.size())) ++overlayIndex_;
        return;
    }
    if (action == Action::Left && cursor_ > 0) --cursor_;
    else if (action == Action::Right && cursor_ < int(artStatus_.screenshots.size())) ++cursor_;
    else if ((action == Action::Up || action == Action::Down) && cursor_ != 0) cursor_ = 0;
    else if (action == Action::Confirm) {
        if (cursor_ > 0 && cursor_ <= int(artStatus_.screenshots.size())) {
            overlayIndex_ = cursor_ - 1;
            screenshotOverlay_ = true;
            return;
        }
        const auto disc = context_.discDrive.snapshot();
        std::string mgl;
        if (disc.system == DiscDriveService::System::PlayStation) mgl = coreStatus_.psxMgl;
        if (disc.system == DiscDriveService::System::Saturn) mgl = coreStatus_.saturnMgl;
        if (disc.state == DiscDriveService::MediaState::Readable &&
            coreStatus_.readyFor(disc.system) && !mgl.empty() && launch_)
            launch_(mgl);
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
    if (screenshotOverlay_) return "←/→ Browse screenshots   A/B Close";
    if (cursor_ > 0) return "A View screenshot   ←/→ Choose   B Back";
    return "A Start game   ←/→ Choose screenshot";
}

void DiscScreen::render(Canvas &canvas, const Rect &area, bool) {
    Theme &theme = context_.theme;
    theme.bold().draw(canvas, area.x, area.y, "Disc Drive", theme.sizeHeading(), theme.textPrimary);
    const auto disc = context_.discDrive.snapshot();
    const int gap = theme.px(42);
    const int top = area.y + theme.px(62);
    const int leftW = std::min(theme.px(470), area.w * 39 / 100);
    const Rect artBox{area.x, top, leftW, area.bottom() - top - theme.px(8)};
    const int rightX = artBox.right() + gap;
    const int rightW = std::max(0, area.right() - rightX);

    ImagePtr boxart;
    if (!artStatus_.boxart.empty())
        boxart = context_.images.requestAsync(artStatus_.boxart, std::string(), artBox.w, artBox.h, true);
    if (boxart) {
        const int x = artBox.x + (artBox.w - boxart->width()) / 2;
        const int y = artBox.y + (artBox.h - boxart->height()) / 2;
        canvas.drawImage(*boxart, {x, y, boxart->width(), boxart->height()}, 255, theme.px(8));
    } else {
        const int size = std::min(artBox.w, artBox.h) * 72 / 100;
        const Rect discShape{artBox.x + (artBox.w - size) / 2, artBox.y + (artBox.h - size) / 2,
                             size, size};
        canvas.fillRoundedRect(discShape, size / 2, theme.surfaceHi);
        canvas.strokeRoundedRect(discShape, size / 2, std::max(1, theme.px(3)), theme.textMuted);
        const int inner = size / 5;
        canvas.fillRoundedRect({discShape.x + (size - inner) / 2, discShape.y + (size - inner) / 2,
                                inner, inner}, inner / 2, theme.background);
        if (artStatus_.state == DiscArtworkService::State::Searching)
            theme.regular().draw(canvas, artBox.x, artBox.bottom() - theme.px(44),
                                 "Loading artwork…", theme.sizeBody(), theme.textMuted);
    }

    std::string heading;
    Color tone = theme.textPrimary;
    switch (disc.state) {
    case DiscDriveService::MediaState::NoDrive: heading = "No optical drive detected"; tone = theme.textMuted; break;
    case DiscDriveService::MediaState::NoDisc: heading = "Please insert disc"; tone = theme.textMuted; break;
    case DiscDriveService::MediaState::Checking: heading = "Reading disc…"; tone = theme.accent; break;
    case DiscDriveService::MediaState::Readable: heading = disc.title.empty() ? "PlayStation disc detected" : disc.title; tone = theme.accent; break;
    case DiscDriveService::MediaState::Unsupported: heading = "No supported game detected"; tone = theme.warning; break;
    case DiscDriveService::MediaState::Unreadable: heading = "Disc could not be read"; tone = theme.warning; break;
    }
    theme.bold().draw(canvas, rightX, top + theme.px(8), heading, theme.sizeTitle(), tone);
    int y = top + theme.px(84);
    const auto drawLine = [&](const std::string &label, const std::string &value) {
        theme.bold().draw(canvas, rightX, y, label, theme.sizeSmall(), theme.textMuted);
        theme.regular().draw(canvas, rightX + theme.px(170), y, value, theme.sizeBody(), theme.textPrimary);
        y += theme.px(38);
    };
    if (disc.system != DiscDriveService::System::Unknown)
        drawLine("System", DiscDriveService::systemName(disc.system));
    if (!disc.identifier.empty()) drawLine("Disc ID", disc.identifier);
    if (!disc.model.empty()) drawLine("Drive", disc.model);
    if (!disc.error.empty()) {
        theme.regular().draw(canvas, rightX, y + theme.px(8), disc.error, theme.sizeSmall(), theme.warning);
        y += theme.px(38);
    }

    const int rowBottom = area.bottom() - theme.px(8);
    const int thumbSize = std::min(theme.px(150), std::max(1, (rightW - theme.px(18) * 3) / 4));
    const int thumbsY = rowBottom - thumbSize;
    const int buttonY = thumbsY - theme.px(82);
    const Rect button{rightX, buttonY, thumbSize * 4 + theme.px(18) * 3, theme.px(60)};
    const bool canLaunch = coreStatus_.readyFor(disc.system) &&
                           disc.state == DiscDriveService::MediaState::Readable;
    const bool rbfMissing = disc.system == DiscDriveService::System::PlayStation
        ? !coreStatus_.psxCoreInstalled
        : disc.system == DiscDriveService::System::Saturn && !coreStatus_.saturnCoreInstalled;
    const std::string label = canLaunch ? "Start game" :
        disc.state != DiscDriveService::MediaState::Readable ? "Waiting for supported disc" :
        rbfMissing ? "Install matching FPGA core" : "Configure Physical Disc package";
    canvas.fillRoundedRect(button, theme.px(9), canLaunch ? theme.accent : theme.surfaceHi);
    canvas.strokeRoundedRect(button, theme.px(9), std::max(1, theme.px(2)),
                             cursor_ == 0 ? theme.focus : theme.surfaceHi);
    const int labelWidth = theme.bold().measure(label, theme.sizeBody());
    theme.bold().draw(canvas, button.x + (button.w - labelWidth) / 2,
                      button.y + theme.px(17), label, theme.sizeBody(),
                      canLaunch ? theme.background : theme.textMuted);

    for (size_t i = 0; i < artStatus_.screenshots.size() && i < 4; ++i) {
        const int x = rightX + int(i) * (thumbSize + theme.px(18));
        const Rect thumb{x, thumbsY, thumbSize, thumbSize};
        ImagePtr shot = context_.images.requestAsync(artStatus_.screenshots[i], std::string(),
                                                     thumb.w, thumb.h, true);
        if (shot) canvas.drawImage(*shot, thumb, 255, theme.px(5));
        else canvas.fillRoundedRect(thumb, theme.px(5), theme.surface);
        canvas.strokeRoundedRect(thumb, theme.px(5), std::max(1, theme.px(2)),
                                 cursor_ == int(i + 1) ? theme.focus : theme.surfaceHi);
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
