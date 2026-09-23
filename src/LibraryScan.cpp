#include "LibraryScan.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "DebugLog.h"

namespace {

bool isDirectory(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace

void LibraryScan::start(const std::vector<std::string> &roots) {
    roots_ = roots.empty() ? detectRoots() : roots;
    queue_.clear();
    position_ = written_ = games_ = 0;
    current_.clear();
    error_.clear();
    state_ = State::Discovering;
}

void LibraryScan::cancel() {
    if (running()) {
        error_ = "cancelled";
        state_ = State::Failed;
    }
}

float LibraryScan::progress() const {
    if (state_ == State::Done) return 1.0f;
    if (queue_.empty()) return 0.0f;
    return float(position_) / float(queue_.size());
}

std::string LibraryScan::statusLine() const {
    char buffer[160];

    switch (state_) {
    case State::Idle:
        return "not started";
    case State::Discovering:
        return "Looking for systems on " + std::to_string(roots_.size()) + " volume(s) …";
    case State::Scanning:
        std::snprintf(buffer, sizeof(buffer), "%zu / %zu  ·  %s  ·  %zu games so far",
                      position_, queue_.size(), current_.c_str(), games_);
        return buffer;
    case State::Writing:
        return "Writing the catalogue …";
    case State::Done:
        std::snprintf(buffer, sizeof(buffer), "%zu systems, %zu games", written_, games_);
        return buffer;
    case State::Failed:
    default:
        return error_.empty() ? "failed" : error_;
    }
}

std::vector<std::string> LibraryScan::scanOne(const CatalogEntry &system) const {
    std::vector<std::string> games;
    std::vector<std::string> subdirectories;

    DIR *d = opendir(system.dir.c_str());
    if (!d) return games;

    while (dirent *entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name.empty() || name[0] == '.') continue;

        const std::string full = system.dir + "/" + name;

        if (isDirectory(full)) {
            if (SystemCatalog::isIgnoredDirectory(name)) continue;

            // A CD game is a folder of tracks, so the folder is the game. For everything
            // else a folder is just how someone chose to organise their ROMs.
            if (system.discBased) games.push_back(full);
            else subdirectories.push_back(full);
            continue;
        }

        if (SystemCatalog::looksLikeGame(name, system.extensions)) games.push_back(full);
    }
    closedir(d);

    // One level down, and no further. Libraries are sometimes sorted into A/B/C folders;
    // they are never nested deeply, and unbounded recursion on a large drive is exactly the
    // access pattern to avoid.
    for (const std::string &sub : subdirectories) {
        DIR *inner = opendir(sub.c_str());
        if (!inner) continue;

        while (dirent *entry = readdir(inner)) {
            const std::string name = entry->d_name;
            if (name.empty() || name[0] == '.') continue;

            const std::string full = sub + "/" + name;
            if (isDirectory(full)) continue;
            if (SystemCatalog::looksLikeGame(name, system.extensions)) games.push_back(full);
        }
        closedir(inner);
    }

    std::sort(games.begin(), games.end(), [](const std::string &a, const std::string &b) {
        const size_t sa = a.find_last_of('/'), sb = b.find_last_of('/');
        return strcasecmp(a.c_str() + (sa == std::string::npos ? 0 : sa + 1),
                          b.c_str() + (sb == std::string::npos ? 0 : sb + 1)) < 0;
    });

    return games;
}

void LibraryScan::fail(std::string message) {
    error_ = std::move(message);
    DebugLog::warn("scan: " + error_);
    state_ = State::Failed;
}

void LibraryScan::step() {
    switch (state_) {
    case State::Discovering: {
        if (roots_.empty()) {
            fail("no volumes found");
            return;
        }

        cores_.scan(roots_);
        if (cores_.empty()) {
            fail("no cores found — is this a MiSTer?");
            return;
        }

        queue_ = SystemCatalog::discover(roots_, cores_);
        if (queue_.empty()) {
            fail("no game directories found next to an installed core");
            return;
        }

        if (!database_.beginWrite(roots_)) {
            fail(database_.lastError());
            return;
        }

        std::printf("scan: %zu candidate systems on %zu volumes\n", queue_.size(),
                    roots_.size());
        state_ = State::Scanning;
        return;
    }

    case State::Scanning: {
        // Piggybacks on a phase that already takes a while and already moves one step per
        // frame, so a few extra filesystem operations here are the last place anyone notices
        // them. This is what clears out whatever the *previous* rebuild's finishWrite() swap
        // left behind — see GameDatabase::pruneOldGenerationStep().
        database_.pruneOldGenerationStep();

        if (position_ >= queue_.size()) {
            state_ = State::Writing;
            return;
        }

        const CatalogEntry &system = queue_[position_];
        current_ = system.name;

        const std::vector<std::string> games = scanOne(system);
        if (!games.empty()) {
            DatabaseSystem record;
            record.key = system.key;
            record.name = system.name;
            record.group = system.group;
            record.core = system.core;
            record.dir = system.dir;
            record.discBased = system.discBased;

            if (!database_.writeSystem(record, games)) {
                fail(database_.lastError());
                return;
            }

            ++written_;
            games_ += games.size();
        }

        ++position_;
        return;
    }

    case State::Writing: {
        if (!database_.finishWrite()) {
            fail(database_.lastError());
            return;
        }
        state_ = State::Done;
        return;
    }

    default:
        return;
    }
}
