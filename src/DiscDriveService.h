#pragma once

#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

// Polls Linux optical drives outside the frame loop and reads the small amount of
// system-specific metadata needed to identify a PS1 or Saturn disc.
class DiscDriveService {
public:
    enum class MediaState { NoDrive, NoDisc, Checking, Readable, Unsupported, Unreadable };
    enum class System { Unknown, PlayStation, Saturn };

    struct Snapshot {
        MediaState state = MediaState::NoDrive;
        System system = System::Unknown;
        std::string device;
        std::string model;
        std::string title;
        std::string identifier;
        std::string error;
        uint64_t mediaFingerprint = 0;
        uint64_t generation = 0;
    };

    DiscDriveService();
    ~DiscDriveService();

    DiscDriveService(const DiscDriveService &) = delete;
    DiscDriveService &operator=(const DiscDriveService &) = delete;

    void start();
    void requestProbe();
    Snapshot snapshot() const;

    static const char *stateName(MediaState state);
    static const char *systemName(System system);
    static int runProbeRequest(int inputFd, int outputFd);
    static Snapshot probeOnce(const Snapshot &previous, bool forceMetadataRead = false);

private:
    void workerLoop();
    Snapshot probeWithTimeout(const Snapshot &previous, bool forceMetadataRead);

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    Snapshot snapshot_;
    bool stopping_ = false;
    bool probeActive_ = false;
    bool probeRequested_ = false;
    bool forceMetadataRead_ = false;
    std::chrono::steady_clock::time_point probeStarted_{};
    int outstandingProbePid_ = -1;
    std::thread worker_;
};
