#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ReleaseInfo.h"
#include "UpdateInstaller.h"
#include "VerifiedDownloader.h"

enum class UpdatePhase {
    Idle, WaitingForInternet, Checking, Current, Available, Blocked, Confirming,
    Downloading, Verifying, Installing, RestartRequired, Error
};

struct UpdateSnapshot {
    UpdatePhase phase = UpdatePhase::Idle;
    std::string targetVersion;
    std::string releaseNotes;
    std::string statusText;
    std::string errorText;
    std::string runningVersion;
    std::string selectedVersion;
    std::string newestInstalledVersion;
    bool pinned = false;
    std::vector<std::string> repairPaths;
    uint64_t bytesDone = 0;
    uint64_t bytesTotal = 0;
    bool databaseRebuildHint = false;
    bool canInstall = false;
};

class UpdateService {
public:
    UpdateService();
    ~UpdateService();
    UpdateService(const UpdateService &) = delete;
    UpdateService &operator=(const UpdateService &) = delete;

    void initializeLocalState();
    bool startCheck(bool automatic);
    void cancelAutomaticCheck();
    bool beginConfirmation();
    bool startInstall();
    void cancelBeforeCommit();
    UpdateSnapshot snapshot() const;
    bool commitRunning() const;
    void shutdown();

private:
    enum class Command { None, Check, AutomaticCheck, Install };
    void workerLoop();
    void check(bool automatic);
    void install();
    void publishError(const std::string &error);
    std::string localBlockReason() const;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    std::atomic<bool> cancel_{false};
    bool stopping_ = false;
    bool automaticCheckActive_ = false;
    Command pending_ = Command::None;
    UpdateSnapshot snapshot_;
    ReleaseCandidate candidate_;
    VerifiedDownloader downloader_;
};
