#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "DiscDriveService.h"
#include "Downloader.h"
#include "TheGamesDBClient.h"

// Fetches artwork for a recognized physical disc on a worker and keeps it on the SD card.
// Existing artwork is returned from disk before any network request is attempted.
class DiscArtworkService {
public:
    enum class State { Idle, Searching, Ready, Missing };
    struct Snapshot {
        State state = State::Idle;
        std::string cacheKey;
        std::string boxart;
        std::string releaseYear;
        std::string players;
        std::string rating;
        std::string coop;
        std::string overview;
        std::vector<std::string> screenshots;
    };

    explicit DiscArtworkService(TheGamesDBClient &client);
    ~DiscArtworkService();
    DiscArtworkService(const DiscArtworkService &) = delete;
    DiscArtworkService &operator=(const DiscArtworkService &) = delete;

    void request(const DiscDriveService::Snapshot &disc);
    void refresh(const DiscDriveService::Snapshot &disc);
    void reload(const DiscDriveService::Snapshot &disc);
    Snapshot snapshot() const;

private:
    struct Request {
        uint64_t generation = 0;
        DiscDriveService::System system = DiscDriveService::System::Unknown;
        std::string title;
        std::string identifier;
        std::string region;
        bool clearCache = false;
    };
    void workerLoop();
    Snapshot fetch(const Request &request, const std::atomic<bool> &cancel);
    bool fetchFromTheGamesDB(const Request &request, Downloader &downloader,
                             Snapshot &out, const std::string &gameDir,
                             const std::atomic<bool> &cancel);

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    Snapshot snapshot_;
    Request pending_;
    uint64_t activeGeneration_ = 0;
    bool hasPending_ = false;
    bool stopping_ = false;
    std::chrono::steady_clock::time_point retryAt_{};
    std::atomic<bool> cancelCurrent_{false};
    TheGamesDBClient &client_;
    std::thread worker_;
};
