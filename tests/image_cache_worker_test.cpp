#include "Image.h"
#include "ImageCache.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

ImagePtr solid(int r, int g, int b) {
    auto image = std::make_shared<Image>(24, 24);
    for (int y = 0; y < image->height(); ++y)
        for (int x = 0; x < image->width(); ++x)
            image->row(y)[x] = 0xFF000000u | (uint32_t(r) << 16) |
                               (uint32_t(g) << 8) | uint32_t(b);
    return image;
}

bool waitFor(ImageCache &cache, const std::string &path, const std::string &fallback,
             bool expectRed, int maxFrames = 400) {
    for (int frame = 0; frame < maxFrames; ++frame) {
        cache.beginFrame();
        ImagePtr image = cache.requestAsync(path, fallback, 24, 24, true);
        if (image) {
            const uint32_t pixel = image->row(0)[0];
            const bool red = ((pixel >> 16) & 0xFF) > ((pixel >> 8) & 0xFF);
            if (red == expectRed) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

} // namespace

int main() {
    char directory[] = "/tmp/mister-image-cache-XXXXXX";
    char *created = mkdtemp(directory);
    check(created != nullptr, "create temporary directory");
    if (!created) return 1;

    const std::string root(created);
    const std::string lateBmp = root + "/late.bmp";
    const std::string fallbackJpeg = root + "/fallback.jpg";
    const std::string badJpeg = root + "/bad.jpg";
    const std::string latestBmp = root + "/latest.bmp";
    const std::string afterBadBmp = root + "/after-bad.bmp";
    const std::string budgetBmp1 = root + "/budget-1.bmp";
    const std::string budgetBmp2 = root + "/budget-2.bmp";
    const std::string budgetBmp3 = root + "/budget-3.bmp";

    check(solid(0, 0, 255)->saveJpeg(fallbackJpeg), "write JPEG fallback");
    check(solid(0, 255, 0)->saveBmp(latestBmp), "write latest BMP");
    check(solid(0, 255, 0)->saveBmp(budgetBmp1), "write first budget BMP");
    check(solid(0, 255, 0)->saveBmp(budgetBmp2), "write second budget BMP");
    check(solid(0, 255, 0)->saveBmp(budgetBmp3), "write third budget BMP");
    {
        std::FILE *bad = std::fopen(badJpeg.c_str(), "wb");
        check(bad != nullptr, "create malformed JPEG");
        if (bad) {
            const unsigned char bytes[] = {0xFF, 0xD8, 0xFF, 0x00};
            std::fwrite(bytes, 1, sizeof(bytes), bad);
            std::fclose(bad);
        }
    }

    {
        ImageCache cache(8);

        // Simulate ten thousand fast letter skips. Each frame has a different focused game;
        // old queued work must not accumulate ahead of the final selection.
        for (int i = 0; i < 10000; ++i) {
            cache.beginFrame();
            const std::string skipped = root + "/skip-" + std::to_string(i) + ".bmp";
            cache.requestAsync(skipped, {}, 24, 24, true);
        }
        check(waitFor(cache, latestBmp, {}, false),
              "latest focused image loads after rapid letter skips");

        // Corrupt decoder input must not terminate the worker or the application.
        cache.beginFrame();
        cache.requestAsync(badJpeg, {}, 24, 24, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        check(solid(255, 255, 0)->saveBmp(afterBadBmp), "write post-error recovery image");
        check(waitFor(cache, afterBadBmp, {}, false), "worker survives malformed JPEG input");

        // A fallback shown while its preferred BMP is being prepared must be replaced after
        // the BMP appears and the cache's retry interval elapses.
        check(waitFor(cache, lateBmp, fallbackJpeg, false), "show JPEG while BMP is absent");
        check(solid(255, 0, 0)->saveBmp(lateBmp), "write preferred BMP after fallback cached");
        for (int frame = 0; frame < 95; ++frame) {
            cache.beginFrame();
            cache.requestAsync(lateBmp, fallbackJpeg, 24, 24, true);
        }
        check(waitFor(cache, lateBmp, fallbackJpeg, true),
              "replace cached fallback with newly prepared BMP");
    }

    {
        // Three 24x24 ARGB images exceed this byte budget together. The cache must evict by
        // actual pixel memory, even when the entry-count limit has not been reached.
        ImageCache cache(20, 3000);
        check(waitFor(cache, budgetBmp1, {}, false), "load first image under RAM budget");
        check(cache.memoryBytes() <= 3000, "first image respects RAM budget");
        check(waitFor(cache, budgetBmp2, {}, false), "load second image under RAM budget");
        check(cache.memoryBytes() <= 3000, "second image respects RAM budget");
        check(waitFor(cache, budgetBmp3, {}, false), "load third image under RAM budget");
        check(cache.memoryBytes() <= 3000, "third image respects RAM budget");
    }

    // Destruction with queued and in-flight work must join the worker safely.
    {
        ImageCache cache(8);
        for (int i = 0; i < 32; ++i) {
            cache.beginFrame();
            cache.requestAsync(badJpeg + std::to_string(i), {}, 24, 24, true);
        }
    }

    unlink(lateBmp.c_str());
    unlink(fallbackJpeg.c_str());
    unlink(badJpeg.c_str());
    unlink(latestBmp.c_str());
    unlink(afterBadBmp.c_str());
    unlink(budgetBmp1.c_str());
    unlink(budgetBmp2.c_str());
    unlink(budgetBmp3.c_str());
    rmdir(root.c_str());

    if (failures) return 1;
    std::puts("ImageCache worker stress checks passed.");
    return 0;
}
