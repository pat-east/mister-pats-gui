#include "Splash.h"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <unistd.h>

#include "Canvas.h"
#include "Framebuffer.h"
#include "Image.h"
#include "Input.h"   // nowMs
#include "SplashImage.h"
#include "Theme.h"

namespace {

// A directory read on each present USB slot, spread out over the splash instead of fired all
// at once — see ImageCache's note on the same drive losing an early, un-paced read. Whichever
// slots are not actually populated just fail to open and cost nothing.
void warmUpNextDrive(int &index) {
    char path[16];
    std::snprintf(path, sizeof(path), "/media/usb%d", index % 6);
    ++index;

    if (DIR *dir = opendir(path)) {
        readdir(dir);
        closedir(dir);
    }
}

} // namespace

namespace Splash {

void show(Framebuffer &framebuffer, Theme &theme, int durationMs) {
    // /tmp is a RAM disk on a MiSTer (see MediaScraper's kTempImage) — writing the embedded
    // bytes out is just to reuse Image::load()'s JPEG decoder, not a real disk access.
    const char *tempPath = "/tmp/mister-pat-splash.jpg";
    if (FILE *out = std::fopen(tempPath, "wb")) {
        std::fwrite(splash_image::kData, 1, splash_image::kSize, out);
        std::fclose(out);
    }
    ImagePtr logo = Image::load(tempPath);
    unlink(tempPath);

    Canvas canvas(framebuffer.width(), framebuffer.height());
    const int64_t startedAt = nowMs();
    const int64_t endsAt = startedAt + durationMs;

    const int logoSize = theme.px(128);
    const Rect logoArea{(canvas.width() - logoSize) / 2, (canvas.height() - logoSize) / 2 - theme.px(30),
                        logoSize, logoSize};
    const Rect track{canvas.width() / 2 - theme.px(140), logoArea.bottom() + theme.px(40),
                     theme.px(280), theme.px(10)};

    int driveIndex = 0;
    int64_t nextWarmupAt = startedAt;

    int64_t now = startedAt;
    while (now < endsAt) {
        canvas.clear(theme.background);

        if (logo && logo->valid()) canvas.drawImage(*logo, logoArea, 255, theme.px(16));

        canvas.fillRoundedRect(track, theme.px(5), theme.surface);
        const float fraction = std::min(1.0f, float(now - startedAt) / float(durationMs));
        const int filled = std::max(theme.px(10), int(float(track.w) * fraction));
        canvas.fillRoundedRect({track.x, track.y, filled, track.h}, theme.px(5), theme.accent);

        framebuffer.present(canvas);

        if (now >= nextWarmupAt) {
            warmUpNextDrive(driveIndex);
            nextWarmupAt = now + 250;
        }

        usleep(16000);
        now = nowMs();
    }
}

} // namespace Splash
