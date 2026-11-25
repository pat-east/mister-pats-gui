#include "TabIconDownload.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace {

const char *kRepositoryBase =
    "https://raw.githubusercontent.com/pat-east/mister-pats-gui/main/assets/tab-icons/";

bool ensureDirectory(const std::string &path) {
    if (mkdir(path.c_str(), 0755) == 0) return true;
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool safeAssetName(const std::string &name) {
    if (!TabIcons::knownFile(name)) return false;
    for (char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-'))
            return false;
    return true;
}

} // namespace

bool TabIconDownload::start() {
    if (running()) return false;
    error_.clear();
    next_ = 0;
    missing_ = icons_.missingFiles();
    if (!ensureDirectory(TabIcons::kDefaultDirectory)) {
        state_ = State::Failed;
        error_ = "cannot create the tab icon directory";
        return false;
    }
    if (missing_.empty()) {
        state_ = State::Done;
        return true;
    }

    const std::string manifest = std::string(TabIcons::kDefaultDirectory) + "/manifest.part";
    unlink(manifest.c_str());
    if (!downloader_.fetch(std::string(kRepositoryBase) + "manifest.tsv", manifest, 30)) {
        state_ = State::Failed;
        error_ = "could not download the tab icon manifest: " + downloader_.lastError();
        unlink(manifest.c_str());
        return false;
    }

    std::set<std::string> listed;
    std::ifstream input(manifest);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string filename = line.substr(0, tab);
        if (safeAssetName(filename)) listed.insert(filename);
    }
    unlink(manifest.c_str());
    if (!input.eof()) {
        state_ = State::Failed;
        error_ = "cannot read the tab icon manifest";
        return false;
    }

    std::vector<std::string> selected;
    for (const std::string &filename : missing_)
        if (listed.count(filename)) selected.push_back(filename);
    missing_.swap(selected);
    if (missing_.empty()) {
        state_ = State::Failed;
        error_ = "the manifest does not list the missing tab icons";
        return false;
    }

    state_ = State::Running;
    showStartedFrame_ = true;
    return true;
}

void TabIconDownload::step() {
    if (!running()) return;
    if (showStartedFrame_) {
        showStartedFrame_ = false;
        return;
    }
    if (next_ >= missing_.size()) {
        finish();
        return;
    }

    const std::string &name = missing_[next_++];
    const std::string destination = std::string(TabIcons::kDefaultDirectory) + "/" + name;
    const std::string temporary = destination + ".part";
    unlink(temporary.c_str());
    if (!downloader_.fetch(std::string(kRepositoryBase) + name, temporary, 30)) {
        error_ = "failed to download " + name + ": " + downloader_.lastError();
        unlink(temporary.c_str());
        state_ = State::Failed;
        return;
    }

    ImagePtr image = Image::load(temporary);
    if (!image || !image->valid() || image->width() != TabIcons::kIconSize ||
        image->height() != TabIcons::kIconSize || !image->opaque() ||
        std::rename(temporary.c_str(), destination.c_str()) != 0) {
        error_ = "downloaded tab icon is invalid: " + name;
        unlink(temporary.c_str());
        state_ = State::Failed;
        return;
    }

    if (next_ >= missing_.size()) finish();
}

void TabIconDownload::finish() {
    icons_.refresh();
    if (!icons_.allAvailable()) {
        state_ = State::Failed;
        error_ = "some tab icons are still missing";
    } else {
        state_ = State::Done;
    }
}

float TabIconDownload::progress() const {
    if (missing_.empty()) return 1.0f;
    return float(next_) / float(missing_.size());
}

std::string TabIconDownload::statusLine() const {
    if (state_ == State::Running)
        return std::to_string(next_) + "/" + std::to_string(missing_.size());
    if (state_ == State::Failed) return "Retry";
    if (state_ == State::Done) return "Current";
    const std::vector<std::string> missing = icons_.missingFiles();
    return missing.empty() ? "Current" : "Download " + std::to_string(missing.size());
}
