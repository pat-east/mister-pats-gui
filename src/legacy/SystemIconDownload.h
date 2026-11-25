#pragma once

#include <string>
#include <vector>

#include "Downloader.h"
#include "Icons.h"

// Downloads the repository's small system icons one at a time on the UI thread. Each file is
// written beside its destination and renamed only after it has decoded successfully, so an
// interrupted transfer can never replace a working icon with a partial BMP.
class SystemIconDownload {
public:
    enum class State { Idle, Running, Done, Failed };

    explicit SystemIconDownload(Icons &icons) : icons_(icons) {}

    bool start();
    void step();

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    float progress() const;
    std::string statusLine() const;
    const std::string &error() const { return error_; }

private:
    void finish();

    Icons &icons_;
    Downloader downloader_;
    std::vector<std::string> names_;
    size_t next_ = 0;
    size_t downloaded_ = 0;
    size_t failed_ = 0;
    bool showStartedFrame_ = false;
    State state_ = State::Idle;
    std::string error_;
};
