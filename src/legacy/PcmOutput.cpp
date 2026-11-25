#include "PcmOutput.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

PcmOutput::~PcmOutput() { stop(); }

bool PcmOutput::start(int bufferMs) {
    stop();
    int fds[2];
    if (pipe(fds) != 0) return false;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[0], 0);
    posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);

    const std::string buffer = std::to_string(std::max(20, std::min(1000, bufferMs)) * 1000);
    std::vector<std::string> args = {"aplay", "-q", "-t", "raw", "-f", "S16_LE", "-r", "48000",
                                     "-c", "2", "-B", buffer, "-"};
    std::vector<char *> argv;
    for (std::string &a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);

    pid_t child = 0;
    const int spawned = posix_spawnp(&child, "aplay", &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[0]);
    if (spawned != 0) {
        close(fds[1]);
        return false;
    }
    signal(SIGPIPE, SIG_IGN);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        fd_ = fds[1];
        pid_ = child;
        running_ = true;
        queue_.clear();
        queued_ = 0;
    }
    try {
        thread_ = std::thread([this] { run(); });
    } catch (...) {
        stop();
        return false;
    }
    return true;
}

void PcmOutput::push(const uint8_t *data, int frames, int volume256) {
    if (frames <= 0) return;
    std::vector<uint8_t> chunk(size_t(frames) * 4);
    const int16_t *in = reinterpret_cast<const int16_t *>(data);
    int16_t *out = reinterpret_cast<int16_t *>(chunk.data());
    for (int i = 0; i < frames * 2; ++i) out[i] = int16_t((int(in[i]) * volume256) >> 8);
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return;
    queued_ += chunk.size();
    queue_.push_back(std::move(chunk));
    while (queued_ > kMaxQueuedBytes && queue_.size() > 1) {
        queued_ -= queue_.front().size();
        queue_.pop_front();
    }
    wake_.notify_all();
}

void PcmOutput::stop() {
    pid_t child;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && pid_ < 0) return;
        running_ = false;
        child = pid_;
        wake_.notify_all();
    }
    if (child > 0) kill(child, SIGTERM);   // stop at once; the buffer is not played out
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
    if (pid_ > 0) {
        int status = 0;
        waitpid(pid_, &status, 0);
        pid_ = -1;
    }
    queue_.clear();
    queued_ = 0;
}

void PcmOutput::run() {
    for (;;) {
        std::vector<uint8_t> chunk;
        int fd;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return !running_ || !queue_.empty(); });
            if (!running_) return;
            chunk = std::move(queue_.front());
            queue_.pop_front();
            queued_ -= chunk.size();
            fd = fd_;
        }
        size_t offset = 0;
        while (offset < chunk.size()) {
            const ssize_t n = write(fd, chunk.data() + offset, chunk.size() - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return;   // aplay ended (no device, or stopped): stay silent
            offset += size_t(n);
        }
    }
}
