#include "ArcadeScan.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace {

constexpr size_t kBatchPerStep = 8;

// Directories *listed* per step while still discovering .mra files — bounded independently of
// kBatchPerStep, and deliberately small: a real library's _Arcade tree can hold hundreds of
// manufacturer/genre subfolders (see ArcadeScan.h), and listing all of them in one unbroken
// burst is exactly what blocked the UI for a noticeable moment on real hardware before this was
// paced — see ARCADE.md.
constexpr size_t kDirsPerStep = 6;

bool hasMraSuffix(const std::string &s) {
    const char *suffix = ".mra";
    const size_t n = std::strlen(suffix);
    return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, suffix) == 0;
}

} // namespace

void ArcadeScan::start(const std::vector<std::string> &roots,
                       const std::vector<std::string> &romSearchOrder) {
    state_ = State::Idle;
    pendingDirs_.clear();
    files_.clear();
    entries_.clear();
    position_ = 0;

    const std::vector<std::string> mounted = roots.empty() ? MraFile::mountedRoots() : roots;
    gameSearchRoots_ =
        romSearchOrder.empty() ? MraFile::misterGameSearchOrder(mounted) : romSearchOrder;

    for (const std::string &root : mounted) pendingDirs_.push_back(root + "/_Arcade");

    state_ = pendingDirs_.empty() ? State::Done : State::Discovering;
}

void ArcadeScan::discoverBatch() {
    for (size_t i = 0; i < kDirsPerStep && !pendingDirs_.empty(); ++i) {
        // Order among siblings does not matter — an explicit stack instead of recursion is
        // what makes this resumable one directory at a time across frames.
        const std::string dir = pendingDirs_.back();
        pendingDirs_.pop_back();

        DIR *d = opendir(dir.c_str());
        if (!d) continue;

        while (dirent *entry = readdir(d)) {
            const std::string name = entry->d_name;
            if (name.empty() || name[0] == '.') continue;

            const std::string full = dir + "/" + name;
            struct stat st {};
            if (stat(full.c_str(), &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                // cores holds large .rbf files this walk has no reason to even stat, media
                // holds artwork, and _Organized is the Arcade Organizer's tree of links to the
                // very same .mra files — walking it would list every game twice.
                if (name == "cores" || name == "media" || name == "_Organized") continue;
                pendingDirs_.push_back(full);
            } else if (S_ISREG(st.st_mode) && hasMraSuffix(name)) {
                files_.push_back(full);
            }
        }
        closedir(d);
    }
}

void ArcadeScan::step() {
    if (state_ == State::Discovering) {
        discoverBatch();
        if (pendingDirs_.empty()) state_ = files_.empty() ? State::Done : State::Scanning;
        return;
    }

    if (state_ != State::Scanning) return;

    for (size_t i = 0; i < kBatchPerStep && position_ < files_.size(); ++i, ++position_) {
        ArcadeEntry entry;
        if (MraFile::parse(files_[position_], entry)) {
            MraFile::resolveCore(entry);
            MraFile::resolveRom(entry, gameSearchRoots_);
            entries_.push_back(std::move(entry));
        }
    }

    if (position_ >= files_.size()) state_ = State::Done;
}

float ArcadeScan::progress() const {
    if (state_ == State::Discovering) return 0.0f;
    if (files_.empty()) return 1.0f;
    return float(position_) / float(files_.size());
}

std::string ArcadeScan::statusLine() const {
    char buffer[128];
    if (state_ == State::Discovering) {
        std::snprintf(buffer, sizeof(buffer), "Looking for .mra files… %zu found so far",
                      files_.size());
        return buffer;
    }
    std::snprintf(buffer, sizeof(buffer), "Checking %zu of %zu .mra files", position_,
                  files_.size());
    return buffer;
}
