#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "VerifiedDownloader.h"

enum class CoreMraType { ArcadeCore, ConsoleCore, ArcadeMra };
enum class CoreMraStatus { UpToDate, Missing, UpdateAvailable, Modified, Error };
enum class CoreMraPhase { Idle, Checking, Ready, Installing, Complete, Failed };

struct CoreMraEntry {
    std::string path;
    std::string name;
    std::string url;
    std::string hash;
    std::vector<std::string> replacements;
    uint64_t size = 0;
    CoreMraType type = CoreMraType::ArcadeCore;
    CoreMraStatus status = CoreMraStatus::Missing;
    bool selected = false;
};

// One line of the live list shown while the check runs.
struct CoreMraRecent {
    std::string name;
    CoreMraStatus status = CoreMraStatus::UpToDate;
};

struct CoreMraSnapshot {
    CoreMraPhase phase = CoreMraPhase::Idle;
    std::vector<CoreMraEntry> entries;
    size_t completed = 0;
    size_t totalFiles = 0;
    size_t current = 0;
    uint64_t currentBytes = 0;
    uint64_t currentTotal = 0;
    size_t installed = 0;
    size_t failed = 0;
    std::string message;
    std::string detail;
    std::vector<CoreMraRecent> recent; // last files checked, oldest first, at most 40
    int64_t checkedAt = 0;             // when the list and file results were obtained (epoch seconds)
    bool fromCache = false;            // the check reused a saved result (see docs/UPDATER.md)
};

class CoreMraUpdater {
public:
    CoreMraUpdater() = default;
    ~CoreMraUpdater();
    CoreMraUpdater(const CoreMraUpdater &) = delete;
    CoreMraUpdater &operator=(const CoreMraUpdater &) = delete;

    // `fresh` skips the saved result and asks the official source again.
    bool startCheck(bool fresh = false);
    bool startInstall();
    void cancelCheck();
    void selectAllMissingOrOutdated();
    bool toggleSelected(size_t index);
    CoreMraSnapshot snapshot() const;
    bool busy() const;

private:
    void checkWorker();
    void installWorker(std::vector<CoreMraEntry> selected);
    void joinFinished();

    mutable std::mutex mutex_;
    CoreMraSnapshot state_;
    std::thread worker_;
    std::atomic<bool> cancel_{false};
    bool workerRunning_ = false;
    bool forceFresh_ = false;
};
