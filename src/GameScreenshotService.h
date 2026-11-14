#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "TheGamesDBClient.h"

// Looks up screenshots only for the game whose options panel was opened.
// Network and cache work stay on one worker, never in the input or render path.
class GameScreenshotService {
public:
    enum class State { Idle, Searching, Ready, Missing };
    struct Snapshot {
        State state = State::Idle;
        std::string key;
        std::vector<std::string> screenshots;
    };

    explicit GameScreenshotService(TheGamesDBClient &client);
    ~GameScreenshotService();
    GameScreenshotService(const GameScreenshotService &) = delete;
    GameScreenshotService &operator=(const GameScreenshotService &) = delete;

    void request(const std::string &systemKey, const std::string &title,
                 const std::vector<std::string> &localCandidates);
    Snapshot snapshot() const;

private:
    struct Request {
        std::string key, systemKey, title;
        std::vector<std::string> localCandidates;
    };
    void workerLoop();
    Snapshot fetch(const Request &request);

    TheGamesDBClient &client_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    Request pending_;
    Snapshot snapshot_;
    std::map<std::string, std::string> platformIds_;
    std::atomic<bool> cancel_{false};
    bool hasPending_ = false;
    bool stopping_ = false;
    std::chrono::steady_clock::time_point retryAt_{};
};
