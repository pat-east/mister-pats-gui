// PoC: play a preview video straight into the framebuffer, the way the GUI would: a decode
// thread produces finished BGRA frames into a small pool, and the main thread, ticking at the
// GUI's ~30 Hz frame rate, copies the newest due frame into the framebuffer. Silent unless
// --audio is given.
//
//   videoplay FILE.mp4 [--seconds N] [--x X --y Y] [--fb /dev/fb0] [--nice N] [--dry]
//             [--audio] [--volume PERCENT] [--buffer-ms N] [--av-offset-ms N] [--timeline]
//
// Sound (--audio): the decode thread converts the AAC track to signed 16-bit stereo 48 kHz and
// a writer thread feeds it to `aplay -t raw` over a pipe, the same tool the GUI uses for its
// interface sounds. --volume scales the samples in software (default 25, the system volume is
// not touched). --buffer-ms is aplay's buffer (default 150): small keeps picture and sound close
// together, large survives a busy CPU. --av-offset-ms delays the picture clock against the
// sound start (positive: picture later); tune it by ear. --timeline prints one line per second
// of playback (frames shown, dropped, stalls, lateness, decode time) to find where it slows down.
//
// The video is shown at its OWN size, unscaled. One that does not fit the 689x624 slot of the
// GUI's List view is refused, not shrunk. The picture is placed where the List view's detail
// slot is (right half of a 1080p screen) unless --x/--y say otherwise. What was on the screen
// under the video is saved first and put back when playback ends.
//
// The GUI must not be drawing at the same time: suspend it first (kill -STOP) and resume it
// afterwards. --dry decodes and copies into memory instead of the framebuffer, for testing the
// pipeline on a development machine.
#include <atomic>
#include <chrono>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#ifdef __linux__
#include <linux/fb.h>
#endif
#include <deque>
#include <mutex>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "videodec.h"

namespace {

constexpr int kSlotW = 689;
constexpr int kSlotH = 624;
constexpr int kTickMs = 33;
constexpr int kPool = 3;

std::atomic<bool> g_stop{false};
void onSignal(int) { g_stop = true; }

double now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
}

double cpuProcess() {
    timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
}

struct Slot {
    std::vector<uint8_t> pixels;
    double pts = 0;
    bool ready = false;
};

struct Pool {
    std::mutex mutex;
    std::condition_variable changed;
    Slot slots[kPool];
    bool firstReady = false;
};

struct DecodeStats {
    std::atomic<long> frames{0};
    std::atomic<long> loops{0};
    std::atomic<double> cpu{0.0};
    std::vector<float> ms;  // decode time per frame; written by the decode thread, read after join
};

// Feeds PCM to aplay. The decode thread only queues bytes; this thread does the blocking pipe
// writes, so a slow sound device can never stall the picture. If the queue grows past one
// second the oldest audio is dropped (and counted) instead of blocking.
class AudioOut {
public:
    ~AudioOut() { stop(); }

    // spawn == false: swallow the audio (for --dry on a machine without aplay).
    bool start(int bufferMs, bool spawn) {
        if (spawn) {
            int fds[2];
            if (pipe(fds) < 0) { perror("pipe"); return false; }
            pid_ = fork();
            if (pid_ < 0) { perror("fork"); close(fds[0]); close(fds[1]); return false; }
            if (pid_ == 0) {
                dup2(fds[0], 0);
                close(fds[0]);
                close(fds[1]);
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), "%d", bufferMs * 1000);
                execlp("aplay", "aplay", "-q", "-t", "raw", "-f", "S16_LE", "-r", "48000", "-c", "2",
                       "-B", buffer, "-", static_cast<char *>(nullptr));
                _exit(127);
            }
            close(fds[0]);
            fd_ = fds[1];
            std::signal(SIGPIPE, SIG_IGN);
        }
        running_ = true;
        thread_ = std::thread(&AudioOut::run, this);
        return true;
    }

    // volume256: 256 = unchanged, 128 = half.
    void push(const uint8_t *data, int frames, int volume256) {
        std::vector<uint8_t> chunk(size_t(frames) * 4);
        const int16_t *in = reinterpret_cast<const int16_t *>(data);
        int16_t *out = reinterpret_cast<int16_t *>(chunk.data());
        for (int i = 0; i < frames * 2; ++i) out[i] = int16_t((int(in[i]) * volume256) >> 8);
        std::lock_guard<std::mutex> lock(mutex_);
        queued_ += chunk.size();
        bytes_ += chunk.size();
        queue_.push_back(std::move(chunk));
        while (queued_ > kMaxQueued && queue_.size() > 1) {
            queued_ -= queue_.front().size();
            dropped_ += queue_.front().size();
            queue_.pop_front();
        }
        wake_.notify_all();
    }

    void stop() {
        if (!running_) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            wake_.notify_all();
        }
        if (pid_ > 0) kill(pid_, SIGTERM);  // stop at once, do not play out the buffer
        if (thread_.joinable()) thread_.join();
        if (fd_ >= 0) { close(fd_); fd_ = -1; }
        if (pid_ > 0) { int status = 0; waitpid(pid_, &status, 0); pid_ = -1; }
    }

    long long bytesQueued() const { return bytes_; }
    long long bytesWritten() const { return written_; }
    long long bytesDropped() const { return dropped_; }
    int writeFailures() const { return failures_; }

private:
    static constexpr size_t kMaxQueued = 48000 * 4;  // one second

    void run() {
        for (;;) {
            std::vector<uint8_t> chunk;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [&] { return !running_ || !queue_.empty(); });
                if (!running_) return;
                chunk = std::move(queue_.front());
                queue_.pop_front();
                queued_ -= chunk.size();
            }
            if (fd_ < 0) { written_ += (long long)chunk.size(); continue; }
            size_t offset = 0;
            while (offset < chunk.size()) {
                const ssize_t n = write(fd_, chunk.data() + offset, chunk.size() - offset);
                if (n < 0) {
                    if (errno == EINTR) continue;
                    ++failures_;
                    return;
                }
                offset += size_t(n);
                written_ += n;
            }
        }
    }

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::vector<uint8_t>> queue_;
    std::thread thread_;
    size_t queued_ = 0;
    std::atomic<long long> bytes_{0}, written_{0}, dropped_{0};
    std::atomic<int> failures_{0};
    bool running_ = false;
    int fd_ = -1;
    pid_t pid_ = -1;
};

void decodeLoop(VideoDecoder &decoder, Pool &pool, DecodeStats &stats, int niceValue) {
    if (niceValue > 0) setpriority(PRIO_PROCESS, static_cast<id_t>(0), niceValue);  // this thread (Linux)
    double offset = 0, lastPts = 0;
    int failures = 0;
    while (!g_stop) {
        int index = -1;
        {
            std::unique_lock<std::mutex> lock(pool.mutex);
            pool.changed.wait(lock, [&] {
                if (g_stop) return true;
                for (int i = 0; i < kPool; ++i) if (!pool.slots[i].ready) return true;
                return false;
            });
            if (g_stop) break;
            for (int i = 0; i < kPool; ++i) if (!pool.slots[i].ready) { index = i; break; }
        }
        double pts = 0;
        const double d0 = now();
        if (!decoder.next(pool.slots[index].pixels.data(), pts)) {
            // End of file: start over, keeping the clock running.
            if (++failures > 2) break;
            offset += lastPts + 1.0 / decoder.fps();
            lastPts = 0;
            decoder.rewind();
            ++stats.loops;
            continue;
        }
        failures = 0;
        lastPts = pts;
        ++stats.frames;
        stats.ms.push_back(float((now() - d0) * 1000.0));
        std::lock_guard<std::mutex> lock(pool.mutex);
        pool.slots[index].pts = pts + offset;
        pool.slots[index].ready = true;
        pool.firstReady = true;
        pool.changed.notify_all();
    }
    timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    stats.cpu = double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
}

}  // namespace

int main(int argc, char **argv) {
    std::string path, fbPath = "/dev/fb0";
    double seconds = 30.0;
    int x = -1, y = -1, niceValue = 5;
    bool dry = false, audio = false;
    bool timeline = false;
    int volumePercent = 25, bufferMs = 150, avOffsetMs = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--seconds" && i + 1 < argc) seconds = std::atof(argv[++i]);
        else if (a == "--x" && i + 1 < argc) x = std::atoi(argv[++i]);
        else if (a == "--y" && i + 1 < argc) y = std::atoi(argv[++i]);
        else if (a == "--fb" && i + 1 < argc) fbPath = argv[++i];
        else if (a == "--nice" && i + 1 < argc) niceValue = std::atoi(argv[++i]);
        else if (a == "--dry") dry = true;
        else if (a == "--audio") audio = true;
        else if (a == "--timeline") timeline = true;
        else if (a == "--volume" && i + 1 < argc) volumePercent = std::atoi(argv[++i]);
        else if (a == "--buffer-ms" && i + 1 < argc) bufferMs = std::atoi(argv[++i]);
        else if (a == "--av-offset-ms" && i + 1 < argc) avOffsetMs = std::atoi(argv[++i]);
        else if (a[0] != '-') path = a;
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }
    if (path.empty()) {
        std::fprintf(stderr, "usage: %s FILE.mp4 [--seconds N] [--x X --y Y] [--fb DEV] [--nice N] [--dry] [--audio] [--volume P] [--buffer-ms N] [--av-offset-ms N] [--timeline]\n", argv[0]);
        return 2;
    }
    if (seconds < 1.0) seconds = 1.0;
    if (seconds > 120.0) seconds = 120.0;
    if (volumePercent < 0) volumePercent = 0;
    if (volumePercent > 100) volumePercent = 100;
    if (bufferMs < 20) bufferMs = 20;
    if (bufferMs > 1000) bufferMs = 1000;

    av_log_set_level(AV_LOG_ERROR);
    VideoDecoder decoder;
    if (!decoder.open(path, DecodeVariant::Normal, audio)) {
        std::fprintf(stderr, "cannot use %s: %s\n", path.c_str(), decoder.error().c_str());
        return 1;
    }
    const int vw = decoder.width(), vh = decoder.height();
    std::printf("video: %s %s level %d, %dx%d, %.2f fps\n", decoder.codecName().c_str(),
                decoder.profile().c_str(), decoder.level(), vw, vh, decoder.fps());
    if (vw > kSlotW || vh > kSlotH) {
        std::fprintf(stderr, "refused: %dx%d does not fit the %dx%d slot and videos are never scaled\n",
                     vw, vh, kSlotW, kSlotH);
        return 3;
    }

    // Framebuffer (or a stand-in in memory for --dry).
    int fd = -1;
    uint8_t *fb = nullptr;
    size_t fbSize = 0;
    int screenW = 1920, screenH = 1080, stride = 1920 * 4;
    std::vector<uint8_t> fakeFb;
    if (dry) {
        fakeFb.assign(size_t(stride) * screenH, 0);
        fb = fakeFb.data();
    } else {
#ifdef __linux__
        fd = open(fbPath.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0) { perror(fbPath.c_str()); return 1; }
        fb_var_screeninfo var{};
        fb_fix_screeninfo fix{};
        if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
            perror("framebuffer info");
            return 1;
        }
        if (var.bits_per_pixel != 32 || var.red.offset != 16 || var.green.offset != 8 || var.blue.offset != 0) {
            std::fprintf(stderr, "unsupported framebuffer format (need 32 bpp, BGRA order)\n");
            return 1;
        }
        screenW = int(var.xres);
        screenH = int(var.yres);
        stride = int(fix.line_length);
        fbSize = size_t(stride) * var.yres;
        fb = static_cast<uint8_t *>(mmap(nullptr, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (fb == MAP_FAILED) { perror("mmap"); return 1; }
#else
        std::fprintf(stderr, "the framebuffer is only available on Linux; use --dry\n");
        return 1;
#endif
    }
    std::printf("screen: %dx%d, stride %d%s\n", screenW, screenH, stride, dry ? " (dry run, memory only)" : "");

    // Default position: centred in the right half of the content area, like the detail slot.
    const double k = double(screenH) / 1080.0;
    if (x < 0) x = int((973 + (875 - vw / k) / 2) * k);
    if (y < 0) y = int((128 + (848 - vh / k) / 2) * k);
    if (x < 0 || y < 0 || x + vw > screenW || y + vh > screenH) {
        std::fprintf(stderr, "the video at %d,%d does not fit the %dx%d screen\n", x, y, screenW, screenH);
        return 1;
    }
    std::printf("placed at %d,%d (original size, no scaling)\n", x, y);

    // Save what is under the video.
    std::vector<uint8_t> saved(size_t(vw) * vh * 4);
    for (int row = 0; row < vh; ++row)
        std::memcpy(&saved[size_t(row) * vw * 4], fb + size_t(y + row) * stride + size_t(x) * 4, size_t(vw) * 4);

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    // Sound. aplay is started before any other thread exists, so the fork is clean.
    AudioOut audioOut;
    bool audioOn = false;
    if (audio && !decoder.hasAudio()) std::printf("audio: the file has no usable audio track\n");
    if (audio && decoder.hasAudio()) {
        audioOn = audioOut.start(bufferMs, !dry);
        if (!audioOn) std::fprintf(stderr, "audio: could not start aplay, playing silent\n");
    }
    if (audioOn) {
        const int volume256 = volumePercent * 256 / 100;
        decoder.audioSink = [&audioOut, volume256](const uint8_t *data, int frames) {
            audioOut.push(data, frames, volume256);
        };
        std::printf("audio: on, volume %d%%, aplay buffer %d ms, picture offset %d ms\n", volumePercent,
                    bufferMs, avOffsetMs);
    }

    Pool pool;
    for (Slot &s : pool.slots) s.pixels.resize(size_t(vw) * vh * 4);
    DecodeStats dstats;
    dstats.ms.reserve(4000);
    std::thread worker(decodeLoop, std::ref(decoder), std::ref(pool), std::ref(dstats), niceValue);

    // Wait for the first frame; the clock starts there.
    {
        std::unique_lock<std::mutex> lock(pool.mutex);
        pool.changed.wait_for(lock, std::chrono::seconds(5), [&] { return pool.firstReady || g_stop.load(); });
    }

    struct SecStat { int shown = 0, dropped = 0, stalls = 0; double lateMax = 0; };
    std::vector<SecStat> secs(200);
    double lastShownAt = 0;
    long shown = 0, dropped = 0, ticks = 0;
    double copyTotal = 0, copyWorst = 0, lateTotal = 0, lateWorst = 0;
    const double wallStart = now(), cpuStart = cpuProcess();
    double t0 = now() + double(avOffsetMs) / 1000.0;
    double nextTick = now();
    std::printf("playing for up to %.0f s (signal or timeout stops it)\n", seconds);

    while (!g_stop) {
        const double t = now();
        if (t - wallStart >= seconds) break;
        const double elapsed = t - t0;

        const size_t sec = elapsed < 0 ? 0 : std::min(size_t(elapsed), secs.size() - 1);
        const long droppedBefore = dropped;
        int pick = -1;
        {
            std::lock_guard<std::mutex> lock(pool.mutex);
            // Newest frame whose time has come; older ready frames are dropped, never queued.
            for (int i = 0; i < kPool; ++i)
                if (pool.slots[i].ready && pool.slots[i].pts <= elapsed &&
                    (pick < 0 || pool.slots[i].pts > pool.slots[pick].pts)) pick = i;
            if (pick >= 0) {
                for (int i = 0; i < kPool; ++i)
                    if (i != pick && pool.slots[i].ready && pool.slots[i].pts < pool.slots[pick].pts) {
                        pool.slots[i].ready = false;
                        ++dropped;
                    }
            }
        }
        if (pick >= 0) {
            const double c0 = now();
            const uint8_t *src = pool.slots[pick].pixels.data();
            for (int row = 0; row < vh; ++row)
                std::memcpy(fb + size_t(y + row) * stride + size_t(x) * 4, src + size_t(row) * vw * 4, size_t(vw) * 4);
            const double ms = (now() - c0) * 1000.0;
            copyTotal += ms;
            if (ms > copyWorst) copyWorst = ms;
            const double late = (elapsed - pool.slots[pick].pts) * 1000.0;
            lateTotal += late;
            if (late > lateWorst) lateWorst = late;
            ++shown;
            ++secs[sec].shown;
            if (late > secs[sec].lateMax) secs[sec].lateMax = late;
            lastShownAt = elapsed;
            std::lock_guard<std::mutex> lock(pool.mutex);
            pool.slots[pick].ready = false;
            pool.changed.notify_all();
        } else {
            // No frame was due. A gap of more than ~2 frame times counts as a stall.
            if (elapsed > 0.2 && elapsed - lastShownAt > 0.075) ++secs[sec].stalls;
            std::lock_guard<std::mutex> lock(pool.mutex);
            pool.changed.notify_all();
        }
        secs[sec].dropped += int(dropped - droppedBefore);
        ++ticks;

        nextTick += kTickMs / 1000.0;
        const double sleepFor = nextTick - now();
        if (sleepFor > 0) std::this_thread::sleep_for(std::chrono::duration<double>(sleepFor));
        else nextTick = now();  // fell behind: do not try to catch up with a burst
    }

    const double wall = now() - wallStart;
    const double cpu = cpuProcess() - cpuStart;
    g_stop = true;
    pool.changed.notify_all();
    worker.join();
    audioOut.stop();

    // Put back what was under the video.
    for (int row = 0; row < vh; ++row)
        std::memcpy(fb + size_t(y + row) * stride + size_t(x) * 4, &saved[size_t(row) * vw * 4], size_t(vw) * 4);
    if (!dry) { munmap(fb, fbSize); close(fd); }

    if (timeline) {
        std::printf("\ntimeline (one line per second of playback)\n");
        std::printf("  sec  shown dropped stalls  late max ms  decode mean/worst ms\n");
        const double fps = decoder.fps();
        for (size_t i = 0; i < secs.size(); ++i) {
            const SecStat &st = secs[i];
            if (st.shown == 0 && st.dropped == 0 && st.stalls == 0) continue;
            double sum = 0, worst = 0;
            int n = 0;
            for (size_t f = size_t(double(i) * fps); f < size_t(double(i + 1) * fps) && f < dstats.ms.size(); ++f) {
                sum += dstats.ms[f];
                if (dstats.ms[f] > worst) worst = dstats.ms[f];
                ++n;
            }
            std::printf("  %3zu  %5d %7d %6d  %11.1f  %9.1f / %.1f\n", i, st.shown, st.dropped, st.stalls,
                        st.lateMax, n ? sum / n : 0.0, worst);
        }
    }

    std::printf("\nresult\n");
    std::printf("  wall time        : %.1f s, %ld ticks of %d ms\n", wall, ticks, kTickMs);
    std::printf("  frames shown     : %ld (%.1f per second)\n", shown, wall > 0 ? double(shown) / wall : 0.0);
    std::printf("  frames dropped   : %ld (decoded %ld, loops %ld)\n", dropped, dstats.frames.load(), dstats.loops.load());
    std::printf("  copy to screen   : mean %.2f ms, worst %.2f ms per frame\n", shown ? copyTotal / double(shown) : 0.0, copyWorst);
    std::printf("  lateness         : mean %.1f ms, worst %.1f ms behind the frame's time\n", shown ? lateTotal / double(shown) : 0.0, lateWorst);
    std::printf("  CPU              : process %.0f%% of one core, decode thread %.0f%%\n",
                wall > 0 ? 100.0 * cpu / wall : 0.0, wall > 0 ? 100.0 * dstats.cpu.load() / wall : 0.0);
    if (audioOn)
        std::printf("  audio            : %lld bytes queued, %lld written to aplay, %lld dropped, %d write failures\n",
                    audioOut.bytesQueued(), audioOut.bytesWritten(), audioOut.bytesDropped(),
                    audioOut.writeFailures());
    return 0;
}
