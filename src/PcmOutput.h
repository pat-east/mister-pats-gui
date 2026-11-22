#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>

// Plays raw audio (signed 16-bit, stereo, 48 kHz) through `aplay` over a pipe, the same tool the
// interface sounds use, so no sound library is linked. A writer thread does the blocking pipe
// writes, so a slow sound device can never stall the picture. If more than a second of audio is
// waiting, the oldest is dropped instead of blocking. If aplay or a sound device is missing, it
// stays silent.
class PcmOutput {
public:
    PcmOutput() = default;
    ~PcmOutput();
    PcmOutput(const PcmOutput &) = delete;
    PcmOutput &operator=(const PcmOutput &) = delete;

    // `bufferMs` is aplay's buffer: small keeps sound and picture close together.
    bool start(int bufferMs);
    // volume256: 256 = unchanged, 64 = a quarter. `frames` is the number of sample frames.
    void push(const uint8_t *data, int frames, int volume256);
    // Stops at once (the rest of the buffer is not played) and waits for aplay to end.
    void stop();

private:
    static constexpr size_t kMaxQueuedBytes = 48000 * 4;   // one second
    void run();

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::vector<uint8_t>> queue_;
    std::thread thread_;
    size_t queued_ = 0;
    bool running_ = false;
    int fd_ = -1;
    pid_t pid_ = -1;
};
