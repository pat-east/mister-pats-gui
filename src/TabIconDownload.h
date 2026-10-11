#pragma once

#include <string>
#include <vector>

#include "Downloader.h"
#include "TabIcons.h"

// Fetches only the selected tab-icon assets, one per frame, and validates each BMP before
// atomically installing it under MISTER_PAT_ROOT/tab-icons.
class TabIconDownload {
public:
    enum class State { Idle, Running, Done, Failed };

    explicit TabIconDownload(TabIcons &icons) : icons_(icons) {}
    bool start();
    void step();
    bool running() const { return state_ == State::Running; }
    State state() const { return state_; }
    float progress() const;
    std::string statusLine() const;
    const std::string &error() const { return error_; }

private:
    void finish();

    TabIcons &icons_;
    Downloader downloader_;
    std::vector<std::string> missing_;
    size_t next_ = 0;
    State state_ = State::Idle;
    bool showStartedFrame_ = false;
    std::string error_;
};
