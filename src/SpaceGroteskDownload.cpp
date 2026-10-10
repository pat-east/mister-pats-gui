#include "SpaceGroteskDownload.h"

#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "Paths.h"

namespace {

struct DownloadFile { const char *name; const char *url; bool font; };
const DownloadFile kFiles[] = {
    {"SpaceGrotesk-Bold.ttf", "fonts/ttf/static/SpaceGrotesk-Bold.ttf", true},
    {"SpaceGrotesk-Medium.ttf", "fonts/ttf/static/SpaceGrotesk-Medium.ttf", true},
    {"SpaceGrotesk-OFL.txt", "OFL.txt", false},
};

bool validTtf(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    unsigned char signature[4]{};
    if (!in.read(reinterpret_cast<char *>(signature), sizeof(signature))) return false;
    return signature[0] == 0x00 && signature[1] == 0x01 &&
           signature[2] == 0x00 && signature[3] == 0x00;
}

} // namespace

bool SpaceGroteskDownload::start() {
    if (running()) return false;
    missing_.clear();
    next_ = 0;
    error_.clear();
    const std::string directory = std::string(MISTER_PAT_ROOT) + "/fonts";
    if (mkdir(directory.c_str(), 0755) != 0) {
        struct stat info {};
        if (stat(directory.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) {
            state_ = State::Failed;
            error_ = "cannot create the font directory";
            return false;
        }
    }
    for (const DownloadFile &file : kFiles) {
        const std::string path = directory + "/" + file.name;
        if (file.font ? !validTtf(path) : access(path.c_str(), R_OK) != 0)
            missing_.push_back(file.name);
    }
    state_ = missing_.empty() ? State::Done : State::Running;
    showStartedFrame_ = running();
    return true;
}

void SpaceGroteskDownload::step() {
    if (!running()) return;
    if (showStartedFrame_) {
        showStartedFrame_ = false;
        return;
    }
    const std::string name = missing_[next_++];
    const std::string destination = std::string(MISTER_PAT_ROOT) + "/fonts/" + name;
    const std::string temporary = destination + ".part";
    std::string url = "https://raw.githubusercontent.com/floriankarsten/space-grotesk/2.0.0/";
    for (const DownloadFile &file : kFiles)
        if (name == file.name) { url += file.url; break; }
    bool valid = false;
    for (const DownloadFile &file : kFiles)
        if (name == file.name)
            valid = file.font ? (downloader_.fetch(url, temporary, 30) && validTtf(temporary))
                              : (downloader_.fetch(url, temporary, 30) && access(temporary.c_str(), F_OK) == 0);
    if (!valid || std::rename(temporary.c_str(), destination.c_str()) != 0) {
        error_ = "could not install " + name;
        if (!downloader_.lastError().empty()) error_ += ": " + downloader_.lastError();
        unlink(temporary.c_str());
        state_ = State::Failed;
        return;
    }
    if (next_ >= missing_.size()) state_ = State::Done;
}

std::string SpaceGroteskDownload::status() const {
    if (state_ == State::Running)
        return "Downloading " + std::to_string(next_) + "/" +
               std::to_string(missing_.size());
    if (state_ == State::Failed) return "Retry";
    if (state_ == State::Done) return "Installed";
    return std::string();
}
