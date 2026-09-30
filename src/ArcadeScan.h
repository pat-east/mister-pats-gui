#pragma once

#include <string>
#include <vector>

#include "MraFile.h"

// Builds the Arcade diagnostic list by walking every mounted volume's _Arcade directory.
//
// Runs in steps rather than one pass: discovering the file list is cheap (metadata-only
// directory listings), but resolving each entry can mean opening a zip's central directory,
// and doing that for a full arcade set (a thousand-plus .mra files) in one unbroken burst is
// exactly the kind of flat-out drive access this project's own LibraryScan avoids elsewhere.
class ArcadeScan {
public:
    enum class State { Idle, Discovering, Scanning, Done };

    // Roots are found automatically when none are given. The order ROM zips are searched in is
    // MiSTer's own (MraFile::misterGameSearchOrder: USB first, the SD card last) — never the
    // order of `roots`, which is the wrong one for this and was once a real bug. Only a test,
    // whose scratch folders are not MiSTer's real mount points, passes `romSearchOrder` to
    // say otherwise.
    void start(const std::vector<std::string> &roots = {},
               const std::vector<std::string> &romSearchOrder = {});

    // One unit of work — while still finding files, a small batch of directories listed; once
    // that is done, a small batch of .mra files, each fully resolved. Call until finished().
    void step();

    State state() const { return state_; }
    bool running() const { return state_ == State::Discovering || state_ == State::Scanning; }
    bool finished() const { return state_ == State::Done; }

    // 0 while still finding files — the total is not known yet, and a bar that jumps from 0%
    // straight to some real fraction once discovery ends is honester than a fake estimate.
    float progress() const;
    std::string statusLine() const; // what to show while it works

    const std::vector<ArcadeEntry> &entries() const { return entries_; }
    size_t total() const { return files_.size(); }
    size_t done() const { return position_; }

private:
    void discoverBatch();

    State state_ = State::Idle;
    std::vector<std::string> pendingDirs_;  // still to list, discovery's own work queue
    std::vector<std::string> files_;
    std::vector<std::string> gameSearchRoots_;
    size_t position_ = 0;
    std::vector<ArcadeEntry> entries_;
};
