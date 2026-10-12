#include "DiscDriveService.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <sstream>
#include <linux/cdrom.h>
#include <poll.h>
#include <spawn.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "Paths.h"
#include "DiscIndexData.h"

extern char **environ;

namespace {

constexpr size_t kSectorBytes = 2048;
constexpr uint32_t kMaxRootDirectoryBytes = 4u * 1024u * 1024u;
constexpr uint32_t kMaxSystemConfigBytes = 8192;
constexpr unsigned kPollSeconds = 4;
constexpr unsigned kProbeProcessTimeoutSeconds = 7;
constexpr unsigned kProbeStatusTimeoutSeconds = 8;
constexpr uint32_t kProbeWireMagic = 0x44505242; // "DPRB"

struct ProbeWire {
    uint32_t magic = kProbeWireMagic;
    uint32_t state = 0;
    uint32_t system = 0;
    uint32_t reserved = 0;
    uint64_t generation = 0;
    char device[64] = {};
    char model[128] = {};
    char title[512] = {};
    char identifier[128] = {};
    char error[256] = {};
};

template <size_t N>
void copyWireText(char (&destination)[N], const std::string &source) {
    const size_t count = std::min(source.size(), N - 1);
    std::memcpy(destination, source.data(), count);
    destination[count] = '\0';
}

ProbeWire toWire(const DiscDriveService::Snapshot &snapshot) {
    ProbeWire wire;
    wire.state = static_cast<uint32_t>(snapshot.state);
    wire.system = static_cast<uint32_t>(snapshot.system);
    wire.generation = snapshot.generation;
    copyWireText(wire.device, snapshot.device);
    copyWireText(wire.model, snapshot.model);
    copyWireText(wire.title, snapshot.title);
    copyWireText(wire.identifier, snapshot.identifier);
    copyWireText(wire.error, snapshot.error);
    return wire;
}

bool fromWire(const ProbeWire &wire, DiscDriveService::Snapshot &snapshot) {
    if (wire.magic != kProbeWireMagic ||
        wire.state > static_cast<uint32_t>(DiscDriveService::MediaState::Unreadable) ||
        wire.system > static_cast<uint32_t>(DiscDriveService::System::Saturn) ||
        wire.device[sizeof(wire.device) - 1] != '\0' ||
        wire.model[sizeof(wire.model) - 1] != '\0' ||
        wire.title[sizeof(wire.title) - 1] != '\0' ||
        wire.identifier[sizeof(wire.identifier) - 1] != '\0' ||
        wire.error[sizeof(wire.error) - 1] != '\0') return false;
    snapshot.state = static_cast<DiscDriveService::MediaState>(wire.state);
    snapshot.system = static_cast<DiscDriveService::System>(wire.system);
    snapshot.generation = wire.generation;
    snapshot.device = wire.device;
    snapshot.model = wire.model;
    snapshot.title = wire.title;
    snapshot.identifier = wire.identifier;
    snapshot.error = wire.error;
    return true;
}

bool writeAll(int fd, const void *buffer, size_t size) {
    const char *bytes = static_cast<const char *>(buffer);
    size_t offset = 0;
    while (offset < size) {
        const ssize_t written = write(fd, bytes + offset, size - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        offset += size_t(written);
    }
    return true;
}

bool readAll(int fd, void *buffer, size_t size) {
    char *bytes = static_cast<char *>(buffer);
    size_t offset = 0;
    while (offset < size) {
        const ssize_t count = read(fd, bytes + offset, size - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        offset += size_t(count);
    }
    return true;
}

bool sendAllNoSignal(int fd, const void *buffer, size_t size) {
    const char *bytes = static_cast<const char *>(buffer);
    size_t offset = 0;
    while (offset < size) {
        const ssize_t sent = send(fd, bytes + offset, size - offset, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return false;
        offset += size_t(sent);
    }
    return true;
}

std::string trim(const std::string &text);

std::string lowerAscii(std::string text) {
    for (char &ch : text)
        if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
    return text;
}

bool indexQualifier(const std::string &value) {
    const std::string qualifier = lowerAscii(trim(value));
    if (qualifier == "edc") return true;
    if (qualifier.compare(0, 4, "rev ") == 0 && qualifier.size() > 4) {
        for (size_t i = 4; i < qualifier.size(); ++i)
            if (!std::isalnum(static_cast<unsigned char>(qualifier[i]))) return false;
        return true;
    }

    static const char *regions[] = {
        "japan", "usa", "europe", "australia", "canada", "korea", "brazil",
        "china", "asia", "france", "germany", "spain", "italy", "netherlands",
        "sweden", "norway", "denmark", "finland", "uk", "world", "portugal",
        "russia", "mexico", "argentina", "hong kong", "taiwan"
    };
    static const char *languages[] = {
        "en", "ja", "fr", "de", "es", "it", "nl", "sv", "no", "da", "fi",
        "pt", "ko", "zh", "ru", "pl", "cs", "hu", "el", "he", "ar", "th"
    };

    bool hasToken = false;
    size_t start = 0;
    while (start <= qualifier.size()) {
        const size_t comma = qualifier.find(',', start);
        const std::string token = trim(qualifier.substr(start,
            comma == std::string::npos ? std::string::npos : comma - start));
        if (token.empty()) return false;
        bool known = false;
        for (const char *region : regions)
            if (token == region) { known = true; break; }
        if (!known) {
            for (const char *language : languages)
                if (token == language) { known = true; break; }
        }
        if (!known) {
            // Saturn discs list their cartridge RAM requirements as e.g. "1M, 4M".
            const size_t m = token.find('m');
            if (m == std::string::npos || m != token.size() - 1 || m == 0) return false;
            for (size_t i = 0; i < m; ++i)
                if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
        }
        hasToken = true;
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return hasToken;
}

std::string indexTitleFamily(const std::string &title) {
    std::string family = trim(title);
    while (!family.empty() && family.back() == ')') {
        const size_t open = family.find_last_of('(');
        if (open == std::string::npos || !indexQualifier(family.substr(open + 1,
                                                                          family.size() - open - 2)))
            break;
        family = trim(family.substr(0, open));
    }
    return family;
}

std::string resolveIndexTitles(const std::vector<std::string> &titles) {
    if (titles.empty()) return {};
    const std::string family = lowerAscii(indexTitleFamily(titles.front()));
    for (const std::string &title : titles)
        if (lowerAscii(indexTitleFamily(title)) != family) return {};
    return *std::min_element(titles.begin(), titles.end(), [](const std::string &a,
                                                              const std::string &b) {
        return a.size() != b.size() ? a.size() < b.size() : a < b;
    });
}

void readIndex(std::istream &source,
               std::unordered_map<std::string, std::vector<std::string>> &destination) {
    std::string line;
    while (std::getline(source, line)) {
        if (line.empty() || line[0] == '#' || line.size() > 2048) continue;
        const size_t first = line.find('\t');
        const size_t second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (first == std::string::npos || second == std::string::npos) continue;
        const std::string system = line.substr(0, first);
        const std::string identifier = trim(line.substr(first + 1, second - first - 1));
        const std::string title = trim(line.substr(second + 1));
        if ((system != "PSX" && system != "SATURN") || identifier.empty() ||
            identifier.size() > 128 || title.empty() || title.size() > 512) continue;
        destination[system + "\t" + identifier].push_back(title);
    }
}

const std::unordered_map<std::string, std::vector<std::string>> &titleIndex() {
    static const std::unordered_map<std::string, std::vector<std::string>> index = [] {
        std::unordered_map<std::string, std::vector<std::string>> result;
        std::string embedded;
        if (loadEmbeddedDiscIndex(embedded)) {
            std::istringstream builtIn(embedded);
            readIndex(builtIn, result);
        }

        const std::string externalPath = std::string(MISTER_PAT_ROOT) + "/disc-index.tsv";
        const int indexFd = open(externalPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        if (indexFd >= 0) {
            struct stat info {};
            if (fstat(indexFd, &info) == 0 && S_ISREG(info.st_mode) &&
                info.st_size > 0 && info.st_size <= 2 * 1024 * 1024) {
                std::string contents;
                contents.reserve(static_cast<size_t>(info.st_size));
                char buffer[4096];
                bool bounded = true;
                for (;;) {
                    const ssize_t count = read(indexFd, buffer, sizeof(buffer));
                    if (count < 0 && errno == EINTR) continue;
                    if (count < 0) { bounded = false; break; }
                    if (count == 0) break;
                    if (contents.size() + static_cast<size_t>(count) > 2 * 1024 * 1024) {
                        bounded = false;
                        break;
                    }
                    contents.append(buffer, static_cast<size_t>(count));
                }
                if (bounded && !contents.empty()) {
                    std::istringstream externalInput(contents);
                    std::unordered_map<std::string, std::vector<std::string>> overrides;
                    readIndex(externalInput, overrides);
                    for (auto &entry : overrides)
                        result[entry.first] = std::move(entry.second);
                }
            }
            close(indexFd);
        }
        return result;
    }();
    return index;
}

std::string indexedTitle(const char *system, const std::string &identifier) {
    const auto &index = titleIndex();
    const std::string key = std::string(system) + "\t" + identifier;
    auto exact = index.find(key);
    if (exact != index.end()) return resolveIndexTitles(exact->second);
    // Saturn boot headers omit Redump's region/disc suffix (for example -50).
    if (std::strcmp(system, "SATURN") == 0) {
        const std::string prefix = key + "-";
        std::vector<std::string> candidates;
        for (const auto &entry : index)
            if (entry.first.compare(0, prefix.size(), prefix) == 0)
                candidates.insert(candidates.end(), entry.second.begin(), entry.second.end());
        return resolveIndexTitles(candidates);
    }
    return std::string();
}

std::string trim(const std::string &text) {
    size_t first = 0;
    size_t last = text.size();
    while (first < last && (text[first] == ' ' || text[first] == '\0' ||
                            text[first] == '\r' || text[first] == '\n' ||
                            text[first] == '\t')) ++first;
    while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\0' ||
                            text[last - 1] == '\r' || text[last - 1] == '\n' ||
                            text[last - 1] == '\t')) --last;
    return text.substr(first, last - first);
}

std::string upperAscii(std::string text) {
    for (char &ch : text)
        if (ch >= 'a' && ch <= 'z') ch = char(ch - 'a' + 'A');
    return text;
}

bool readAt(int fd, uint64_t lba, unsigned char *bytes, size_t count) {
    const uint64_t maxOffset = static_cast<uint64_t>(std::numeric_limits<off_t>::max());
    if (lba > maxOffset / kSectorBytes) return false;
    const uint64_t offset = lba * kSectorBytes;
    if (count > maxOffset - offset) return false;
    size_t done = 0;
    while (done < count) {
        const ssize_t result = pread(fd, bytes + done, count - done,
                                     off_t(offset + done));
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return false;
        done += size_t(result);
    }
    return true;
}

uint32_t little32(const unsigned char *bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
           (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

bool isoBothEndian32(const unsigned char *bytes, uint32_t &value) {
    const uint32_t little = little32(bytes);
    const uint32_t big = (uint32_t(bytes[4]) << 24) | (uint32_t(bytes[5]) << 16) |
                         (uint32_t(bytes[6]) << 8) | uint32_t(bytes[7]);
    if (little != big) return false;
    value = little;
    return true;
}

bool isoBothEndian16(const unsigned char *bytes, uint16_t &value) {
    const uint16_t little = uint16_t(bytes[0] | (uint16_t(bytes[1]) << 8));
    const uint16_t big = uint16_t((uint16_t(bytes[2]) << 8) | bytes[3]);
    if (little != big) return false;
    value = little;
    return true;
}

std::string headerText(const unsigned char *bytes, size_t length) {
    std::string out;
    out.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        const unsigned char ch = bytes[i];
        if (ch == 0 || ch == '\r' || ch == '\n' || ch == '\t') {
            out.push_back(' ');
        } else if (ch >= 0x20 && ch <= 0x7e) {
            out.push_back(char(ch));
        }
    }
    return trim(out);
}

bool isPlayStationPvd(const unsigned char *pvd, uint32_t &volumeBlocks) {
    uint16_t logicalBlockSize = 0;
    return pvd[0] == 1 && std::memcmp(pvd + 1, "CD001", 5) == 0 && pvd[6] == 1 &&
           upperAscii(headerText(pvd + 8, 32)) == "PLAYSTATION" &&
           isoBothEndian32(pvd + 80, volumeBlocks) && volumeBlocks != 0 &&
           isoBothEndian16(pvd + 128, logicalBlockSize) &&
           logicalBlockSize == kSectorBytes;
}

std::string normalizePsxSerial(const std::string &text) {
    const std::string upper = upperAscii(text);
    for (size_t start = 0; start + 11 <= upper.size(); ++start) {
        bool prefix = true;
        for (size_t i = 0; i < 4; ++i)
            if (upper[start + i] < 'A' || upper[start + i] > 'Z') prefix = false;
        if (!prefix) continue;
        const char separator = upper[start + 4];
        if (separator != '_' && separator != '-') continue;
        if (!std::isdigit(static_cast<unsigned char>(upper[start + 5])) ||
            !std::isdigit(static_cast<unsigned char>(upper[start + 6])) ||
            !std::isdigit(static_cast<unsigned char>(upper[start + 7])) ||
            (upper[start + 8] != '.' && upper[start + 8] != '_') ||
            !std::isdigit(static_cast<unsigned char>(upper[start + 9])) ||
            !std::isdigit(static_cast<unsigned char>(upper[start + 10]))) continue;
        return upper.substr(start, 4) + "-" + upper.substr(start + 5, 3) +
               upper.substr(start + 9, 2);
    }
    return std::string();
}

bool readPsxBootIdentifier(int fd, std::string &identifier) {
    unsigned char pvd[kSectorBytes];
    uint32_t volumeBlocks = 0;
    if (!readAt(fd, 16, pvd, sizeof(pvd)) || !isPlayStationPvd(pvd, volumeBlocks))
        return false;

    const unsigned char *root = pvd + 156;
    uint32_t rootExtent = 0;
    uint32_t directoryLba = 0;
    uint32_t directoryBytes = 0;
    if (root[0] < 34 || (root[25] & 0x02) == 0 || root[32] != 1 || root[33] != 0 ||
        !isoBothEndian32(root + 2, rootExtent) ||
        !isoBothEndian32(root + 10, directoryBytes)) return false;
    if (!directoryBytes || directoryBytes > kMaxRootDirectoryBytes) return false;
    const uint64_t directoryStart = uint64_t(rootExtent) + root[1];
    if (directoryStart > UINT32_MAX) return false;
    directoryLba = uint32_t(directoryStart);
    const uint64_t directorySectors =
        (uint64_t(directoryBytes) + kSectorBytes - 1) / kSectorBytes;
    if (uint64_t(directoryLba) + directorySectors > volumeBlocks) return false;

    uint32_t configLba = 0;
    uint32_t configBytes = 0;
    unsigned char sector[kSectorBytes];
    for (uint32_t offset = 0; offset < directoryBytes;) {
        const uint64_t directorySector = uint64_t(directoryLba) + offset / kSectorBytes;
        if (!readAt(fd, directorySector, sector, sizeof(sector)))
            return false;
        const size_t valid = std::min<size_t>(kSectorBytes, directoryBytes - offset);
        size_t pos = 0;
        while (pos < valid) {
            const unsigned char recordBytes = sector[pos];
            if (!recordBytes) break; // directory entries never cross a logical block
            if (recordBytes < 34 || pos + recordBytes > valid) return false;
            const unsigned char nameBytes = sector[pos + 32];
            if (33u + nameBytes > recordBytes) return false;
            std::string name(reinterpret_cast<const char *>(sector + pos + 33), nameBytes);
            const size_t version = name.find(';');
            if (version != std::string::npos) name.erase(version);
            if (upperAscii(name) == "SYSTEM.CNF") {
                uint32_t configExtent = 0;
                if ((sector[pos + 25] & 0x82) != 0 ||
                    !isoBothEndian32(sector + pos + 2, configExtent) ||
                    !isoBothEndian32(sector + pos + 10, configBytes)) return false;
                const uint64_t configStart = uint64_t(configExtent) + sector[pos + 1];
                if (configStart > UINT32_MAX) return false;
                configLba = uint32_t(configStart);
                break;
            }
            pos += recordBytes;
        }
        if (configBytes) break;
        const uint32_t consumed = uint32_t(valid);
        if (consumed > directoryBytes - offset) break;
        offset += consumed;
    }
    if (!configBytes || configBytes > kMaxSystemConfigBytes ||
        uint64_t(configLba) + (uint64_t(configBytes) + kSectorBytes - 1) / kSectorBytes >
            volumeBlocks) return false;

    std::string config(configBytes, '\0');
    const size_t sectors = (configBytes + kSectorBytes - 1) / kSectorBytes;
    std::vector<unsigned char> data(sectors * kSectorBytes);
    if (!readAt(fd, configLba, data.data(), data.size())) return false;
    std::copy(data.begin(), data.begin() + configBytes, config.begin());

    size_t start = 0;
    while (start < config.size()) {
        const size_t end = config.find('\n', start);
        const std::string line = trim(config.substr(start, end == std::string::npos
                                                            ? std::string::npos
                                                            : end - start));
        const std::string upper = upperAscii(line);
        if (upper.compare(0, 5, "BOOT ") == 0 || upper.compare(0, 5, "BOOT=") == 0) {
            identifier = normalizePsxSerial(line);
            if (!identifier.empty()) return true;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

void readMetadata(int fd, DiscDriveService::Snapshot &out) {
    unsigned char first[kSectorBytes];
    if (!readAt(fd, 0, first, sizeof(first))) {
        out.state = DiscDriveService::MediaState::Unreadable;
        out.error = "Could not read the disc";
        return;
    }

    if (std::memcmp(first, "SEGA SEGASATURN", 15) == 0) {
        out.system = DiscDriveService::System::Saturn;
        out.identifier = headerText(first + 0x20, 16);
        out.title = headerText(first + 0x60, 0x70);
        const std::string indexed = indexedTitle("SATURN", out.identifier);
        if (!indexed.empty()) out.title = indexed;
        if (out.title.empty()) out.title = out.identifier;
        out.state = out.identifier.empty() ? DiscDriveService::MediaState::Unreadable
                                           : DiscDriveService::MediaState::Readable;
        if (out.state == DiscDriveService::MediaState::Unreadable)
            out.error = "The Saturn disc header is incomplete";
        return;
    }

    std::string serial;
    if (readPsxBootIdentifier(fd, serial)) {
        out.system = DiscDriveService::System::PlayStation;
        out.identifier = serial;
        out.title = indexedTitle("PSX", serial);
        if (out.title.empty()) out.title = "PlayStation disc";
        out.state = DiscDriveService::MediaState::Readable;
        return;
    }

    // Read the primary descriptor separately so an ISO disc with a malformed or missing
    // SYSTEM.CNF is distinguished from an unrelated CD or a failed optical read.
    unsigned char pvd[kSectorBytes];
    uint32_t volumeBlocks = 0;
    if (readAt(fd, 16, pvd, sizeof(pvd)) && isPlayStationPvd(pvd, volumeBlocks)) {
        out.system = DiscDriveService::System::PlayStation;
        out.state = DiscDriveService::MediaState::Readable;
        return;
    }

    out.state = DiscDriveService::MediaState::Unsupported;
    out.error = "No supported PlayStation or Saturn disc was found";
}

std::string readModel(const std::string &device) {
    const size_t slash = device.find_last_of('/');
    if (slash == std::string::npos) return std::string();
    std::ifstream input("/sys/block/" + device.substr(slash + 1) + "/device/model");
    std::string model;
    std::getline(input, model);
    return trim(model);
}

bool opticalDeviceName(const char *name) {
    if (!name || name[0] != 's' || name[1] != 'r' || !name[2]) return false;
    for (const char *p = name + 2; *p; ++p)
        if (*p < '0' || *p > '9') return false;
    return true;
}

bool sameMedia(const DiscDriveService::Snapshot &a, const DiscDriveService::Snapshot &b) {
    return a.state == b.state && a.system == b.system && a.device == b.device &&
           a.model == b.model && a.title == b.title && a.identifier == b.identifier &&
           a.error == b.error;
}

} // namespace

DiscDriveService::DiscDriveService() = default;

void DiscDriveService::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (worker_.joinable()) return;
    stopping_ = false;
    worker_ = std::thread(&DiscDriveService::workerLoop, this);
}

void DiscDriveService::requestProbe() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        probeRequested_ = true;
    }
    wake_.notify_one();
}

DiscDriveService::~DiscDriveService() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
    if (outstandingProbePid_ > 0) {
        kill(pid_t(outstandingProbePid_), SIGKILL);
        waitpid(pid_t(outstandingProbePid_), nullptr, WNOHANG);
        outstandingProbePid_ = -1;
    }
}

DiscDriveService::Snapshot DiscDriveService::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((probeActive_ || probeRequested_) && snapshot_.state != MediaState::NoDrive) {
        Snapshot pending = snapshot_;
        pending.state = MediaState::Checking;
        pending.system = System::Unknown;
        pending.title.clear();
        pending.identifier.clear();
        pending.error = std::chrono::steady_clock::now() - probeStarted_ >=
                                std::chrono::seconds(kProbeStatusTimeoutSeconds)
                            ? "Drive read is taking longer than expected"
                            : std::string();
        return pending;
    }
    return snapshot_;
}

const char *DiscDriveService::stateName(MediaState state) {
    switch (state) {
    case MediaState::NoDrive: return "No drive detected";
    case MediaState::NoDisc: return "Please insert disc";
    case MediaState::Checking: return "Checking disc";
    case MediaState::Readable: return "Disc ready";
    case MediaState::Unsupported: return "No supported game detected";
    case MediaState::Unreadable: return "Disc could not be read";
    }
    return "Unknown state";
}

const char *DiscDriveService::systemName(System system) {
    switch (system) {
    case System::Unknown: return "Unknown system";
    case System::PlayStation: return "Sony PlayStation";
    case System::Saturn: return "Sega Saturn";
    }
    return "Unknown system";
}

void DiscDriveService::workerLoop() {
    for (;;) {
        Snapshot previous;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopping_) return;
            previous = snapshot_;
            probeRequested_ = false;
            probeActive_ = true;
            probeStarted_ = std::chrono::steady_clock::now();
        }

        Snapshot next = probeWithTimeout(previous);
        if (next.generation <= previous.generation) {
            next.generation = sameMedia(previous, next)
                ? previous.generation : previous.generation + 1;
        }
        {
            std::unique_lock<std::mutex> lock(mutex_);
            probeActive_ = false;
            if (stopping_) return;
            snapshot_ = std::move(next);
            if (wake_.wait_for(lock, std::chrono::seconds(kPollSeconds),
                               [this] { return stopping_ || probeRequested_; }) && stopping_)
                return;
        }
    }
}

int DiscDriveService::runProbeRequest(int inputFd, int outputFd) {
    ProbeWire request;
    if (!readAll(inputFd, &request, sizeof(request))) return 2;
    Snapshot previous;
    if (!fromWire(request, previous)) return 2;
    const Snapshot result = probeOnce(previous);
    const ProbeWire response = toWire(result);
    return writeAll(outputFd, &response, sizeof(response)) ? 0 : 3;
}

DiscDriveService::Snapshot DiscDriveService::probeWithTimeout(const Snapshot &previous) {
    if (outstandingProbePid_ > 0) {
        int status = 0;
        const pid_t result = waitpid(pid_t(outstandingProbePid_), &status, WNOHANG);
        if (result == 0) {
            Snapshot pending = previous;
            pending.state = MediaState::Checking;
            pending.system = System::Unknown;
            pending.title.clear();
            pending.identifier.clear();
            pending.error = "Drive read is taking longer than expected";
            return pending;
        }
        outstandingProbePid_ = -1;
    }

    int requestSocket[2] = {-1, -1};
    int responsePipe[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, requestSocket) != 0 || pipe(responsePipe) != 0) {
        if (requestSocket[0] >= 0) { close(requestSocket[0]); close(requestSocket[1]); }
        if (responsePipe[0] >= 0) { close(responsePipe[0]); close(responsePipe[1]); }
        Snapshot failed;
        failed.state = MediaState::Unreadable;
        failed.error = "Cannot create bounded disc probe";
        return failed;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, requestSocket[1], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, responsePipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, requestSocket[0]);
    posix_spawn_file_actions_addclose(&actions, responsePipe[0]);
    posix_spawn_file_actions_addclose(&actions, requestSocket[1]);
    posix_spawn_file_actions_addclose(&actions, responsePipe[1]);

    char program[] = MISTER_PAT_ROOT "/mister-gui";
    char probeArg[] = "--disc-probe-once";
    char *argv[] = {program, probeArg, nullptr};
    pid_t child = -1;
    const int spawnError = posix_spawn(&child, program, &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(requestSocket[1]);
    close(responsePipe[1]);
    if (spawnError != 0) {
        close(requestSocket[0]);
        close(responsePipe[0]);
        Snapshot failed;
        failed.state = MediaState::Unreadable;
        failed.error = "Cannot start bounded disc probe";
        return failed;
    }

    const ProbeWire request = toWire(previous);
    if (!sendAllNoSignal(requestSocket[0], &request, sizeof(request))) {
        close(requestSocket[0]);
        close(responsePipe[0]);
        kill(child, SIGKILL);
        waitpid(child, nullptr, 0);
        Snapshot failed;
        failed.state = MediaState::Unreadable;
        failed.error = "Cannot send bounded disc probe request";
        return failed;
    }
    close(requestSocket[0]);

    ProbeWire response;
    size_t received = 0;
    bool timedOut = false;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(kProbeProcessTimeoutSeconds);
    while (received < sizeof(response)) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) { timedOut = true; break; }
        pollfd descriptor{responsePipe[0], POLLIN | POLLHUP, 0};
        const int ready = poll(&descriptor, 1, int(std::min<long long>(200, remaining)));
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) break;
        if (ready == 0) continue;
        const ssize_t count = read(responsePipe[0], reinterpret_cast<char *>(&response) + received,
                                   sizeof(response) - received);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        received += size_t(count);
    }
    close(responsePipe[0]);

    int childStatus = 0;
    if (timedOut || received != sizeof(response)) kill(child, SIGKILL);
    pid_t waited;
    do { waited = waitpid(child, &childStatus, WNOHANG); }
    while (waited < 0 && errno == EINTR);
    if (waited == 0) {
        // A SCSI request may remain in an uninterruptible kernel wait after SIGKILL.
        // Retain one PID and do not pile up more probes behind it.
        outstandingProbePid_ = int(child);
    }
    if (received == sizeof(response)) {
        Snapshot result;
        if (fromWire(response, result)) return result;
    }

    Snapshot failed = previous;
    failed.state = timedOut || waited == 0 ? MediaState::Checking : MediaState::Unreadable;
    failed.system = System::Unknown;
    failed.title.clear();
    failed.identifier.clear();
    failed.error = timedOut ? "Disc read timed out" : "Disc probe failed";
    return failed;
}

DiscDriveService::Snapshot DiscDriveService::probeOnce(const Snapshot &previous) {
    DIR *directory = opendir("/dev");
    if (!directory) return Snapshot();

    std::vector<std::string> devices;
    while (dirent *entry = readdir(directory)) {
        if (!opticalDeviceName(entry->d_name)) continue;
        const std::string path = std::string("/dev/") + entry->d_name;
        struct stat info {};
        if (stat(path.c_str(), &info) == 0 && S_ISBLK(info.st_mode))
            devices.push_back(path);
    }
    closedir(directory);
    std::sort(devices.begin(), devices.end());
    if (devices.empty()) return Snapshot();

    Snapshot noDisc;
    noDisc.state = MediaState::NoDisc;
    for (const std::string &device : devices) {
        Snapshot current;
        current.device = device;
        current.model = readModel(device);
        current.state = MediaState::Checking;

        const int fd = open(device.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            current.state = MediaState::Unreadable;
            current.error = "Cannot open " + device;
            if (noDisc.device.empty()) noDisc = current;
            continue;
        }

        const int status = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
        const int changed = ioctl(fd, CDROM_MEDIA_CHANGED, CDSL_CURRENT);
        if (status == CDS_NO_DISC || status == CDS_TRAY_OPEN) {
            close(fd);
            if (noDisc.device.empty()) noDisc = current;
            continue;
        }
        if (status == CDS_DRIVE_NOT_READY && changed <= 0) {
            current.state = MediaState::Checking;
            close(fd);
            if (noDisc.device.empty()) noDisc = current;
            continue;
        }

        if (status == CDS_DISC_OK || status == CDS_NO_INFO || changed > 0) {
            const bool sameDevice = previous.device == device;
            const bool previousHasMedia = previous.state == MediaState::Readable ||
                                          previous.state == MediaState::Unsupported;
            if (sameDevice && previousHasMedia && changed == 0) {
                current = previous;
                current.model = readModel(device);
            } else {
                current.state = MediaState::Checking;
                readMetadata(fd, current);
            }
        }
        close(fd);

        if (current.state != MediaState::NoDisc && current.state != MediaState::Checking) {
            // Keep a tray/media-change event even when the replacement has the same
            // serial and title as the previous disc. A pending launch must be invalidated.
            if (changed > 0 && (current.state == MediaState::Readable ||
                                current.state == MediaState::Unsupported))
                current.generation = previous.generation + 1;
            return current;
        }
        if (noDisc.device.empty()) noDisc = current;
    }
    return noDisc;
}
