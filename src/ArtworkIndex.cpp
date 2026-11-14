#include "ArtworkIndex.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#include "DebugLog.h"
#include "Paths.h"
#include "SafeFileWrite.h"

namespace {

// About 25 MB at the worst; a library of this kind is normally a tenth of that.
constexpr size_t kMaxNames = 300000;
// Pause after each directory that was read, to keep the reads gentle on the storage.
constexpr useconds_t kPauseBetweenDirectoriesUs = 50 * 1000;
constexpr size_t kMaxIndexBytes = 64 * 1024 * 1024;
const char *kIndexPath = MISTER_PAT_ROOT "/boxart-index.tsv";
const char *kIndexHeader = "MISTER-PAT-ARTWORK-INDEX 1";

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

std::string hexEncode(const std::string &value) {
    static const char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (unsigned char c : value) {
        result.push_back(digits[c >> 4]);
        result.push_back(digits[c & 15]);
    }
    return result;
}

bool hexDecode(const std::string &value, std::string &result) {
    if (value.size() % 2) return false;
    result.clear();
    result.reserve(value.size() / 2);
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    for (size_t i = 0; i < value.size(); i += 2) {
        int hi = digit(value[i]), lo = digit(value[i + 1]);
        if (hi < 0 || lo < 0) return false;
        result.push_back(char((hi << 4) | lo));
    }
    return true;
}

bool readIndexFile(std::string &content) {
    content.clear();
    int fd = open(kIndexPath, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
        static_cast<unsigned long long>(info.st_size) > kMaxIndexBytes) {
        close(fd);
        return false;
    }
    content.reserve(static_cast<size_t>(info.st_size));
    char buffer[16384];
    for (;;) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            const bool ok = count == 0 && close(fd) == 0;
            if (!ok) content.clear();
            return ok;
        }
        if (content.size() + static_cast<size_t>(count) > kMaxIndexBytes) {
            close(fd);
            content.clear();
            return false;
        }
        content.append(buffer, static_cast<size_t>(count));
    }
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

void ArtworkIndex::build(const std::vector<std::string> &directories, bool persistWhenDone) {
    clear();
    scan(directories, generation_.load(), persistWhenDone);
}

void ArtworkIndex::buildAsync(std::vector<std::string> directories) {
    clear();
    const unsigned generation = generation_.load();
    try {
        worker_ = std::thread([this, directories = std::move(directories), generation] {
            scan(directories, generation, true);
        });
    } catch (...) {
        // No thread: leave the index empty; every answer stays "Unknown".
    }
}

bool ArtworkIndex::loadPersistent(const std::vector<std::string> &expectedDirectories) {
    clear();
    std::string content;
    if (!readIndexFile(content) || content.empty() || content.back() != '\n') return false;

    std::unordered_set<std::string> expected;
    for (const std::string &directory : expectedDirectories)
        expected.insert(stripTrailingSlashes(directory));

    std::istringstream input(content);
    std::string line;
    if (!std::getline(input, line) || line != kIndexHeader) return false;
    std::unordered_map<std::string, std::unordered_set<std::string>> loaded;
    std::string currentDirectory;
    size_t count = 0;
    bool haveDirectory = false;
    while (std::getline(input, line)) {
        if (line.size() < 3 || line[1] != ' ') return false;
        std::string decoded;
        if (!hexDecode(line.substr(2), decoded)) return false;
        if (line[0] == 'D') {
            currentDirectory = stripTrailingSlashes(decoded);
            if (currentDirectory.empty() || !expected.count(currentDirectory) ||
                loaded.count(currentDirectory)) return false;
            loaded.emplace(currentDirectory, std::unordered_set<std::string>{});
            haveDirectory = true;
        } else if (line[0] == 'F') {
            if (!haveDirectory || decoded.empty() || decoded.find('/') != std::string::npos ||
                decoded.find('\0') != std::string::npos || ++count > kMaxNames) return false;
            std::string folded;
            if (!foldName(decoded, folded) || folded != decoded) return false;
            loaded[currentDirectory].insert(std::move(decoded));
        } else {
            return false;
        }
    }
    if (loaded.empty()) return false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        directories_ = std::move(loaded);
        names_ = count;
    }
    DebugLog::info("artwork index: loaded persistent index, names=" + std::to_string(count));
    return true;
}

bool ArtworkIndex::savePersistent() const {
    std::string content = std::string(kIndexHeader) + "\n";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &directory : directories_) {
            content += "D " + hexEncode(directory.first) + "\n";
            if (content.size() > kMaxIndexBytes) return false;
            for (const std::string &name : directory.second) {
                content += "F " + hexEncode(name) + "\n";
                if (content.size() > kMaxIndexBytes) return false;
            }
        }
    }
    return SafeFileWrite::atomicWrite(kIndexPath, content);
}

void ArtworkIndex::scan(const std::vector<std::string> &directories, unsigned generation,
                        bool persistWhenDone) {
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
    if (persistWhenDone && generation_.load() == generation) {
        if (savePersistent()) DebugLog::info("artwork index: persistent index saved");
        else DebugLog::info("artwork index: could not save persistent index");
    }
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
