#include "TheGamesDBClient.h"

#include <cctype>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>

#include "Downloader.h"
#include "Paths.h"

namespace {

std::string trim(const std::string &text) {
    size_t first = 0, last = text.size();
    while (first < last && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

bool appendUtf8(std::string &out, unsigned long cp) {
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    if (cp <= 0x7f) out.push_back(char(cp));
    else if (cp <= 0x7ff) {
        out.push_back(char(0xc0 | (cp >> 6)));
        out.push_back(char(0x80 | (cp & 0x3f)));
    } else if (cp <= 0xffff) {
        out.push_back(char(0xe0 | (cp >> 12)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(char(0x80 | (cp & 0x3f)));
    } else {
        out.push_back(char(0xf0 | (cp >> 18)));
        out.push_back(char(0x80 | ((cp >> 12) & 0x3f)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(char(0x80 | (cp & 0x3f)));
    }
    return true;
}

bool parseHexQuad(const std::string &json, size_t pos, unsigned long &value) {
    if (pos + 4 > json.size()) return false;
    value = 0;
    for (size_t i = pos; i < pos + 4; ++i) {
        const char digit = json[i];
        value <<= 4;
        if (digit >= '0' && digit <= '9') value |= digit - '0';
        else if (digit >= 'a' && digit <= 'f') value |= digit - 'a' + 10;
        else if (digit >= 'A' && digit <= 'F') value |= digit - 'A' + 10;
        else return false;
    }
    return true;
}

bool parseJsonString(const std::string &json, size_t quote, std::string &value, size_t &after) {
    if (quote >= json.size() || json[quote] != '"') return false;
    value.clear();
    for (size_t i = quote + 1; i < json.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(json[i]);
        if (ch == '"') { after = i + 1; return true; }
        if (ch < 0x20) return false;
        if (ch != '\\') { value.push_back(char(ch)); continue; }
        if (++i >= json.size()) return false;
        switch (json[i]) {
        case '"': value.push_back('"'); break;
        case '\\': value.push_back('\\'); break;
        case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break;
        case 'f': value.push_back('\f'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case 'u': {
            unsigned long cp = 0;
            if (!parseHexQuad(json, i + 1, cp)) return false;
            i += 4;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (i + 6 >= json.size() || json[i + 1] != '\\' || json[i + 2] != 'u') return false;
                unsigned long low = 0;
                if (!parseHexQuad(json, i + 3, low) || low < 0xdc00 || low > 0xdfff) return false;
                cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                i += 6;
            }
            if (!appendUtf8(value, cp)) return false;
            break;
        }
        default: return false;
        }
    }
    return false;
}

size_t propertyPosition(const std::string &json, const std::string &key) {
    unsigned objectDepth = 0;
    unsigned arrayDepth = 0;
    for (size_t i = 0; i < json.size(); ++i) {
        const char ch = json[i];
        if (ch == '"') {
            std::string candidate;
            size_t after = 0;
            if (!parseJsonString(json, i, candidate, after)) return std::string::npos;
            size_t colon = after;
            while (colon < json.size() && std::isspace(static_cast<unsigned char>(json[colon]))) ++colon;
            if (objectDepth == 1 && arrayDepth == 0 && candidate == key &&
                colon < json.size() && json[colon] == ':') return i;
            i = after - 1;
        } else if (ch == '{') ++objectDepth;
        else if (ch == '}' && objectDepth > 0) --objectDepth;
        else if (ch == '[') ++arrayDepth;
        else if (ch == ']' && arrayDepth > 0) --arrayDepth;
    }
    return std::string::npos;
}

size_t propertyColon(const std::string &json, size_t position) {
    std::string ignored;
    size_t after = 0;
    if (!parseJsonString(json, position, ignored, after)) return std::string::npos;
    while (after < json.size() && std::isspace(static_cast<unsigned char>(json[after]))) ++after;
    return after < json.size() && json[after] == ':' ? after : std::string::npos;
}

std::string jsonString(const std::string &json, const std::string &key) {
    const size_t pos = propertyPosition(json, key);
    if (pos == std::string::npos) return std::string();
    const size_t colon = propertyColon(json, pos);
    if (colon == std::string::npos) return std::string();
    size_t quote = colon + 1;
    while (quote < json.size() && std::isspace(static_cast<unsigned char>(json[quote]))) ++quote;
    std::string value;
    size_t after = 0;
    return parseJsonString(json, quote, value, after) ? value : std::string();
}

std::string jsonNumber(const std::string &json, const std::string &key) {
    const size_t keyPos = propertyPosition(json, key);
    if (keyPos == std::string::npos) return std::string();
    const size_t colon = propertyColon(json, keyPos);
    if (colon == std::string::npos) return std::string();
    size_t start = colon + 1;
    while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) ++start;
    size_t end = start;
    while (end < json.size() && json[end] >= '0' && json[end] <= '9') ++end;
    return json.substr(start, end - start);
}

} // namespace

TheGamesDBClient::TheGamesDBClient() {
    ensureSettingsFile();
    worker_ = std::thread(&TheGamesDBClient::workerLoop, this);
}

TheGamesDBClient::~TheGamesDBClient() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        cancelCurrent_.store(true);
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

const char *TheGamesDBClient::settingsPath() {
    return MISTER_PAT_ROOT "/thegamesdb-settings.ini";
}

void TheGamesDBClient::ensureSettingsFile() const {
    struct stat info {};
    if (lstat(settingsPath(), &info) == 0) return;
    if (errno != ENOENT) return;
    if (mkdir(MISTER_PAT_ROOT, 0755) != 0 && errno != EEXIST) return;
    if (lstat(MISTER_PAT_ROOT, &info) != 0 || !S_ISDIR(info.st_mode)) return;
    const int fd = open(settingsPath(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) return;
    const char initial[] = "apikey=\"\"\n";
    size_t offset = 0;
    while (offset < sizeof(initial) - 1) {
        const ssize_t written = write(fd, initial + offset, sizeof(initial) - 1 - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) break;
        offset += size_t(written);
    }
    if (offset != sizeof(initial) - 1 || fsync(fd) != 0) unlink(settingsPath());
    close(fd);
}

std::string TheGamesDBClient::readKey() const {
    std::lock_guard<std::mutex> lock(keyMutex_);
    const auto now = std::chrono::steady_clock::now();
    if (keyCacheInitialized_ && now - keyCacheCheckedAt_ < std::chrono::seconds(1))
        return cachedKey_;

    const auto readFromDisk = [this] {
        ensureSettingsFile();
        struct stat info {};
        const int fd = open(settingsPath(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) return std::string();
        if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
            info.st_size > 16384) {
            close(fd);
            return std::string();
        }
        std::string contents;
        contents.reserve(size_t(info.st_size));
        char buffer[1024];
        for (;;) {
            const ssize_t count = read(fd, buffer, sizeof(buffer));
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 || contents.size() + size_t(count) > 16384) {
                close(fd);
                return std::string();
            }
            if (count == 0) break;
            contents.append(buffer, size_t(count));
        }
        close(fd);
        std::istringstream input(contents);
        std::string line;
        while (std::getline(input, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            const size_t equals = line.find('=');
            if (equals == std::string::npos || trim(line.substr(0, equals)) != "apikey") continue;
            std::string key = trim(line.substr(equals + 1));
            if (key.size() >= 2 && key.front() == '"' && key.back() == '"')
                key = key.substr(1, key.size() - 2);
            if (key.size() > 256) return std::string();
            return trim(key);
        }
        return std::string();
    };
    cachedKey_ = readFromDisk();
    keyCacheCheckedAt_ = std::chrono::steady_clock::now();
    keyCacheInitialized_ = true;
    return cachedKey_;
}

bool TheGamesDBClient::configured() const { return !readKey().empty(); }

bool TheGamesDBClient::startApiKeyCheck() {
    const std::string key = readKey();
    if (key.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == CheckState::Checking || pending_) return false;
    activeKey_ = key;
    state_ = CheckState::Checking;
    cancelCurrent_.store(false);
    pending_ = true;
    wake_.notify_one();
    return true;
}

bool TheGamesDBClient::checking() const {
    const std::string key = readKey();
    if (key.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == CheckState::Checking && activeKey_ == key;
}

TheGamesDBClient::CheckState TheGamesDBClient::checkState() const {
    const std::string key = readKey();
    if (key.empty()) return CheckState::NotChecked;
    std::lock_guard<std::mutex> lock(mutex_);
    if ((state_ == CheckState::Checking && activeKey_ != key) ||
        (state_ != CheckState::Checking && checkedKey_ != key))
        return CheckState::NotChecked;
    return state_;
}

std::string TheGamesDBClient::checkStatus() const {
    const std::string key = readKey();
    if (key.empty()) return "Not configured";
    std::lock_guard<std::mutex> lock(mutex_);
    if ((state_ == CheckState::Checking && activeKey_ != key) ||
        (state_ != CheckState::Checking && checkedKey_ != key)) return "Configured";
    switch (state_) {
    case CheckState::NotChecked: return "Configured";
    case CheckState::Checking: return "Checking API key…";
    case CheckState::Success: return "API key works";
    case CheckState::InvalidOrLimited: return "Key invalid or allowance reached";
    case CheckState::NetworkError: return "Network/HTTP error; try again";
    case CheckState::InvalidResponse: return "Unexpected API response";
    }
    return "Configured";
}

void TheGamesDBClient::workerLoop() {
    for (;;) {
        std::string key;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || pending_; });
            if (stopping_) return;
            pending_ = false;
            key = activeKey_;
        }
        const std::string target = std::string(MISTER_PAT_ROOT) + "/thegamesdb-check.json";
        unlink(target.c_str());
        Downloader downloader;
        const std::string url = "https://api.thegamesdb.net/v1/API/Limit?apikey=" +
                                Downloader::encodeComponent(key);
        CheckState result = CheckState::NetworkError;
        int httpStatus = 0;
        if (downloader.fetchWithStatus(url, target, httpStatus, 20, &cancelCurrent_,
                                       65536, false)) {
            std::ifstream input(target.c_str(), std::ios::binary);
            const std::string json((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
            const std::string code = jsonNumber(json, "code");
            const std::string status = jsonString(json, "status");
            // The live allowance endpoint currently returns these numeric fields at the
            // top level without the code/status envelope used by the other API endpoints.
            const std::string remaining = jsonNumber(json, "remaining_monthly_allowance");
            const std::string extra = jsonNumber(json, "extra_allowance");
            const std::string refresh = jsonNumber(json, "allowance_refresh_timer");
            if (httpStatus == 403 || httpStatus == 401 || code == "403" || code == "401")
                result = CheckState::InvalidOrLimited;
            else if (httpStatus != 200) result = CheckState::NetworkError;
            else if ((code == "200" && status == "Success") ||
                     (code.empty() && status.empty() && !remaining.empty() &&
                      !extra.empty() && !refresh.empty())) result = CheckState::Success;
            else if (code.empty() || status.empty()) result = CheckState::InvalidResponse;
            else result = CheckState::InvalidOrLimited;
        }
        unlink(target.c_str());
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        state_ = result;
        checkedKey_ = key;
        activeKey_.clear();
    }
}
