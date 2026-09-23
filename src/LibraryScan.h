#pragma once

#include <string>
#include <vector>

#include "CoreIndex.h"
#include "GameDatabase.h"
#include "SystemCatalog.h"

// Builds the game database by walking the drives.
//
// Runs in steps rather than in one pass, and the steps are coarse on purpose: one system per
// call. That keeps the interface responsive and, more importantly, leaves gaps between reads.
// Walking a large library flat out is precisely what pulls a marginally powered USB drive off
// the bus — the reason the whole index exists in the first place.
class LibraryScan {
public:
    enum class State { Idle, Discovering, Scanning, Writing, Done, Failed };

    // The database directory is a parameter so a scan can be run against a scratch folder
    // during testing; the application uses the default.
    explicit LibraryScan(const std::string &databaseDirectory = GameDatabase::kDirectory)
        : database_(databaseDirectory) {}

    // Roots are found automatically when none are given.
    void start(const std::vector<std::string> &roots = {});

    // One unit of work. Call until `finished()`.
    void step();

    void cancel();

    State state() const { return state_; }
    bool running() const { return state_ == State::Discovering || state_ == State::Scanning; }
    bool finished() const { return state_ == State::Done || state_ == State::Failed; }

    float progress() const;          // 0 … 1
    std::string statusLine() const;  // what to show while it works

    size_t systemsFound() const { return written_; }
    size_t gamesFound() const { return games_; }
    const std::string &error() const { return error_; }

    // The volumes worth looking at: the SD card plus whatever USB volumes are mounted.
    static std::vector<std::string> detectRoots() { return GameDatabase::mountPoints(); }

private:
    std::vector<std::string> scanOne(const CatalogEntry &system) const;
    void fail(std::string message);

    State state_ = State::Idle;
    GameDatabase database_;
    CoreIndex cores_;
    std::vector<std::string> roots_;
    std::vector<CatalogEntry> queue_;
    size_t position_ = 0;
    size_t written_ = 0;
    size_t games_ = 0;
    std::string current_;
    std::string error_;
};
