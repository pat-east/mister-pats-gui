#include "SystemIconDownload.h"

#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#include "Image.h"

namespace {

const char *kRepositoryBase =
    "https://raw.githubusercontent.com/pat-east/mister-pats-gui/"
    "2fcc05a0695326e25451d1e66c8db08f4d20e928/assets/icons/";

bool isRegularFile(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

} // namespace

bool SystemIconDownload::start() {
    if (running()) return false;

    names_ = icons_.missingBitmaps();
    next_ = 0;
    downloaded_ = 0;
    failed_ = 0;
    error_.clear();

    const std::string directory = Icons::kDefaultDirectory;
    if (mkdir(directory.c_str(), 0755) != 0) {
        struct stat st {};
        if (stat(directory.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            state_ = State::Failed;
            error_ = "cannot create the system icon directory";
            return false;
        }
    }

    state_ = names_.empty() ? State::Done : State::Running;
    showStartedFrame_ = state_ == State::Running;
    return true;
}

void SystemIconDownload::step() {
    if (!running()) return;
    if (showStartedFrame_) {
        showStartedFrame_ = false;
        return;
    }
    if (next_ >= names_.size()) {
        finish();
        return;
    }

    const std::string &name = names_[next_++];
    const std::string destination = std::string(Icons::kDefaultDirectory) + "/" + name + ".bmp";
    const std::string temporary = destination + ".part";

    // Another process or a previous partial run may already have installed this icon.
    if (isRegularFile(destination)) {
        if (next_ >= names_.size()) finish();
        return;
    }

    unlink(temporary.c_str());
    const std::string url = std::string(kRepositoryBase) + name + ".bmp";
    if (!downloader_.fetch(url, temporary, 30)) {
        ++failed_;
        if (error_.empty()) error_ = "failed to download " + name + ": " + downloader_.lastError();
        unlink(temporary.c_str());
        next_ = names_.size();
    } else {
        ImagePtr image = Image::load(temporary);
        if (!image || !image->valid()) {
            ++failed_;
            if (error_.empty()) error_ = "downloaded icon is invalid: " + name;
            unlink(temporary.c_str());
            next_ = names_.size();
        } else if (std::rename(temporary.c_str(), destination.c_str()) != 0) {
            ++failed_;
            if (error_.empty()) error_ = "cannot install icon: " + name;
            unlink(temporary.c_str());
            next_ = names_.size();
        } else {
            ++downloaded_;
        }
    }

    if (next_ >= names_.size()) finish();
}

void SystemIconDownload::finish() {
    icons_.refresh();
    state_ = failed_ == 0 ? State::Done : State::Failed;
}

float SystemIconDownload::progress() const {
    if (names_.empty()) return 1.0f;
    return float(next_) / float(names_.size());
}

std::string SystemIconDownload::statusLine() const {
    if (state_ == State::Idle) return std::string();
    if (state_ == State::Running)
        return std::to_string(next_) + "/" + std::to_string(names_.size());
    if (state_ == State::Failed) return "Retry";
    return downloaded_ == 0 ? "Current" : "Done";
}
