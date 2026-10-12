#pragma once

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class DiscCoreInstaller {
public:
    enum class State { Idle, Downloading, Verifying, Installing, Ready, Failed, Cancelled };
    struct Snapshot {
        State state = State::Idle;
        std::string message;
        uint64_t bytes = 0;
    };

    DiscCoreInstaller();
    ~DiscCoreInstaller();
    DiscCoreInstaller(const DiscCoreInstaller &) = delete;
    DiscCoreInstaller &operator=(const DiscCoreInstaller &) = delete;

    bool start();
    void cancel();
    Snapshot snapshot() const;
    bool busy() const;

private:
    void workerLoop();
    bool installPackage(std::string &error);

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    Snapshot snapshot_;
    std::atomic<bool> cancel_{false};
    bool pending_ = false;
    bool stopping_ = false;
    std::thread worker_;
};
