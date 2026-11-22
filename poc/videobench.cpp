// PoC: how fast can this machine decode a preview video? Headless: nothing is drawn.
//
//   videobench FILE.mp4 [--seconds N] [--audio] [--variant normal|nodeblock|nonref] [--timeline]
//
// Decodes the file as fast as it can, converts every frame to BGRA at the video's own size
// (no scaling) and throws the pixels away. Reports frames per second, milliseconds per frame
// and CPU time against wall time. Runs three variants one after the other unless one is chosen;
// each is capped at --seconds of wall time (default 20), so the whole run stays near a minute.
//
// Meant to be run by hand on the device. It reads one file and writes nothing.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "videodec.h"

namespace {

double now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
}

double cpuNow() {
    timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
}

struct Result {
    std::vector<float> ms;  // decode + convert time of every frame
    long frames = 0;
    double wall = 0, cpu = 0, worstMs = 0, sumMs = 0, lastPts = 0;
    bool finishedFile = false;
};

Result run(const std::string &path, DecodeVariant variant, bool audio, double capSeconds,
           VideoDecoder &decoder) {
    Result result;
    if (!decoder.open(path, variant, audio)) {
        std::fprintf(stderr, "  open failed: %s\n", decoder.error().c_str());
        return result;
    }
    std::vector<uint8_t> pixels(size_t(decoder.width()) * decoder.height() * 4);
    const double start = now();
    const double cpuStart = cpuNow();
    double pts = 0;
    for (;;) {
        const double t0 = now();
        if (t0 - start > capSeconds) break;
        if (!decoder.next(pixels.data(), pts)) { result.finishedFile = true; break; }
        const double ms = (now() - t0) * 1000.0;
        result.ms.push_back(float(ms));
        result.sumMs += ms;
        if (ms > result.worstMs) result.worstMs = ms;
        result.lastPts = pts;
        ++result.frames;
    }
    result.wall = now() - start;
    result.cpu = cpuNow() - cpuStart;
    return result;
}

const char *variantName(DecodeVariant v) {
    switch (v) {
    case DecodeVariant::Normal: return "normal";
    case DecodeVariant::NoDeblock: return "nodeblock";
    case DecodeVariant::NonRef: return "nonref";
    }
    return "?";
}

}  // namespace

int main(int argc, char **argv) {
    std::string path;
    double seconds = 20.0;
    bool audio = false, timeline = false;
    std::vector<DecodeVariant> variants = {DecodeVariant::Normal, DecodeVariant::NoDeblock,
                                           DecodeVariant::NonRef};
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--seconds" && i + 1 < argc) seconds = std::atof(argv[++i]);
        else if (a == "--audio") audio = true;
        else if (a == "--timeline") timeline = true;
        else if (a == "--variant" && i + 1 < argc) {
            const std::string v = argv[++i];
            if (v == "normal") variants = {DecodeVariant::Normal};
            else if (v == "nodeblock") variants = {DecodeVariant::NoDeblock};
            else if (v == "nonref") variants = {DecodeVariant::NonRef};
            else { std::fprintf(stderr, "unknown variant %s\n", v.c_str()); return 2; }
        } else if (a[0] != '-') path = a;
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }
    if (path.empty()) {
        std::fprintf(stderr, "usage: %s FILE.mp4 [--seconds N] [--audio] [--variant normal|nodeblock|nonref]\n", argv[0]);
        return 2;
    }
    if (seconds < 1.0) seconds = 1.0;
    if (seconds > 60.0) seconds = 60.0;

    av_log_set_level(AV_LOG_ERROR);

    VideoDecoder probe;
    if (!probe.open(path, DecodeVariant::Normal, audio)) {
        std::fprintf(stderr, "cannot use %s: %s\n", path.c_str(), probe.error().c_str());
        return 1;
    }
    std::printf("file     : %s\n", path.c_str());
    std::printf("video    : %s %s level %d, %dx%d, %.2f fps, %.1f s\n", probe.codecName().c_str(),
                probe.profile().c_str(), probe.level(), probe.width(), probe.height(), probe.fps(),
                probe.duration());
    if (audio)
        std::printf("audio    : %s (%d Hz, %d ch), decoded and converted to 48 kHz S16 stereo\n",
                    probe.hasAudio() ? "present" : "none", probe.audioRate(), probe.audioChannels());
    std::printf("slot     : 689x624 -> %s\n",
                probe.width() <= 689 && probe.height() <= 624 ? "fits, shown 1:1"
                                                              : "DOES NOT FIT (would not be played)");
    std::printf("cap      : %.0f s per variant\n\n", seconds);
    const double native = probe.fps();
    probe.close();

    std::printf("%-10s %7s %8s %9s %9s %8s %8s %s\n", "variant", "frames", "fps", "mean ms", "worst ms",
                "cpu/wall", "x real", "note");
    for (DecodeVariant variant : variants) {
        VideoDecoder decoder;
        const Result r = run(path, variant, audio, seconds, decoder);
        if (r.frames == 0 || r.wall <= 0) {
            std::printf("%-10s no frames\n", variantName(variant));
            continue;
        }
        const double fps = double(r.frames) / r.wall;
        // How many seconds of video were decoded per second of wall time. 1.0 is real time.
        const double xreal = r.lastPts / r.wall;
        std::printf("%-10s %7ld %8.1f %9.2f %9.2f %7.0f%% %8.2f %s\n", variantName(variant), r.frames,
                    fps, r.sumMs / double(r.frames), r.worstMs, 100.0 * r.cpu / r.wall, xreal,
                    r.finishedFile ? "whole file" : "stopped at cap");
        if (audio) std::printf("           audio samples converted: %lld\n", decoder.audioSamples());
        if (timeline) {
            std::printf("           per second of video: mean / worst ms per frame\n");
            const size_t perSec = size_t(native + 0.5);
            for (size_t i = 0; i * perSec < r.ms.size(); ++i) {
                double sum = 0, worst = 0;
                size_t n = 0;
                for (size_t f = i * perSec; f < (i + 1) * perSec && f < r.ms.size(); ++f, ++n) {
                    sum += r.ms[f];
                    if (r.ms[f] > worst) worst = r.ms[f];
                }
                std::printf("           %3zu s: %5.1f / %5.1f\n", i, sum / double(n), worst);
            }
        }
    }
    std::printf("\nnative rate of the file: %.2f fps. 'x real' above 1.0 means faster than playback;\n"
                "playback also needs the picture copied to the screen, which is not measured here.\n",
                native);
    return 0;
}
