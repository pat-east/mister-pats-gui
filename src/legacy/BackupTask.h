#pragma once

#include <mutex>
#include <string>
#include <thread>

class BackupTask {
public:
    enum class State { Idle, Running, Complete, Failed };
    struct Snapshot { State state = State::Idle; std::string path; };

    ~BackupTask();
    bool start();
    Snapshot snapshot() const;

private:
    void run();
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    std::thread worker_;
};
