#include "Splash.h"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <unistd.h>

#include "Canvas.h"
#include "Framebuffer.h"
#include "GridView.h"
#include "Image.h"
#include "ImageCache.h"
#include "Input.h"   // nowMs
#include "Icons.h"
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

void show(Framebuffer &framebuffer, Theme &theme, int initialWaitMs, const Icons &icons,
          ImageCache &images) {
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
    const int64_t waitEndsAt = startedAt + std::max(0, initialWaitMs);

    const int logoSize = theme.px(128);
    const Rect logoArea{(canvas.width() - logoSize) / 2, (canvas.height() - logoSize) / 2 - theme.px(30),
                        logoSize, logoSize};
    const Rect track{canvas.width() / 2 - theme.px(140), logoArea.bottom() + theme.px(40),
                     theme.px(280), theme.px(10)};

    int driveIndex = 0;
    int64_t nextWarmupAt = startedAt;

    // Use the Systems grid dimensions and native-size cache entries so the screen can reuse
    // the icons loaded here without resizing them during loading.
    const int header = theme.topBarHeight() + theme.marginY();
    const Rect content{theme.marginX(), header,
                       canvas.width() - 2 * theme.marginX(),
                       canvas.height() - header - theme.bottomBarHeight() - theme.marginY()};
    GridView grid;
    grid.configure(content.inset(theme.px(18)), theme, 250, 4, 3, 0);

    const std::vector<std::string> iconPaths = icons.paths();
    size_t loadedIcons = 0;
    bool completionFrameShown = false;

    int64_t now = startedAt;
    while (now < waitEndsAt || loadedIcons < iconPaths.size() || !completionFrameShown) {
        // The first 25% is a real two-second settle period. Afterwards each icon accounts for
        // an equal share of the remaining 75%; one decode per frame keeps the work sequential
        // and lets the splash redraw between assets. If decoding takes longer, this loop simply
        // keeps the splash up until the final icon is cached.
        if (now >= waitEndsAt && loadedIcons < iconPaths.size()) {
            images.beginFrame(32);
            images.get(iconPaths[loadedIcons], grid.tileWidth(), grid.tileHeight(), false);
            ++loadedIcons;
        }

        canvas.clear(theme.background);

        if (logo && logo->valid()) canvas.drawImage(*logo, logoArea, 255, theme.px(16));

        canvas.fillRoundedRect(track, theme.px(5), theme.surface);
        const float waitFraction = initialWaitMs > 0
                                       ? std::min(1.0f, float(now - startedAt) /
                                                            float(initialWaitMs))
                                       : 1.0f;
        const float iconFraction = iconPaths.empty()
                                       ? (now >= waitEndsAt ? 1.0f : 0.0f)
                                       : float(loadedIcons) / float(iconPaths.size());
        const float fraction = now < waitEndsAt
                                   ? 0.25f * waitFraction
                                   : 0.25f + 0.75f * iconFraction;
        const int filled = std::max(theme.px(10), int(float(track.w) * fraction));
        canvas.fillRoundedRect({track.x, track.y, filled, track.h}, theme.px(5), theme.accent);

        framebuffer.present(canvas);
        if (fraction >= 1.0f) completionFrameShown = true;

        if (now >= nextWarmupAt) {
            warmUpNextDrive(driveIndex);
            nextWarmupAt = now + 250;
        }

        if (now < waitEndsAt || loadedIcons < iconPaths.size() || !completionFrameShown)
            usleep(16000);
        now = nowMs();
    }
}

} // namespace Splash
