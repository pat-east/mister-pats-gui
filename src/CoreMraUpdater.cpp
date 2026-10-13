#include "CoreMraUpdater.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <map>
#include <poll.h>
#include <spawn.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#include "Downloader.h"
#include "Paths.h"
#include "SafeFileWrite.h"
#include "UpdateFiles.h"

extern char **environ;

namespace {

const char *kManifestUrl =
    "https://raw.githubusercontent.com/MiSTer-devel/Distribution_MiSTer/main/db.json.zip";
const uint64_t kManifestZipLimit = 32u * 1024u * 1024u;
const size_t kManifestLimit = 96u * 1024u * 1024u;
const std::string kDistributionPrefix =
    "https://raw.githubusercontent.com/MiSTer-devel/Distribution_MiSTer/";

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    std::string text;
    bool boolean = false;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;
};

class JsonReader {
public:
    explicit JsonReader(const std::string &text) : text_(text) {}
    bool parse(Json &out) {
        if (!value(out, 0)) return false;
        whitespace();
        return cursor_ == text_.size();
    }
private:
    const std::string &text_;
    size_t cursor_ = 0;
    void whitespace() {
        while (cursor_ < text_.size() && std::isspace((unsigned char)text_[cursor_])) ++cursor_;
    }
    bool consume(char ch) {
        whitespace();
        if (cursor_ >= text_.size() || text_[cursor_] != ch) return false;
        ++cursor_;
        return true;
    }
    bool string(std::string &out) {
        if (!consume('"')) return false;
        while (cursor_ < text_.size()) {
            unsigned char ch = (unsigned char)text_[cursor_++];
            if (ch == '"') return true;
            if (ch < 0x20) return false;
            if (ch != '\\') { out.push_back(char(ch)); continue; }
            if (cursor_ >= text_.size()) return false;
            const char escape = text_[cursor_++];
            switch (escape) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (cursor_ + 4 > text_.size()) return false;
                unsigned cp = 0;
                for (int i = 0; i < 4; ++i) {
                    const char digit = text_[cursor_++];
                    cp <<= 4;
                    if (digit >= '0' && digit <= '9') cp |= unsigned(digit - '0');
                    else if (digit >= 'a' && digit <= 'f') cp |= unsigned(digit - 'a' + 10);
                    else if (digit >= 'A' && digit <= 'F') cp |= unsigned(digit - 'A' + 10);
                    else return false;
                }
                if (cp < 0x80) out.push_back(char(cp));
                else if (cp < 0x800) {
                    out.push_back(char(0xc0 | (cp >> 6)));
                    out.push_back(char(0x80 | (cp & 0x3f)));
                } else {
                    out.push_back(char(0xe0 | (cp >> 12)));
                    out.push_back(char(0x80 | ((cp >> 6) & 0x3f)));
                    out.push_back(char(0x80 | (cp & 0x3f)));
                }
                break;
            }
            default: return false;
            }
        }
        return false;
    }
    bool value(Json &out, unsigned depth) {
        if (depth > 48) return false;
        whitespace();
        if (cursor_ >= text_.size()) return false;
        const char ch = text_[cursor_];
        if (ch == '{') {
            ++cursor_; out.type = Json::Type::Object;
            if (consume('}')) return true;
            do {
                std::string key; Json item;
                if (!string(key) || !consume(':') || !value(item, depth + 1)) return false;
                for (const auto &old : out.object) if (old.first == key) return false;
                out.object.emplace_back(std::move(key), std::move(item));
                if (consume('}')) return true;
            } while (consume(','));
            return false;
        }
        if (ch == '[') {
            ++cursor_; out.type = Json::Type::Array;
            if (consume(']')) return true;
            do {
                Json item;
                if (!value(item, depth + 1)) return false;
                out.array.push_back(std::move(item));
                if (consume(']')) return true;
            } while (consume(','));
            return false;
        }
        if (ch == '"') { out.type = Json::Type::String; return string(out.text); }
        if (text_.compare(cursor_, 4, "true") == 0) {
            cursor_ += 4; out.type = Json::Type::Bool; out.boolean = true; return true;
        }
        if (text_.compare(cursor_, 5, "false") == 0) {
            cursor_ += 5; out.type = Json::Type::Bool; return true;
        }
        if (text_.compare(cursor_, 4, "null") == 0) { cursor_ += 4; return true; }
        const size_t start = cursor_;
        if (text_[cursor_] == '-') ++cursor_;
        while (cursor_ < text_.size() && text_[cursor_] >= '0' && text_[cursor_] <= '9') ++cursor_;
        if (cursor_ == start || (cursor_ == start + 1 && text_[start] == '-')) return false;
        out.type = Json::Type::Number; out.text = text_.substr(start, cursor_ - start); return true;
    }
};

const Json *field(const Json &object, const char *name) {
    if (object.type != Json::Type::Object) return nullptr;
    for (const auto &item : object.object) if (item.first == name) return &item.second;
    return nullptr;
}

uint16_t le16(const unsigned char *p) {
    return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}
uint32_t le32(const unsigned char *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

bool inflateManifest(const std::string &zip, std::string &json, std::string &error) {
    const unsigned char *data = reinterpret_cast<const unsigned char *>(zip.data());
    const size_t size = zip.size();
    if (size < 22) { error = "distribution manifest archive is truncated"; return false; }
    size_t eocd = std::string::npos;
    const size_t scanStart = size > 65557 ? size - 65557 : 0;
    for (size_t i = size - 22 + 1; i-- > scanStart;) {
        if (i + 22 <= size && !std::memcmp(data + i, "PK\x05\x06", 4)) { eocd = i; break; }
    }
    if (eocd == std::string::npos) { error = "distribution manifest archive has no directory"; return false; }
    const uint16_t entries = le16(data + eocd + 10);
    const uint32_t centralSize = le32(data + eocd + 12);
    const uint32_t centralAt = le32(data + eocd + 16);
    if (centralAt > size || centralSize > size - centralAt) {
        error = "distribution manifest archive has an invalid directory"; return false;
    }
    size_t pos = centralAt;
    for (uint16_t i = 0; i < entries; ++i) {
        if (pos + 46 > size || std::memcmp(data + pos, "PK\x01\x02", 4)) {
            error = "distribution manifest archive has an invalid entry"; return false;
        }
        const uint16_t flags = le16(data + pos + 8), method = le16(data + pos + 10);
        const uint32_t crc = le32(data + pos + 16), compressed = le32(data + pos + 20);
        const uint32_t uncompressed = le32(data + pos + 24), localAt = le32(data + pos + 42);
        const uint16_t nameLen = le16(data + pos + 28), extraLen = le16(data + pos + 30);
        const uint16_t commentLen = le16(data + pos + 32);
        if (pos + 46u + nameLen + extraLen + commentLen > size) {
            error = "distribution manifest archive entry is truncated"; return false;
        }
        const std::string name(reinterpret_cast<const char *>(data + pos + 46), nameLen);
        if (name == "db.json") {
            if ((flags & 1) || (method != 0 && method != 8) || uncompressed == 0 ||
                uncompressed > kManifestLimit || compressed > size || localAt + 30 > size ||
                std::memcmp(data + localAt, "PK\x03\x04", 4)) {
                error = "unsupported or unsafe distribution manifest entry"; return false;
            }
            const uint16_t localName = le16(data + localAt + 26), localExtra = le16(data + localAt + 28);
            const size_t bodyAt = size_t(localAt) + 30u + localName + localExtra;
            if (bodyAt > size || compressed > size - bodyAt) {
                error = "distribution manifest data is truncated"; return false;
            }
            json.assign(uncompressed, '\0');
            if (method == 0) {
                if (compressed != uncompressed) { error = "invalid stored manifest size"; return false; }
                std::memcpy(&json[0], data + bodyAt, uncompressed);
            } else {
                z_stream stream{};
                stream.next_in = const_cast<Bytef *>(data + bodyAt);
                stream.avail_in = compressed;
                stream.next_out = reinterpret_cast<Bytef *>(&json[0]);
                stream.avail_out = uncompressed;
                if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) { error = "cannot initialize manifest decompression"; return false; }
                const int status = inflate(&stream, Z_FINISH);
                inflateEnd(&stream);
                if (status != Z_STREAM_END || stream.total_out != uncompressed || stream.total_in != compressed) {
                    error = "cannot decompress distribution manifest"; return false;
                }
            }
            if (crc32(0, reinterpret_cast<const Bytef *>(json.data()), uInt(json.size())) != crc) {
                error = "distribution manifest checksum is invalid"; return false;
            }
            return true;
        }
        pos += 46u + nameLen + extraLen + commentLen;
    }
    error = "distribution manifest archive does not contain db.json";
    return false;
}

bool validPath(const std::string &path) {
    if (path.empty() || path.size() > 512 || path[0] == '/' || path.find('\\') != std::string::npos ||
        path.find('\0') != std::string::npos) return false;
    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        const std::string part = path.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") return false;
        for (unsigned char ch : part) if (ch < 0x20 || ch == 0x7f) return false;
        start = end + 1;
    }
    return true;
}

bool hexHash(const std::string &hash) {
    if (hash.size() != 32) return false;
    for (unsigned char c : hash)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    return true;
}

std::string encodePath(const std::string &path) {
    std::string out;
    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        if (!out.empty()) out.push_back('/');
        out += Downloader::encodeComponent(path.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

bool runMd5sum(const std::string &path, std::string &hash, std::string &error) {
    int pipefd[2];
    if (pipe(pipefd) != 0) { error = "cannot create hash pipe"; return false; }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);
    char tool[] = "md5sum", option[] = "--";
    char *argv[] = {tool, option, const_cast<char *>(path.c_str()), nullptr};
    pid_t child = -1;
    const int spawnError = posix_spawnp(&child, tool, &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);
    if (spawnError != 0) { close(pipefd[0]); error = "cannot start md5sum"; return false; }
    char output[256]; size_t used = 0;
    while (used + 1 < sizeof(output)) {
        ssize_t n = read(pipefd[0], output + used, sizeof(output) - used - 1);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        used += size_t(n);
    }
    close(pipefd[0]); output[used] = '\0';
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || used < 32) {
        error = "cannot hash installed file with md5sum"; return false;
    }
    hash.assign(output, 32);
    std::transform(hash.begin(), hash.end(), hash.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return hexHash(hash);
}

bool safeTarget(const std::string &root, const std::string &relative, std::string &absolute,
                std::string &error) {
    if (!validPath(relative)) { error = "unsafe path in distribution manifest"; return false; }
    absolute = root + "/" + relative;
    std::string current = root;
    size_t start = 0;
    while (start < relative.size()) {
        size_t end = relative.find('/', start);
        if (end == std::string::npos) end = relative.size();
        current += "/" + relative.substr(start, end - start);
        struct stat st{};
        if (lstat(current.c_str(), &st) == 0) {
            if (S_ISLNK(st.st_mode)) { error = "symlink in managed update path: " + relative; return false; }
            if (end < relative.size() && !S_ISDIR(st.st_mode)) {
                error = "non-directory in managed update path: " + relative; return false;
            }
            if (end == relative.size() && !S_ISREG(st.st_mode)) {
                error = "target is not a regular file: " + relative; return false;
            }
        } else if (errno != ENOENT) { error = "cannot inspect update path: " + relative; return false; }
        start = end + 1;
    }
    return true;
}

bool ensureParentDirectories(const std::string &root, const std::string &relative,
                             std::string &error) {
    std::string current = root;
    size_t start = 0;
    for (;;) {
        size_t end = relative.find('/', start);
        if (end == std::string::npos) return true;
        current += "/" + relative.substr(start, end - start);
        if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
            error = "cannot create update directory: " + current;
            return false;
        }
        struct stat st{};
        if (lstat(current.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            error = "unsafe update directory: " + current;
            return false;
        }
        start = end + 1;
    }
}

bool parseManifest(const std::string &text, std::vector<CoreMraEntry> &entries, std::string &error) {
    Json root;
    if (!JsonReader(text).parse(root) || root.type != Json::Type::Object) {
        error = "distribution manifest is not valid JSON"; return false;
    }
    const Json *base = field(root, "base_files_url"), *files = field(root, "files"), *tags = field(root, "tag_dictionary");
    if (!base || base->type != Json::Type::String ||
        base->text.compare(0, kDistributionPrefix.size(), kDistributionPrefix) != 0 ||
        !files || files->type != Json::Type::Object || !tags || tags->type != Json::Type::Object) {
        error = "distribution manifest has an unsupported schema or source"; return false;
    }
    // The official file lists "base_files_url" with a trailing slash; the URLs below add their own.
    std::string baseUrl = base->text;
    while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
    const std::string revision = baseUrl.size() > kDistributionPrefix.size()
                                     ? baseUrl.substr(kDistributionPrefix.size()) : std::string();
    if (revision.empty() || revision.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~") != std::string::npos) {
        error = "distribution manifest has an unsafe base URL"; return false;
    }
    std::map<int64_t, std::string> tagNames;
    for (const auto &tag : tags->object) {
        if (tag.second.type != Json::Type::Number) continue;
        char *end = nullptr; long long id = std::strtoll(tag.second.text.c_str(), &end, 10);
        if (!end || *end || id < 0) { error = "invalid tag dictionary entry"; return false; }
        tagNames[id] = tag.first;
    }
    for (const auto &item : files->object) {
        const std::string &path = item.first;
        if (!validPath(path) || item.second.type != Json::Type::Object) continue;
        const std::string lower = [&] { std::string v = path; std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){return char(std::tolower(c));}); return v; }();
        CoreMraType type;
        bool include = false;
        const Json *tagList = field(item.second, "tags");
        if (!tagList || tagList->type != Json::Type::Array) continue;
        bool arcadeCore = false, consoleCore = false;
        for (const Json &tag : tagList->array) {
            if (tag.type != Json::Type::Number) continue;
            char *end = nullptr; long long id = std::strtoll(tag.text.c_str(), &end, 10);
            if (!end || *end) continue;
            auto found = tagNames.find(id);
            if (found == tagNames.end()) continue;
            arcadeCore = arcadeCore || found->second == "arcadecores";
            consoleCore = consoleCore || found->second == "consolecores";
        }
        if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".rbf") == 0 && (arcadeCore || consoleCore)) {
            type = arcadeCore ? CoreMraType::ArcadeCore : CoreMraType::ConsoleCore;
            include = true;
        } else if (lower.compare(0, 8, "_arcade/") == 0 && lower.find('/', 8) == std::string::npos &&
                   lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".mra") == 0) {
            type = CoreMraType::ArcadeMra; include = true;
        }
        if (!include) continue;
        const Json *hash = field(item.second, "hash"), *size = field(item.second, "size");
        if (!hash || hash->type != Json::Type::String || !hexHash(hash->text) ||
            !size || size->type != Json::Type::Number) {
            error = "invalid hash or size for distribution file: " + path; return false;
        }
        char *end = nullptr; unsigned long long bytes = std::strtoull(size->text.c_str(), &end, 10);
        if (!end || *end || bytes == 0 || bytes > 8ull * 1024 * 1024 * 1024) {
            error = "invalid file size in distribution manifest: " + path; return false;
        }
        CoreMraEntry entry;
        entry.path = path; entry.name = path.substr(path.find_last_of('/') + 1);
        entry.type = type; entry.hash = hash->text;
        std::transform(entry.hash.begin(), entry.hash.end(), entry.hash.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        entry.size = uint64_t(bytes);
        entry.url = baseUrl + "/" + encodePath(path);
        if (!Downloader::safeUrl(entry.url) || entry.url.compare(0, kDistributionPrefix.size(), kDistributionPrefix) != 0) {
            error = "unsafe distribution URL for: " + path; return false;
        }
        const Json *tangle = field(item.second, "tangle");
        if (tangle) {
            if (tangle->type != Json::Type::Array) { error = "invalid replacement list: " + path; return false; }
            for (const Json &old : tangle->array) {
                if (old.type != Json::Type::String || old.text.empty() || old.text.size() > 255) {
                    error = "invalid replacement entry: " + path; return false;
                }
                entry.replacements.push_back(old.text);
            }
        }
        entries.push_back(std::move(entry));
        if (entries.size() > 3000) { error = "distribution selection exceeds supported size"; return false; }
    }
    if (entries.empty()) { error = "distribution manifest contains no supported core or MRA files"; return false; }
    std::sort(entries.begin(), entries.end(), [](const CoreMraEntry &a, const CoreMraEntry &b) {
        if (a.type != b.type) return int(a.type) < int(b.type);
        return a.path < b.path;
    });
    return true;
}

// Saved result of the last check, so reopening the updater (also after a restart) does not
// download the list and hash every installed file again. Two small files in the GUI's own
// directory, written once at the end of a completed check:
//   core-manifest.zip  the official list exactly as downloaded
//   core-check.tsv     "v1<TAB>epoch", then "path<TAB>size<TAB>mtime<TAB>md5" per installed file
// A result is reused for 15 minutes. Every installed file is still checked with stat() against
// the saved size and modification time, and re-hashed if either differs, so a file changed in the
// meantime is never reported with an old answer. A clock that went backwards (the MiSTer has no
// battery clock) makes the result unusable rather than eternally fresh.
const char *kCacheManifestPath = MISTER_PAT_ROOT "/cache/core-manifest.zip";
const char *kCacheIndexPath = MISTER_PAT_ROOT "/cache/core-check.tsv";
const int64_t kCacheSeconds = 15 * 60;

struct HashRecord {
    uint64_t size = 0;
    int64_t mtime = 0;
    std::string md5;
    bool operator==(const HashRecord &o) const { return size == o.size && mtime == o.mtime && md5 == o.md5; }
};
struct CheckCache {
    int64_t at = 0;
    std::map<std::string, HashRecord> hashes;
};

bool loadCacheIndex(CheckCache &cache) {
    std::string text;
    if (!UpdateFiles::readLimited(kCacheIndexPath, 16u * 1024u * 1024u, text)) return false;
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line)) return false;
    long long at = 0;
    if (std::sscanf(line.c_str(), "v1\t%lld", &at) != 1 || at <= 0) return false;
    CheckCache loaded;
    loaded.at = at;
    while (std::getline(in, line)) {
        const size_t a = line.find('\t'), b = line.find('\t', a + 1), c = line.find('\t', b + 1);
        if (a == std::string::npos || b == std::string::npos || c == std::string::npos) continue;
        HashRecord record;
        record.size = std::strtoull(line.c_str() + a + 1, nullptr, 10);
        record.mtime = std::strtoll(line.c_str() + b + 1, nullptr, 10);
        record.md5 = line.substr(c + 1);
        if (!hexHash(record.md5)) continue;
        loaded.hashes[line.substr(0, a)] = record;
    }
    cache = std::move(loaded);
    return true;
}

bool cacheIsFresh(const CheckCache &cache, int64_t now) {
    return cache.at > 0 && now >= cache.at && now - cache.at <= kCacheSeconds;
}

void saveCacheIndex(const CheckCache &cache) {
    std::string text = "v1\t" + std::to_string(cache.at) + "\n";
    for (const auto &item : cache.hashes)
        text += item.first + "\t" + std::to_string(item.second.size) + "\t" +
                std::to_string(item.second.mtime) + "\t" + item.second.md5 + "\n";
    SafeFileWrite::atomicWrite(kCacheIndexPath, text);
}


// `error` is the technical reason, kept for the log and the small "Details" line. `advice`
// is what the screen tells the user: what went wrong in plain words and what to do about it.
bool loadManifest(std::atomic<bool> &cancel, std::vector<CoreMraEntry> &entries,
                  std::string &error, std::string &advice, bool useSaved, bool &fromCache) {
    fromCache = false;
    if (useSaved) {
        std::string saved, json, ignored;
        if (UpdateFiles::readLimited(kCacheManifestPath, kManifestZipLimit, saved) &&
            inflateManifest(saved, json, ignored) && parseManifest(json, entries, ignored)) {
            fromCache = true;
            return true;
        }
        entries.clear();
    }
    // A private directory instead of a pre-created file: VerifiedDownloader creates its output
    // exclusively, so a file that mkstemp() already made would always be refused.
    char directory[] = "/tmp/mister-pat-core-manifest-XXXXXX";
    if (!mkdtemp(directory)) {
        error = std::string("cannot create a temporary directory: ") + std::strerror(errno);
        advice = "The MiSTer could not prepare a temporary place for the download.";
        return false;
    }
    const std::string path = std::string(directory) + "/manifest.zip";
    auto cleanup = [&] { unlink(path.c_str()); rmdir(directory); };

    VerifiedDownloader downloader;
    const DownloadResult fetched = downloader.fetch(kManifestUrl, path, kManifestZipLimit, 60, cancel, {});
    if (!fetched.ok()) {
        cleanup();
        error = fetched.error;
        switch (fetched.code) {
        case DownloadResult::Code::Cancelled: advice = "The check was cancelled."; break;
        case DownloadResult::Code::File:
            advice = "The MiSTer could not store the download temporarily."; break;
        case DownloadResult::Code::TooLarge:
            advice = "The official list is larger than expected, so it was not used."; break;
        case DownloadResult::Code::Tool:
            advice = "The download could not be started."; break;
        default:
            advice = "The official MiSTer distribution could not be reached. "
                     "Check the network connection and try again."; break;
        }
        return false;
    }
    std::string archive;
    if (!UpdateFiles::readLimited(path, kManifestZipLimit, archive)) {
        cleanup();
        error = "cannot read downloaded distribution manifest";
        advice = "The downloaded list could not be read.";
        return false;
    }
    cleanup();
    std::string json;
    if (!inflateManifest(archive, json, error) || !parseManifest(json, entries, error)) {
        advice = "The downloaded list is not in the expected format. Try again later.";
        return false;
    }
    SafeFileWrite::atomicWrite(kCacheManifestPath, archive); // best effort; the check does not depend on it
    return true;
}

bool fileExists(const std::string &path, struct stat &st) {
    if (lstat(path.c_str(), &st) == 0) return true;
    return false;
}

} // namespace

CoreMraUpdater::~CoreMraUpdater() {
    cancel_.store(true);
    if (worker_.joinable()) worker_.join();
}

void CoreMraUpdater::joinFinished() {
    std::thread finished;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (workerRunning_ || !worker_.joinable()) return;
        finished = std::move(worker_);
    }
    finished.join();
}

bool CoreMraUpdater::startCheck(bool fresh) {
    joinFinished();
    std::lock_guard<std::mutex> lock(mutex_);
    if (workerRunning_) return false;
    forceFresh_ = fresh;
    cancel_.store(false);
    workerRunning_ = true;
    state_ = {};
    state_.phase = CoreMraPhase::Checking;
    worker_ = std::thread(&CoreMraUpdater::checkWorker, this);
    return true;
}

bool CoreMraUpdater::startInstall() {
    joinFinished();
    std::vector<CoreMraEntry> selected;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (workerRunning_ || state_.phase != CoreMraPhase::Ready) return false;
        for (const auto &entry : state_.entries) if (entry.selected) selected.push_back(entry);
        if (selected.empty()) return false;
        workerRunning_ = true;
        cancel_.store(false);
        state_.phase = CoreMraPhase::Installing;
        state_.completed = 0; state_.installed = 0; state_.failed = 0;
        state_.totalFiles = selected.size();
        state_.message = "Starting installation";
    }
    worker_ = std::thread(&CoreMraUpdater::installWorker, this, std::move(selected));
    return true;
}

void CoreMraUpdater::cancelCheck() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.phase == CoreMraPhase::Checking) cancel_.store(true);
}

void CoreMraUpdater::selectAllMissingOrOutdated() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.phase != CoreMraPhase::Ready) return;
    for (auto &entry : state_.entries)
        entry.selected = entry.status == CoreMraStatus::Missing ||
                         entry.status == CoreMraStatus::UpdateAvailable ||
                         entry.status == CoreMraStatus::Modified;
}

bool CoreMraUpdater::toggleSelected(size_t index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.phase != CoreMraPhase::Ready || index >= state_.entries.size()) return false;
    CoreMraEntry &entry = state_.entries[index];
    if (entry.status == CoreMraStatus::UpToDate || entry.status == CoreMraStatus::Error) return false;
    entry.selected = !entry.selected;
    return true;
}

CoreMraSnapshot CoreMraUpdater::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool CoreMraUpdater::busy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return workerRunning_;
}

void CoreMraUpdater::checkWorker() {
    std::vector<CoreMraEntry> entries;
    std::string error, advice;
    bool fromCache = false;
    bool fresh = false;
    CheckCache cache;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        fresh = forceFresh_;
    }
    const int64_t startedAt = int64_t(std::time(nullptr));
    const bool savedUsable = !fresh && loadCacheIndex(cache) && cacheIsFresh(cache, startedAt);
    if (!savedUsable) cache = CheckCache();
    if (!loadManifest(cancel_, entries, error, advice, savedUsable, fromCache)) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.phase = CoreMraPhase::Failed;
        state_.message = advice.empty() ? "The official update list could not be loaded." : advice;
        state_.detail = error;
        workerRunning_ = false;
        return;
    }
    if (!fromCache) cache = CheckCache(); // a new download starts a new 15-minute period
    if (!fromCache || cache.at <= 0) cache.at = startedAt;
    const std::map<std::string, HashRecord> saved = std::move(cache.hashes);
    std::map<std::string, HashRecord> hashed;
    const std::string root = "/media/fat";
    size_t completed = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.totalFiles = entries.size();
        state_.currentTotal = entries.size();
    }
    for (CoreMraEntry &entry : entries) {
        if (cancel_.load()) break;
        std::string target, targetError;
        if (!safeTarget(root, entry.path, target, targetError)) {
            entry.status = CoreMraStatus::Error;
            ++completed;
            std::lock_guard<std::mutex> lock(mutex_);
            state_.completed = completed;
            state_.recent.push_back({entry.name, entry.status});
            if (state_.recent.size() > 40) state_.recent.erase(state_.recent.begin());
            continue;
        }
        struct stat st{};
        if (fileExists(target, st)) {
            if (!S_ISREG(st.st_mode) || uint64_t(st.st_size) != entry.size) {
                entry.status = CoreMraStatus::Modified;
            } else {
                std::string actual, hashError;
                const auto known = saved.find(entry.path);
                const bool reuse = known != saved.end() && known->second.size == uint64_t(st.st_size) &&
                                   known->second.mtime == int64_t(st.st_mtime);
                if (reuse) actual = known->second.md5;
                if (!reuse && !runMd5sum(target, actual, hashError)) entry.status = CoreMraStatus::Error;
                else {
                    hashed[entry.path] = {uint64_t(st.st_size), int64_t(st.st_mtime), actual};
                    entry.status = actual == entry.hash ? CoreMraStatus::UpToDate : CoreMraStatus::Modified;
                }
            }
        } else if (errno == ENOENT) {
            entry.status = CoreMraStatus::Missing;
        } else entry.status = CoreMraStatus::Error;
        entry.selected = entry.status == CoreMraStatus::Missing ||
                         entry.status == CoreMraStatus::UpdateAvailable ||
                         entry.status == CoreMraStatus::Modified;
        ++completed;
        // Cheap per-file update (a short lock and a copy of the name); the screen shows these.
        std::lock_guard<std::mutex> lock(mutex_);
        state_.completed = completed;
        state_.recent.push_back({entry.name, entry.status});
        if (state_.recent.size() > 40) state_.recent.erase(state_.recent.begin());
    }
    if (!cancel_.load() && (!fromCache || hashed != saved)) {
        cache.hashes = hashed;
        saveCacheIndex(cache); // one small write per completed check, none when nothing changed
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (cancel_.load()) {
        state_.phase = CoreMraPhase::Failed;
        state_.message = "Update check stopped";
    } else {
        state_.checkedAt = cache.at;
        state_.fromCache = fromCache;
        state_.entries = std::move(entries);
        state_.completed = completed;
        state_.totalFiles = state_.entries.size();
        state_.phase = CoreMraPhase::Ready;
        state_.message = "Check complete";
    }
    workerRunning_ = false;
}

void CoreMraUpdater::installWorker(std::vector<CoreMraEntry> selected) {
    const std::string root = "/media/fat";
    VerifiedDownloader downloader;
    size_t installed = 0, failedCount = 0, index = 0;
    for (const CoreMraEntry &entry : selected) {
        if (cancel_.load()) break;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_.current = index + 1; state_.currentBytes = 0; state_.currentTotal = entry.size;
            state_.message = "Downloading " + entry.name;
        }
        std::string target, error;
        if (!safeTarget(root, entry.path, target, error)) {
            ++failedCount; ++index; continue;
        }
        struct statvfs space{};
        if (statvfs(root.c_str(), &space) != 0 ||
            uint64_t(space.f_bavail) * uint64_t(space.f_frsize) < entry.size + 1024u * 1024u ||
            !ensureParentDirectories(root, entry.path, error)) {
            ++failedCount; ++index; continue;
        }
        const size_t slash = target.find_last_of('/');
        const std::string dir = slash == std::string::npos ? root : target.substr(0, slash);
        const std::string temp = dir + "/.mister-pat-update-" + std::to_string(getpid()) + "-" +
                                 std::to_string(index) + ".part";
        unlink(temp.c_str());
        const DownloadResult download = downloader.fetch(entry.url, temp, entry.size, 180, cancel_,
            [&](uint64_t bytes) {
                std::lock_guard<std::mutex> lock(mutex_);
                state_.currentBytes = bytes;
            });
        if (!download.ok() || download.bytes != entry.size) {
            unlink(temp.c_str()); ++failedCount; ++index; continue;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_.message = "Verifying " + entry.name;
        }
        std::string actual;
        if (!runMd5sum(temp, actual, error) || actual != entry.hash || cancel_.load()) {
            unlink(temp.c_str()); ++failedCount; ++index; continue;
        }
        struct stat current{};
        const int targetStat = lstat(target.c_str(), &current);
        if (targetStat == 0 && !S_ISREG(current.st_mode)) {
            unlink(temp.c_str()); ++failedCount; ++index; continue;
        }
        if (targetStat != 0 && errno != ENOENT) {
            unlink(temp.c_str()); ++failedCount; ++index; continue;
        }
        if (chmod(temp.c_str(), 0644) != 0 || rename(temp.c_str(), target.c_str()) != 0) {
            unlink(temp.c_str()); ++failedCount; ++index; continue;
        }
        std::string installedHash;
        if (!runMd5sum(target, installedHash, error) || installedHash != entry.hash) {
            ++failedCount; ++index; continue;
        }
        ++installed;
        ++index;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_.installed = installed; state_.failed = failedCount; state_.completed = index;
            state_.message = "Installed " + entry.name;
        }
    }
    sync();
    std::lock_guard<std::mutex> lock(mutex_);
    state_.installed = installed; state_.failed = failedCount; state_.completed = index;
    state_.totalFiles = selected.size();
    state_.phase = failedCount ? CoreMraPhase::Failed : CoreMraPhase::Complete;
    state_.message = failedCount ? "Some files could not be installed" : "Updates complete";
    workerRunning_ = false;
}
