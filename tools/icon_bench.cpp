#include "Canvas.h"
#include "Image.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr int kDecodedIconSize = 201; // Systems cache fit at 1920x1080: 268x201 -> 201x201
constexpr int kDrawSize = 163;        // Systems tile body after caption and image inset
constexpr int kColumns = 6;
constexpr int kRows = 3;
constexpr int kRenderWarmupFrames = 12;
constexpr int kRenderMeasuredFrames = 120;
constexpr int kRenderRepeats = 3;
constexpr int kDecodeWarmupPasses = 1;
constexpr int kDecodeMeasuredPasses = 5;

struct Asset {
    std::string name;
    std::string png;
    std::string bmp;
};

struct TimedImage {
    double decodeMs = 0;
    double scaleMs = 0;
    ImagePtr scaled;
};

struct Samples {
    std::vector<double> decode;
    std::vector<double> scale;
    std::vector<double> total;
    double coldTotalMs = 0;
};

double elapsedMs(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

bool endsWith(const std::string &s, const char *suffix) {
    const size_t n = std::char_traits<char>::length(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

std::vector<Asset> findAssets(const std::string &directory) {
    std::vector<Asset> result;
    DIR *dir = opendir(directory.c_str());
    if (!dir) return result;

    while (dirent *entry = readdir(dir)) {
        const std::string file = entry->d_name;
        if (!endsWith(file, ".png")) continue;
        const std::string base = file.substr(0, file.size() - 4);
        result.push_back({base, directory + "/" + base + ".png",
                          directory + "/" + base + ".bmp"});
    }
    closedir(dir);
    std::sort(result.begin(), result.end(), [](const Asset &a, const Asset &b) {
        return a.name < b.name;
    });
    return result;
}

TimedImage loadAndScale(const std::string &path, bool resize = true) {
    TimedImage result;
    const auto start = Clock::now();
    ImagePtr source = Image::load(path);
    const auto decoded = Clock::now();
    if (source && source->valid())
        result.scaled = resize ? source->scaledTo(kDecodedIconSize, kDecodedIconSize) : source;
    const auto scaled = Clock::now();
    result.decodeMs = elapsedMs(start, decoded);
    result.scaleMs = elapsedMs(decoded, scaled);
    return result;
}

void addSample(Samples &samples, const TimedImage &image) {
    samples.decode.push_back(image.decodeMs);
    samples.scale.push_back(image.scaleMs);
    samples.total.push_back(image.decodeMs + image.scaleMs);
}

double total(const std::vector<double> &values) {
    double result = 0;
    for (double value : values) result += value;
    return result;
}

uint64_t fileBytes(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 ? uint64_t(st.st_size) : 0;
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const size_t index = size_t(p * double(values.size() - 1));
    return values[index];
}

double average(const std::vector<double> &values) {
    return values.empty() ? 0 : total(values) / double(values.size());
}

void printSummary(const char *label, const Samples &samples) {
    std::printf("%s: decode mean %.3f ms, scale mean %.3f ms, total mean %.3f ms/icon; "
                "total p50 %.3f, p95 %.3f ms/icon; measured total %.1f ms\n",
                label, average(samples.decode), average(samples.scale), average(samples.total),
                percentile(samples.total, 0.50), percentile(samples.total, 0.95),
                total(samples.total));
}

double renderFrames(Canvas &canvas, const std::vector<ImagePtr> &images, int frameCount,
                    uint8_t alpha) {
    const std::vector<Rect> destinations = [&] {
        std::vector<Rect> result;
        result.reserve(kColumns * kRows);
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col)
                result.push_back({90 + col * 294, 230 + row * 230, kDrawSize, kDrawSize});
        }
        return result;
    }();

    std::vector<double> frameTimes;
    frameTimes.reserve(frameCount);
    for (int frame = 0; frame < frameCount; ++frame) {
        canvas.clearDamage();
        const auto start = Clock::now();
        for (size_t i = 0; i < destinations.size(); ++i)
            canvas.drawImage(*images[i], destinations[i], alpha, 4);
        frameTimes.push_back(elapsedMs(start, Clock::now()));
    }

    volatile uint32_t sink = 0;
    for (int y = 230; y < 230 + kRows * 230; y += 31)
        for (int x = 90; x < 90 + kColumns * 294; x += 31)
            sink ^= canvas.row(y)[x];
    (void)sink;

    frameTimes.erase(frameTimes.begin(), frameTimes.begin() + kRenderWarmupFrames);
    std::printf("    run mean %.3f ms/frame, p50 %.3f, p95 %.3f ms/frame (%d icons/frame)\n",
                average(frameTimes), percentile(frameTimes, 0.50), percentile(frameTimes, 0.95),
                kColumns * kRows);
    return average(frameTimes);
}

void printGain(const char *label, double oldMs, double newMs) {
    if (oldMs <= 0) return;
    std::printf("  %s gain: %.1f%% less time (%.2fx speedup)\n", label,
                (oldMs - newMs) * 100.0 / oldMs, oldMs / newMs);
}

} // namespace

int main(int argc, char **argv) {
    const std::string directory = argc > 1 ? argv[1] : "/media/fat/mister-pat/icons";
    const std::vector<Asset> assets = findAssets(directory);
    if (assets.empty()) {
        std::fprintf(stderr, "no PNG icons found in %s\n", directory.c_str());
        return 1;
    }

    for (const Asset &asset : assets) {
        FILE *bmp = std::fopen(asset.bmp.c_str(), "rb");
        if (!bmp) {
            std::fprintf(stderr, "missing BMP for %s: %s\n", asset.name.c_str(),
                         asset.bmp.c_str());
            return 1;
        }
        std::fclose(bmp);
    }

    std::printf("MiSTer icon benchmark, %zu matched PNG/BMP pairs\n", assets.size());
    std::printf("PNG source: 300x300; BMP source: 163x163 opaque 24-bit; "
                "PNG cache fit: %dx%d; native BMP: 163x163; Systems draw: %dx%d\n",
                kDecodedIconSize, kDecodedIconSize, kDrawSize, kDrawSize);
    uint64_t pngBytes = 0, bmpBytes = 0;
    for (const Asset &asset : assets) {
        pngBytes += fileBytes(asset.png);
        bmpBytes += fileBytes(asset.bmp);
    }
    std::printf("Asset bytes: PNG %llu, BMP %llu (BMP/PNG %.2fx)\n",
                static_cast<unsigned long long>(pngBytes),
                static_cast<unsigned long long>(bmpBytes),
                pngBytes ? double(bmpBytes) / double(pngBytes) : 0.0);

    Samples pngSamples, bmpSamples;
    double pngColdMs = 0, bmpColdMs = 0;
    for (size_t i = 0; i < assets.size(); ++i) {
        const bool pngFirst = (i % 2) == 0;
        TimedImage png, bmp;
        if (pngFirst) {
            png = loadAndScale(assets[i].png);
            bmp = loadAndScale(assets[i].bmp, false);
        } else {
            bmp = loadAndScale(assets[i].bmp, false);
            png = loadAndScale(assets[i].png);
        }
        if (!png.scaled || !bmp.scaled) {
            std::fprintf(stderr, "could not decode/scale icon pair %s\n", assets[i].name.c_str());
            return 1;
        }
        pngColdMs += png.decodeMs + png.scaleMs;
        bmpColdMs += bmp.decodeMs + bmp.scaleMs;
    }
    std::printf("First alternating pass (page cache state is reported as found):\n");
    std::printf("  PNG decode+scale total %.1f ms; BMP decode+scale total %.1f ms\n",
                pngColdMs, bmpColdMs);
    printGain("first-pass load", pngColdMs, bmpColdMs);

    // All files have now been read once. Repeated alternating passes compare CPU/decode work
    // with both formats resident in the OS page cache and reduce file-order bias.
    for (int pass = 0; pass < kDecodeWarmupPasses; ++pass) {
        for (const Asset &asset : assets) {
            loadAndScale(asset.png);
            loadAndScale(asset.bmp, false);
        }
    }
    for (int pass = 0; pass < kDecodeMeasuredPasses; ++pass) {
        for (size_t i = 0; i < assets.size(); ++i) {
            TimedImage png, bmp;
            if (((i + size_t(pass)) % 2) == 0) {
                png = loadAndScale(assets[i].png);
                bmp = loadAndScale(assets[i].bmp, false);
            } else {
                bmp = loadAndScale(assets[i].bmp, false);
                png = loadAndScale(assets[i].png);
            }
            addSample(pngSamples, png);
            addSample(bmpSamples, bmp);
        }
    }
    std::printf("Warm page-cache decode + scale (%d passes):\n", kDecodeMeasuredPasses);
    printSummary("  PNG", pngSamples);
    printSummary("  BMP", bmpSamples);
    printGain("warm load+scale", total(pngSamples.total), total(bmpSamples.total));

    std::vector<ImagePtr> pngDrawImages, bmpDrawImages;
    for (int i = 0; i < kColumns * kRows; ++i) {
        TimedImage png = loadAndScale(assets[size_t(i)].png);
        TimedImage bmp = loadAndScale(assets[size_t(i)].bmp, false);
        pngDrawImages.push_back(std::move(png.scaled));
        bmpDrawImages.push_back(std::move(bmp.scaled));
    }

    Canvas canvas(1920, 1080);
    std::printf("Canvas-only Systems icon rendering (%d alternating runs each, "
                "%d warm-up + %d measured frames per run):\n",
                kRenderRepeats, kRenderWarmupFrames, kRenderMeasuredFrames);
    std::vector<double> pngRenderRuns, bmpRenderRuns;
    for (int repeat = 0; repeat < kRenderRepeats; ++repeat) {
        if ((repeat % 2) == 0) {
            pngRenderRuns.push_back(renderFrames(
                canvas, pngDrawImages, kRenderWarmupFrames + kRenderMeasuredFrames, 215));
            bmpRenderRuns.push_back(renderFrames(
                canvas, bmpDrawImages, kRenderWarmupFrames + kRenderMeasuredFrames, 255));
        } else {
            bmpRenderRuns.push_back(renderFrames(
                canvas, bmpDrawImages, kRenderWarmupFrames + kRenderMeasuredFrames, 255));
            pngRenderRuns.push_back(renderFrames(
                canvas, pngDrawImages, kRenderWarmupFrames + kRenderMeasuredFrames, 215));
        }
    }
    const double pngRenderMs = average(pngRenderRuns);
    const double bmpRenderMs = average(bmpRenderRuns);
    std::printf("  alternating-run mean: PNG %.3f, BMP %.3f ms/frame\n", pngRenderMs,
                bmpRenderMs);
    printGain("render", pngRenderMs, bmpRenderMs);
    return 0;
}
