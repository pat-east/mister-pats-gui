#include "BackupTask.h"

#include <cstdio>
#include <utility>

BackupTask::~BackupTask() {
    if (worker_.joinable()) worker_.join();
}

bool BackupTask::start() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (snapshot_.state == State::Running) return false;
    if (worker_.joinable()) worker_.join();
    snapshot_ = {State::Running, {}};
    worker_ = std::thread(&BackupTask::run, this);
    return true;
}

BackupTask::Snapshot BackupTask::snapshot() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return snapshot_;
}

void BackupTask::run() {
    FILE *process = popen("/media/fat/mister-pat/tools/backup-mister-pat.sh", "r");
    char output[512]{};
    if (process) std::fgets(output, sizeof(output), process);
    const int result = process ? pclose(process) : -1;
    const std::string line(output);
    Snapshot completed;
    if (result == 0 && line.compare(0, 12, "BACKUP_PATH=") == 0) {
        completed.state = State::Complete;
        completed.path = line.substr(12);
        while (!completed.path.empty() &&
               (completed.path.back() == '\n' || completed.path.back() == '\r'))
            completed.path.pop_back();
    } else {
        completed.state = State::Failed;
    }
    std::lock_guard<std::mutex> guard(mutex_);
    snapshot_ = std::move(completed);
}
