#include "ArtworkIndex.h"

#include <chrono>
#include <dirent.h>
#include <unistd.h>

#include "DebugLog.h"

namespace {

// About 25 MB at the worst; a library of this kind is normally a tenth of that.
constexpr size_t kMaxNames = 300000;
// Pause after each directory that was read, to keep the reads gentle on the storage.
constexpr useconds_t kPauseBetweenDirectoriesUs = 50 * 1000;

std::string stripTrailingSlashes(std::string path) {
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return path;
}

// exFAT ignores case, so names are compared in lower case. A name with anything outside ASCII
// is left to the file system, which folds case with its own table; returns false for those.
bool foldName(const std::string &name, std::string &folded) {
    folded.clear();
    folded.reserve(name.size());
    for (unsigned char c : name) {
        if (c >= 0x80) return false;
        folded.push_back(char(c >= 'A' && c <= 'Z' ? c + 32 : c));
    }
    return true;
}

} // namespace

ArtworkIndex &ArtworkIndex::instance() {
    static ArtworkIndex index;
    return index;
}

ArtworkIndex::~ArtworkIndex() { stopWorker(); }

void ArtworkIndex::stopWorker() {
    generation_.fetch_add(1); // tells a running scan to stop
    if (worker_.joinable()) worker_.join();
}

void ArtworkIndex::clear() {
    stopWorker();
    std::lock_guard<std::mutex> lock(mutex_);
    directories_.clear();
    names_ = 0;
}

void ArtworkIndex::build(const std::vector<std::string> &directories) {
    clear();
    scan(directories, generation_.load());
}

void ArtworkIndex::buildAsync(std::vector<std::string> directories) {
    clear();
    const unsigned generation = generation_.load();
    try {
        worker_ = std::thread([this, directories = std::move(directories), generation] {
            scan(directories, generation);
        });
    } catch (...) {
        // No thread: leave the index empty; every answer stays "Unknown".
    }
}

void ArtworkIndex::scan(const std::vector<std::string> &directories, unsigned generation) {
    const auto started = std::chrono::steady_clock::now();
    size_t scanned = 0;
    bool full = false;
    for (const std::string &raw : directories) {
        if (generation_.load() != generation) return;
        const std::string directory = stripTrailingSlashes(raw);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (directories_.count(directory)) continue;
        }
        DIR *handle = opendir(directory.c_str());
        if (!handle) continue;
        std::unordered_set<std::string> names;
        while (dirent *entry = readdir(handle)) {
            const std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            std::string folded;
            if (foldName(name, folded)) names.insert(std::move(folded));
        }
        closedir(handle);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation_.load() != generation) return;
            if (names_ + names.size() > kMaxNames) {
                full = true; // stop here; directories not indexed are asked the slow way
                break;
            }
            names_ += names.size();
            directories_[directory] = std::move(names);
        }
        ++scanned;
        usleep(kPauseBetweenDirectoriesUs);
    }

    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    size_t total = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        total = names_;
    }
    char message[160];
    std::snprintf(message, sizeof(message),
                  "artwork index: directories=%zu names=%zu elapsed-ms=%lld%s", scanned, total, ms,
                  full ? " (limit reached)" : "");
    DebugLog::info(message);
}

ArtworkIndex::Presence ArtworkIndex::lookup(const std::string &filePath) const {
    const size_t slash = filePath.find_last_of('/');
    if (slash == std::string::npos || slash + 1 >= filePath.size()) return Presence::Unknown;
    std::string folded;
    if (!foldName(filePath.substr(slash + 1), folded)) return Presence::Unknown;
    const std::string directory = stripTrailingSlashes(filePath.substr(0, slash));

    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = directories_.find(directory);
    if (found == directories_.end()) return Presence::Unknown;
    return found->second.count(folded) ? Presence::Present : Presence::Absent;
}

void ArtworkIndex::noteFile(const std::string &filePath) {
    const size_t slash = filePath.find_last_of('/');
    if (slash == std::string::npos || slash + 1 >= filePath.size()) return;
    std::string folded;
    if (!foldName(filePath.substr(slash + 1), folded)) return;
    const std::string directory = stripTrailingSlashes(filePath.substr(0, slash));

    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = directories_.find(directory);
    if (found != directories_.end() && found->second.insert(folded).second) ++names_;
}
