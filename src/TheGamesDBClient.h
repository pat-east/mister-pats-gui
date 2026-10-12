#pragma once

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
    std::condition_variable wake_;
    CheckState state_ = CheckState::NotChecked;
    bool pending_ = false;
    bool stopping_ = false;
    std::thread worker_;
};
