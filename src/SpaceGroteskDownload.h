#pragma once

#include <string>
#include <vector>

#include "Downloader.h"

class SpaceGroteskDownload {
public:
    enum class State { Idle, Running, Done, Failed };

    bool start();
    void step();
    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    std::string status() const;
    const std::string &error() const { return error_; }

private:
    Downloader downloader_;
    std::vector<std::string> missing_;
    size_t next_ = 0;
    bool showStartedFrame_ = false;
    State state_ = State::Idle;
    std::string error_;
};
