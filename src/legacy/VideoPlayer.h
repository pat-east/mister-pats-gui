#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Plays an MP4 preview video (H.264) without sound, frame by frame, for the List view.
//
// A decode thread opens the file, reads it and converts each frame to the canvas pixel format at
// the video's own size, into a small pool of buffers. The screen asks for the newest frame whose
// time has come once per frame it draws, copies it and gives it back. A video that fits is played at its
// own size; a larger one is shrunk to fit, keeping its shape, during the conversion. The decode thread runs at a
// low priority so input and drawing always win. See _vnext/SCREENSCRAPER-POC.md for the
// measurements this design comes from.
class VideoPlayer {
public:
    enum class State {
        Idle,      // nothing started
        Opening,   // the thread is reading the file's header
        Playing,   // size is known, frames are coming
        Failed,    // could not be played; error() says why
        Finished,  // all plays are done and the last frame has been handed out
    };

    VideoPlayer() = default;
    ~VideoPlayer();
    VideoPlayer(const VideoPlayer &) = delete;
    VideoPlayer &operator=(const VideoPlayer &) = delete;

    // Starts opening and decoding in the background and returns at once. The file may be any
    // length; it is played `maxPlays` times. A video larger than maxWidth x maxHeight is shrunk to fit.
    // With `withSound` the audio track is decoded too and played through PcmOutput (aplay) at
    // `volumePercent`; a file without audio, or a missing sound device, simply stays silent.
    void start(const std::string &path, int maxWidth, int maxHeight, int maxPlays,
               bool withSound = false, int volumePercent = 25);
    // Stops and joins the decode thread and frees everything. Returns within a frame or two.
    void stop();

    State state() const;
    std::string error() const;
    // Valid once state() is Playing (or Finished after playing).
    int width() const;
    int height() const;

    // The newest frame that is due, as width() x height() pixels in ARGB32 with no padding; or
    // null when there is nothing new. The pointer stays valid until release().
    const uint32_t *acquire();
    void release();

private:
    struct Slot {
        std::vector<uint32_t> pixels;
        double pts = 0.0;
        bool ready = false;
        bool showing = false;
    };
    static constexpr int kPool = 3;

    void decodeLoop(std::string path, int maxWidth, int maxHeight, int plays, bool withSound,
                    int volumePercent);
    void fail(const std::string &message);

    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    Slot slots_[kPool];
    std::atomic<bool> stop_{false};
    State state_ = State::Idle;
    std::string error_;
    int width_ = 0;
    int height_ = 0;
    bool decodeDone_ = false;     // the decode thread has produced its last frame
    bool clockRunning_ = false;   // the first frame has been shown
    std::chrono::steady_clock::time_point start_{};
};
