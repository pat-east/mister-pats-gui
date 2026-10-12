#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class DiscArtworkService;

class TheGamesDBClient {
public:
    enum class CheckState { NotChecked, Checking, Success, InvalidOrLimited, NetworkError, InvalidResponse };

    TheGamesDBClient();
    ~TheGamesDBClient();
    TheGamesDBClient(const TheGamesDBClient &) = delete;
    TheGamesDBClient &operator=(const TheGamesDBClient &) = delete;

    bool configured() const;
    bool startApiKeyCheck();
    bool checking() const;
    CheckState checkState() const;
    std::string checkStatus() const;
    static const char *settingsPath();

private:
    friend class DiscArtworkService;
    std::string readKey() const;
    void ensureSettingsFile() const;
    void workerLoop();

    mutable std::mutex mutex_;
    mutable std::mutex keyMutex_;
    mutable std::string cachedKey_;
    mutable std::chrono::steady_clock::time_point keyCacheCheckedAt_{};
    mutable bool keyCacheInitialized_ = false;
    std::condition_variable wake_;
    CheckState state_ = CheckState::NotChecked;
    std::string activeKey_;
    std::string checkedKey_;
    bool pending_ = false;
    bool stopping_ = false;
    std::atomic<bool> cancelCurrent_{false};
    std::thread worker_;
};
