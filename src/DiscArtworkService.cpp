#include "DiscArtworkService.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <vector>

#include "Downloader.h"
#include "Image.h"
#include "LibretroIndex.h"
#include "Paths.h"
#include "DebugLog.h"

namespace {

constexpr int kTheGamesDbLookupVersion = 6;

bool fileExists(const std::string &path) {
    struct stat info {};
    return lstat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) && info.st_size > 0;
}

bool ensureDirectory(const std::string &path) {
    if (mkdir(path.c_str(), 0755) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat info {};
    return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode) && !S_ISLNK(info.st_mode);
}

std::string safeKey(const DiscDriveService::Snapshot &disc) {
    const std::string prefix = disc.system == DiscDriveService::System::PlayStation ? "psx-" : "saturn-";
    const bool hasIdentifier = !disc.identifier.empty();
    const std::string identity = hasIdentifier ? disc.identifier : disc.title;
    const std::string stable = disc.identifier.empty() ? LibretroIndex::normalise(disc.title)
                                                       : disc.identifier;
    std::string out = prefix;
    std::string slug;
    bool lossy = !hasIdentifier;
    for (unsigned char ch : stable) {
        const bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                             (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
        if (allowed && slug.size() < 96) slug.push_back(char(ch));
        else {
            if (!allowed || slug.size() >= 96) lossy = true;
            if (slug.size() < 96 && (slug.empty() || slug.back() != '_')) slug.push_back('_');
        }
    }
    while (!slug.empty() && slug.back() == '_') slug.pop_back();
    if (lossy && !identity.empty()) {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char ch : identity) {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        char suffix[18];
        std::snprintf(suffix, sizeof(suffix), "-%016llx",
                      static_cast<unsigned long long>(hash));
        slug += suffix;
    }
    out += slug;
    if (!disc.region.empty()) {
        out += "-r";
        for (unsigned char ch : disc.region) {
            if (std::isalnum(ch)) out.push_back(char(std::tolower(ch)));
        }
    }
    return out;
}

int regionRank(const std::string &discRegion, const std::string &gameRegionId) {
    const int id = gameRegionId.empty() ? 0 : std::atoi(gameRegionId.c_str());
    const std::string region = [&] {
        std::string value;
        for (unsigned char ch : discRegion)
            if (std::isalnum(ch)) value.push_back(char(std::tolower(ch)));
        return value;
    }();
    if (region.find("europe") != std::string::npos || region.find("australia") != std::string::npos ||
        region.find("brazil") != std::string::npos) {
        if (id == 6 || id == 7 || id == 8) return 0; // PAL
        if (id == 2) return 1;                       // US fallback
        if (id == 1) return 2;                       // generic NTSC fallback
        if (id == 4) return 3;                       // Japan
        if (id == 5) return 4;                       // Korea
        if (id == 3) return 5;                       // NTSC-C
        if (id == 9) return 6;
    } else if (region.find("japan") != std::string::npos || region.find("taiwan") != std::string::npos ||
               region.find("hongkong") != std::string::npos || region.find("china") != std::string::npos ||
               region.find("asia") != std::string::npos) {
        if (id == 4) return 0;                       // NTSC-J
        if (id == 5) return 1;                       // Korea
        if (id == 1) return 2;                       // generic NTSC
        if (id == 2) return 3;                       // US fallback
        if (id == 6 || id == 7 || id == 8) return 4; // PAL
        if (id == 3) return 5;
        if (id == 9) return 6;
    } else if (region.find("usa") != std::string::npos || region == "us" ||
               region.find("canada") != std::string::npos || region.find("mexico") != std::string::npos) {
        if (id == 2) return 0;                       // NTSC-U
        if (id == 1) return 1;                       // generic NTSC
        if (id == 3) return 2;                       // NTSC-C
        if (id == 6 || id == 7 || id == 8) return 3; // PAL
        if (id == 4) return 4;                       // Japan
        if (id == 5) return 5;
        if (id == 9) return 6;
    } else if (region.find("korea") != std::string::npos) {
        if (id == 5) return 0;
        if (id == 4) return 1;
        if (id == 2) return 2;
        if (id == 1) return 3;
        if (id == 6 || id == 7 || id == 8) return 4;
        if (id == 3) return 5;
        if (id == 9) return 6;
    }
    // A title record without region metadata must never outrank a known region.
    // This is especially important when the disc itself was positively identified
    // as PAL/NTSC-U/NTSC-J but the TGDB record says "Region Not Set".
    return id == 0 ? 100 : 8 + id;
}

bool saveJpeg(ImagePtr image, const std::string &target) {
    if (!image || !image->valid()) return false;
    const int maxEdge = 720;
    if (image->width() > maxEdge || image->height() > maxEdge) {
        int width = image->width(), height = image->height();
        if (width >= height) { height = std::max(1, int(long(height) * maxEdge / width)); width = maxEdge; }
        else { width = std::max(1, int(long(width) * maxEdge / height)); height = maxEdge; }
        image = image->scaledTo(width, height);
    }
    std::string pattern = target + ".part-XXXXXX";
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back('\0');
    const int fd = mkstemp(temporary.data());
    if (fd < 0) return false;
    const std::string part(temporary.data());
    const int flags = fcntl(fd, F_GETFD);
    if (flags >= 0) fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    std::FILE *file = fdopen(fd, "wb");
    if (!file) {
        close(fd);
        unlink(part.c_str());
        return false;
    }
    const bool saved = image->saveJpeg(file, 85);
    const bool permissions = fchmod(fd, 0644) == 0;
    const bool synced = saved && permissions && fsync(fd) == 0;
    const bool closed = std::fclose(file) == 0;
    if (!synced || !closed || rename(part.c_str(), target.c_str()) != 0) {
        unlink(part.c_str());
        return false;
    }
    return true;
}

bool appendUtf8(std::string &out, unsigned long codepoint) {
    if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
    if (codepoint <= 0x7f) out.push_back(char(codepoint));
    else if (codepoint <= 0x7ff) {
        out.push_back(char(0xc0 | (codepoint >> 6)));
        out.push_back(char(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        out.push_back(char(0xe0 | (codepoint >> 12)));
        out.push_back(char(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(char(0x80 | (codepoint & 0x3f)));
    } else {
        out.push_back(char(0xf0 | (codepoint >> 18)));
        out.push_back(char(0x80 | ((codepoint >> 12) & 0x3f)));
        out.push_back(char(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(char(0x80 | (codepoint & 0x3f)));
    }
    return true;
}

bool parseJsonString(const std::string &json, size_t quote, std::string &out, size_t &after) {
    if (quote >= json.size() || json[quote] != '"') return false;
    out.clear();
    for (size_t i = quote + 1; i < json.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(json[i]);
        if (ch == '"') { after = i + 1; return true; }
        if (ch < 0x20) return false;
        if (ch != '\\') { out.push_back(char(ch)); continue; }
        if (++i >= json.size()) return false;
        switch (json[i]) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
            auto hex = [&json](size_t at, unsigned long &value) {
                if (at + 4 > json.size()) return false;
                value = 0;
                for (size_t j = at; j < at + 4; ++j) {
                    const char digit = json[j];
                    value <<= 4;
                    if (digit >= '0' && digit <= '9') value |= digit - '0';
                    else if (digit >= 'a' && digit <= 'f') value |= digit - 'a' + 10;
                    else if (digit >= 'A' && digit <= 'F') value |= digit - 'A' + 10;
                    else return false;
                }
                return true;
            };
            unsigned long cp = 0;
            if (!hex(i + 1, cp)) return false;
            i += 4;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (i + 6 >= json.size() || json[i + 1] != '\\' || json[i + 2] != 'u') return false;
                unsigned long low = 0;
                if (!hex(i + 3, low) || low < 0xdc00 || low > 0xdfff) return false;
                cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                i += 6;
            }
            if (!appendUtf8(out, cp)) return false;
            break;
        }
        default: return false;
        }
    }
    return false;
}

size_t propertyPosition(const std::string &json, const std::string &key, size_t from = 0) {
    for (size_t i = from; i < json.size(); ++i) {
        if (json[i] != '"') continue;
        std::string candidate;
        size_t after = 0;
        if (!parseJsonString(json, i, candidate, after)) return std::string::npos;
        size_t colon = after;
        while (colon < json.size() && std::isspace(static_cast<unsigned char>(json[colon]))) ++colon;
        if (candidate == key && colon < json.size() && json[colon] == ':') return i;
        i = after - 1;
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

std::string jsonStringAt(const std::string &json, const std::string &key, size_t from = 0) {
    const size_t pos = propertyPosition(json, key, from);
    if (pos == std::string::npos) return std::string();
    const size_t colon = propertyColon(json, pos);
    if (colon == std::string::npos) return std::string();
    size_t quote = colon + 1;
    while (quote < json.size() && std::isspace(static_cast<unsigned char>(json[quote]))) ++quote;
    std::string out;
    size_t after = 0;
    if (parseJsonString(json, quote, out, after)) return out;
    return std::string();
}

std::string jsonNumberAt(const std::string &json, const std::string &key, size_t from = 0) {
    const size_t pos = propertyPosition(json, key, from);
    if (pos == std::string::npos) return std::string();
    const size_t colon = propertyColon(json, pos);
    if (colon == std::string::npos) return std::string();
    size_t begin = colon + 1;
    while (begin < json.size() && std::isspace(static_cast<unsigned char>(json[begin]))) ++begin;
    size_t end = begin;
    while (end < json.size() && json[end] >= '0' && json[end] <= '9') ++end;
    return json.substr(begin, end - begin);
}

bool writeTextAtomically(const std::string &path, const std::string &text);

bool theGamesDbCheckedRecently(const std::string &metadata, const std::string &metadataPath) {
    if (metadata.find("\"thegamesdb_checked\":true") == std::string::npos) return false;
    const std::string lookupVersion = jsonNumberAt(metadata, "thegamesdb_lookup_version");
    if (lookupVersion != std::to_string(kTheGamesDbLookupVersion)) {
        // Re-evaluate old selections after lookup, region-ranking, or fallback
        // behavior changes, even when an older TheGamesDB screenshot was cached.
        return false;
    }
    const std::string timestamp = jsonNumberAt(metadata, "thegamesdb_checked_at");
    long long checkedAt = 0;
    if (timestamp.empty()) {
        // Older cache records had no explicit timestamp. Use the regular metadata file's
        // mtime so those negative lookups expire instead of being permanent.
        struct stat info {};
        if (lstat(metadataPath.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) return false;
        checkedAt = static_cast<long long>(info.st_mtime);
    } else {
        checkedAt = std::strtoll(timestamp.c_str(), nullptr, 10);
    }
    const long long now = static_cast<long long>(time(nullptr));
    constexpr long long kNegativeCacheSeconds = 30LL * 24 * 60 * 60;
    return checkedAt <= now ? now - checkedAt < kNegativeCacheSeconds : true;
}

std::string jsonQuoted(const std::string &text) {
    static const char *hex = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char ch : text) {
        if (ch == '\\' || ch == '"') { out.push_back('\\'); out.push_back(char(ch)); }
        else if (ch == '\n') out += "\\n";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\t') out += "\\t";
        else if (ch < 0x20) {
            out += "\\u00";
            out.push_back(hex[ch >> 4]);
            out.push_back(hex[ch & 0x0f]);
        } else out.push_back(char(ch));
    }
    out.push_back('"');
    return out;
}

std::string readText(const std::string &path);
bool hasImageType(const std::string &metadata, const std::string &type);

void recordTheGamesDbCheck(const std::string &metadataPath, const std::string &title,
                           const std::string &discId, const std::string &gameId = {}) {
    const std::string existing = readText(metadataPath);
    const size_t slash = metadataPath.find_last_of('/');
    const std::string directory = slash == std::string::npos ? "." : metadataPath.substr(0, slash);
    const bool hasBoxart = fileExists(directory + "/boxart.jpg");
    bool hasScreenshots = false;
    for (int i = 1; i <= 4; ++i)
        if (fileExists(directory + "/screenshot-" + std::to_string(i) + ".jpg"))
            hasScreenshots = true;
    std::string metadata = "{\"thegamesdb_checked\":true,\"thegamesdb_checked_at\":" +
        std::to_string(static_cast<long long>(time(nullptr))) +
        ",\"thegamesdb_lookup_version\":" + std::to_string(kTheGamesDbLookupVersion) +
        ",\"provider\":\"TheGamesDB\",\"title\":" + jsonQuoted(title) +
        ",\"disc_id\":" + jsonQuoted(discId);
    const std::string releaseYear = jsonStringAt(existing, "release_year");
    if (releaseYear.size() == 4 &&
        std::all_of(releaseYear.begin(), releaseYear.end(), [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        })) metadata += ",\"release_year\":" + jsonQuoted(releaseYear);
    std::string savedGameId = gameId;
    if (savedGameId.empty()) savedGameId = jsonStringAt(existing, "thegamesdb_game_id");
    if (savedGameId.empty()) savedGameId = jsonStringAt(existing, "game_id");
    if (!savedGameId.empty()) metadata += ",\"thegamesdb_game_id\":" + jsonQuoted(savedGameId);
    std::string boxartProvider = jsonStringAt(existing, "boxart_provider");
    std::string screenshotProvider = jsonStringAt(existing, "screenshot_provider");
    const std::string legacyProvider = jsonStringAt(existing, "provider");
    if (legacyProvider == "TheGamesDB" || legacyProvider == "Libretro.com") {
        if (boxartProvider.empty() && hasImageType(existing, "boxart"))
            boxartProvider = legacyProvider;
        if (screenshotProvider.empty() && hasImageType(existing, "screenshot"))
            screenshotProvider = legacyProvider;
    }
    if (hasBoxart && !boxartProvider.empty())
        metadata += ",\"boxart_provider\":" + jsonQuoted(boxartProvider);
    if (hasScreenshots && !screenshotProvider.empty())
        metadata += ",\"screenshot_provider\":" + jsonQuoted(screenshotProvider);
    metadata += ",\"image_types\":[";
    if (hasBoxart) metadata += "\"boxart\"";
    if (hasScreenshots) {
        if (hasBoxart) metadata += ",";
        metadata += "\"screenshot\"";
    }
    metadata += "]";
    metadata += "}\n";
    writeTextAtomically(metadataPath, metadata);
}

bool hasImageType(const std::string &metadata, const std::string &type) {
    const size_t key = propertyPosition(metadata, "image_types");
    if (key == std::string::npos) return false;
    const size_t begin = metadata.find('[', key);
    const size_t end = begin == std::string::npos ? begin : metadata.find(']', begin + 1);
    return begin != std::string::npos && end != std::string::npos &&
           metadata.find(jsonQuoted(type), begin + 1) < end;
}

std::string recordArtworkMetadata(const std::string &metadata, const std::string &title,
                                  const std::string &discId, bool hasBoxart,
                                  bool hasScreenshots, std::string boxartProvider,
                                  std::string screenshotProvider,
                                  const std::string &releaseYearOverride = {},
                                  const std::string &gameIdOverride = {}) {
    const std::string legacyProvider = jsonStringAt(metadata, "provider");
    if (legacyProvider == "TheGamesDB") {
        if (boxartProvider.empty() && hasImageType(metadata, "boxart"))
            boxartProvider = legacyProvider;
        if (screenshotProvider.empty() && hasImageType(metadata, "screenshot"))
            screenshotProvider = legacyProvider;
    } else if (legacyProvider == "Libretro.com") {
        if (boxartProvider.empty() && hasImageType(metadata, "boxart"))
            boxartProvider = legacyProvider;
        if (screenshotProvider.empty() && hasImageType(metadata, "screenshot"))
            screenshotProvider = legacyProvider;
    }

    std::string result = "{\"title\":" + jsonQuoted(title) +
        ",\"disc_id\":" + jsonQuoted(discId);
    const std::string releaseYear = releaseYearOverride.empty()
        ? jsonStringAt(metadata, "release_year") : releaseYearOverride;
    if (releaseYear.size() == 4 &&
        std::all_of(releaseYear.begin(), releaseYear.end(), [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        })) result += ",\"release_year\":" + jsonQuoted(releaseYear);
    if (metadata.find("\"thegamesdb_checked\":true") != std::string::npos) {
        result += ",\"thegamesdb_checked\":true";
        const std::string checkedAt = jsonNumberAt(metadata, "thegamesdb_checked_at");
        if (!checkedAt.empty()) result += ",\"thegamesdb_checked_at\":" + checkedAt;
        const std::string lookupVersion = jsonNumberAt(metadata, "thegamesdb_lookup_version");
        if (!lookupVersion.empty()) result += ",\"thegamesdb_lookup_version\":" + lookupVersion;
    }
    std::string gameId = jsonStringAt(metadata, "thegamesdb_game_id");
    if (gameId.empty()) gameId = jsonStringAt(metadata, "game_id");
    if (!gameIdOverride.empty()) gameId = gameIdOverride;
    if (!gameId.empty()) result += ",\"thegamesdb_game_id\":" + jsonQuoted(gameId);
    if (hasBoxart && !boxartProvider.empty())
        result += ",\"boxart_provider\":" + jsonQuoted(boxartProvider);
    if (hasScreenshots && !screenshotProvider.empty())
        result += ",\"screenshot_provider\":" + jsonQuoted(screenshotProvider);
    result += ",\"image_types\":[";
    if (hasBoxart) result += "\"boxart\"";
    if (hasScreenshots) {
        if (hasBoxart) result += ",";
        result += "\"screenshot\"";
    }
    result += "]}\n";
    return result;
}

std::vector<std::string> objectsFromArray(const std::string &json, const std::string &key,
                                          size_t from = 0) {
    std::vector<std::string> objects;
    const size_t pos = propertyPosition(json, key, from);
    if (pos == std::string::npos) return objects;
    const size_t array = json.find('[', pos + key.size() + 2);
    if (array == std::string::npos) return objects;
    size_t objectStart = std::string::npos;
    int depth = 0;
    bool quoted = false, escaped = false;
    for (size_t i = array + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') quoted = false;
            continue;
        }
        if (ch == '"') { quoted = true; continue; }
        if (ch == '{') {
            if (depth++ == 0) objectStart = i;
        } else if (ch == '}' && depth > 0 && --depth == 0 && objectStart != std::string::npos) {
            objects.push_back(json.substr(objectStart, i - objectStart + 1));
            objectStart = std::string::npos;
        } else if (ch == ']' && depth == 0) break;
    }
    return objects;
}

std::vector<std::string> stringsFromArray(const std::string &json, const std::string &key,
                                          size_t from = 0) {
    std::vector<std::string> values;
    const size_t pos = propertyPosition(json, key, from);
    if (pos == std::string::npos) return values;
    const size_t colon = propertyColon(json, pos);
    if (colon == std::string::npos) return values;
    size_t array = colon + 1;
    while (array < json.size() && std::isspace(static_cast<unsigned char>(json[array]))) ++array;
    if (array >= json.size() || json[array] != '[') return values;
    size_t cursor = array + 1;
    while (cursor < json.size()) {
        while (cursor < json.size() && (std::isspace(static_cast<unsigned char>(json[cursor])) ||
                                        json[cursor] == ',')) ++cursor;
        if (cursor >= json.size() || json[cursor] == ']') break;
        if (json[cursor] != '"') {
            while (cursor < json.size() && json[cursor] != ',' && json[cursor] != ']') ++cursor;
            continue;
        }
        std::string value;
        size_t after = 0;
        if (!parseJsonString(json, cursor, value, after)) break;
        values.push_back(std::move(value));
        cursor = after;
    }
    return values;
}

std::string simpleTitle(const std::string &title, DiscDriveService::System system,
                        bool removeLeadingNamePrefixes = true) {
    std::string out;
    bool inParens = false;
    for (char ch : title) {
        if (ch == '(') { inParens = true; continue; }
        if (ch == ')') { inParens = false; continue; }
        if (!inParens) out.push_back(ch);
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
    if (removeLeadingNamePrefixes) {
        const char *prefixes[] = {"Terry Pratchett's ", "Terry Pratchett’s ", "The "};
        for (const char *prefix : prefixes) {
            const size_t length = std::char_traits<char>::length(prefix);
            if (out.size() >= length && out.compare(0, length, prefix) == 0) {
                out.erase(0, length);
                break;
            }
        }
    }
    if (system == DiscDriveService::System::PlayStation) {
        const size_t disc = out.find(" - Disc ");
        if (disc != std::string::npos) out.erase(disc);
    }
    return out;
}

std::string titleKey(const std::string &title) {
    std::string out;
    for (unsigned char ch : title) {
        // Keep UTF-8 bytes intact. Dropping them would make distinct titles such as
        // accented and unaccented spellings compare equal and could select wrong artwork.
        if (ch >= 0x80) out.push_back(char(ch));
        else if (std::isalnum(ch)) out.push_back(char(std::tolower(ch)));
    }
    return out;
}

bool isYearSuffixVariant(const std::string &candidate, const std::string &wanted) {
    if (candidate.size() != wanted.size() + 4 || candidate.compare(0, wanted.size(), wanted) != 0)
        return false;
    const std::string year = candidate.substr(wanted.size());
    return (year.compare(0, 2, "19") == 0 || year.compare(0, 2, "20") == 0) &&
        std::all_of(year.begin(), year.end(), [](unsigned char ch) { return std::isdigit(ch); });
}

std::string encodeUrlPath(const std::string &path) {
    std::string result;
    size_t start = 0;
    while (start <= path.size()) {
        const size_t slash = path.find('/', start);
        const std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!result.empty()) result.push_back('/');
        result += Downloader::encodeComponent(part);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return result;
}

std::string readText(const std::string &path) {
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return {};
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        info.st_size > 1024 * 1024) {
        close(fd);
        return {};
    }
    std::string text;
    text.reserve(static_cast<size_t>(info.st_size));
    char buffer[4096];
    for (;;) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        text.append(buffer, static_cast<size_t>(count));
        if (text.size() > 1024 * 1024) { text.clear(); break; }
    }
    close(fd);
    return text;
}

bool writeTextAtomically(const std::string &path, const std::string &text) {
    const std::string part = path + ".part";
    unlink(part.c_str());
    const int fd = open(part.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (fd < 0) return false;
    size_t written = 0;
    while (written < text.size()) {
        const ssize_t count = write(fd, text.data() + written, text.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            close(fd);
            unlink(part.c_str());
            return false;
        }
        written += static_cast<size_t>(count);
    }
    const bool synced = fsync(fd) == 0;
    const bool closed = close(fd) == 0;
    if (!synced || !closed) { unlink(part.c_str()); return false; }
    if (rename(part.c_str(), path.c_str()) != 0) { unlink(part.c_str()); return false; }
    return true;
}

struct CacheDirectory {
    std::string name;
    time_t modified = 0;
    uint64_t bytes = 0;
    dev_t device = 0;
    ino_t inode = 0;
};

void pruneDiscCache(const std::string &root, const std::string &keep) {
    const int rootFd = open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (rootFd < 0) return;
    DIR *directory = fdopendir(rootFd);
    if (!directory) { close(rootFd); return; }
    const int rootHandle = dirfd(directory);
    std::vector<CacheDirectory> games;
    uint64_t total = 0;
    while (dirent *entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name == "index") continue;
        const int childFd = openat(rootHandle, name.c_str(),
                                   O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (childFd < 0) continue;
        CacheDirectory item;
        struct stat info {};
        if (fstat(childFd, &info) != 0 || !S_ISDIR(info.st_mode)) {
            close(childFd);
            continue;
        }
        item.name = name;
        item.modified = info.st_mtime;
        item.device = info.st_dev;
        item.inode = info.st_ino;
        DIR *files = fdopendir(childFd);
        if (!files) { close(childFd); continue; }
        while (dirent *file = readdir(files)) {
            if (!std::strcmp(file->d_name, ".") || !std::strcmp(file->d_name, "..")) continue;
            struct stat fileInfo {};
            if (fstatat(dirfd(files), file->d_name, &fileInfo, AT_SYMLINK_NOFOLLOW) == 0 &&
                S_ISREG(fileInfo.st_mode))
                item.bytes += uint64_t(fileInfo.st_size);
        }
        closedir(files);
        total += item.bytes;
        games.push_back(item);
    }
    std::sort(games.begin(), games.end(), [](const CacheDirectory &a, const CacheDirectory &b) {
        return a.modified < b.modified;
    });
    // Keep the on-device artwork cache bounded. The active disc's files are never pruned.
    size_t count = games.size();
    for (const CacheDirectory &item : games) {
        if ((total <= 128ULL * 1024 * 1024 && count <= 80) ||
            root + "/" + item.name == keep) continue;
        const int childFd = openat(rootHandle, item.name.c_str(),
                                   O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (childFd < 0) continue;
        struct stat childInfo {};
        if (fstat(childFd, &childInfo) != 0 || childInfo.st_dev != item.device ||
            childInfo.st_ino != item.inode) {
            close(childFd);
            continue;
        }
        DIR *files = fdopendir(childFd);
        if (!files) { close(childFd); continue; }
        while (dirent *file = readdir(files)) {
            if (!std::strcmp(file->d_name, ".") || !std::strcmp(file->d_name, "..")) continue;
            struct stat info {};
            if (fstatat(dirfd(files), file->d_name, &info, AT_SYMLINK_NOFOLLOW) == 0 &&
                S_ISREG(info.st_mode)) unlinkat(dirfd(files), file->d_name, 0);
        }
        closedir(files);
        struct stat currentInfo {};
        if (fstatat(rootHandle, item.name.c_str(), &currentInfo, AT_SYMLINK_NOFOLLOW) == 0 &&
            S_ISDIR(currentInfo.st_mode) && currentInfo.st_dev == item.device &&
            currentInfo.st_ino == item.inode &&
            unlinkat(rootHandle, item.name.c_str(), AT_REMOVEDIR) == 0) {
            total -= item.bytes;
            --count;
        }
    }
    closedir(directory);
}

bool clearDiscArtworkFiles(const std::string &cacheKey) {
    if (cacheKey.empty() || cacheKey.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
        return false;
    const std::string cacheRoot = std::string(MISTER_PAT_ROOT) + "/disc-cache";
    const int rootFd = open(cacheRoot.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (rootFd < 0) return errno == ENOENT;
    const int gameFd = openat(rootFd, cacheKey.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    close(rootFd);
    if (gameFd < 0) return errno == ENOENT;

    const char *files[] = {
        "boxart.jpg", "screenshot-1.jpg", "screenshot-2.jpg", "screenshot-3.jpg",
        "screenshot-4.jpg", "metadata.json", "thegamesdb-details.json",
        "tgdb-search.json", "tgdb-images.json", "boxart.jpg.download",
        "screenshot-1.jpg.download", "screenshot-2.jpg.download",
        "screenshot-3.jpg.download", "screenshot-4.jpg.download"
    };
    bool success = true;
    for (const char *file : files) {
        if (unlinkat(gameFd, file, 0) != 0 && errno != ENOENT) success = false;
    }
    close(gameFd);
    return success;
}


} // namespace

DiscArtworkService::DiscArtworkService(TheGamesDBClient &client)
    : client_(client), worker_(&DiscArtworkService::workerLoop, this) {}

DiscArtworkService::~DiscArtworkService() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        cancelCurrent_.store(true);
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void DiscArtworkService::request(const DiscDriveService::Snapshot &disc) {
    if (disc.state == DiscDriveService::MediaState::Checking) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state != State::Idle && activeGeneration_ == disc.generation) return;
        cancelCurrent_.store(true);
        hasPending_ = false;
        snapshot_ = Snapshot();
        retryAt_ = {};
        activeGeneration_ = disc.generation;
        return;
    }
    if (disc.state != DiscDriveService::MediaState::Readable || disc.title.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelCurrent_.store(true);
        hasPending_ = false;
        snapshot_ = Snapshot();
        retryAt_ = {};
        activeGeneration_ = disc.generation;
        return;
    }
    // A PlayStation boot serial without a matching offline title is not a useful
    // TheGamesDB search query. Keep the physical disc launchable, but don't scrape
    // artwork against the generic display label.
    if (disc.system == DiscDriveService::System::PlayStation &&
        disc.title == "PlayStation disc") {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelCurrent_.store(true);
            hasPending_ = false;
            snapshot_ = Snapshot();
            retryAt_ = {};
            activeGeneration_ = disc.generation;
        }
        DebugLog::warn("disc artwork: skipped lookup because no offline PlayStation title was resolved");
        return;
    }
    const std::string key = safeKey(disc);
    if (key.empty()) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        activeGeneration_ = disc.generation;
        if (snapshot_.cacheKey == key && snapshot_.state != State::Idle) {
            if (retryAt_ == std::chrono::steady_clock::time_point{} ||
                std::chrono::steady_clock::now() < retryAt_) return;
            DebugLog::info("disc artwork: retrying artwork lookup after a temporary failure");
        }
        if (hasPending_ && pending_.system == disc.system &&
            pending_.identifier == disc.identifier && pending_.title == disc.title &&
            pending_.region == disc.region) return;
        cancelCurrent_.store(true);
        pending_ = {disc.generation, disc.system, disc.title, disc.identifier, disc.region};
        hasPending_ = true;
        snapshot_ = Snapshot();
        snapshot_.state = State::Searching;
        snapshot_.cacheKey = key;
        retryAt_ = {};
    }
    wake_.notify_one();
    DebugLog::info(std::string("disc artwork: lookup queued for ") +
                   DiscDriveService::systemName(disc.system));
}

void DiscArtworkService::refresh(const DiscDriveService::Snapshot &disc) {
    if (disc.state != DiscDriveService::MediaState::Readable || disc.title.empty()) {
        request(disc);
        return;
    }
    const std::string key = safeKey(disc);
    if (key.empty()) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.cacheKey == key && snapshot_.state == State::Searching) return;
        cancelCurrent_.store(true);
        hasPending_ = false;
        snapshot_ = Snapshot();
        retryAt_ = {};
        activeGeneration_ = disc.generation;
    }
    DebugLog::info("disc artwork: refreshing current disc after API-key check");
    request(disc);
}

void DiscArtworkService::reload(const DiscDriveService::Snapshot &disc) {
    if (disc.state != DiscDriveService::MediaState::Readable || disc.title.empty() ||
        disc.system == DiscDriveService::System::Unknown) {
        request(disc);
        return;
    }
    const std::string key = safeKey(disc);
    if (key.empty()) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelCurrent_.store(true);
        pending_ = {disc.generation, disc.system, disc.title, disc.identifier, disc.region, true};
        hasPending_ = true;
        snapshot_ = Snapshot();
        snapshot_.state = State::Searching;
        snapshot_.cacheKey = key;
        retryAt_ = {};
        activeGeneration_ = disc.generation;
    }
    wake_.notify_one();
    DebugLog::info(std::string("disc artwork: manual cache clear and reload queued; game=") + key);
}

DiscArtworkService::Snapshot DiscArtworkService::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void DiscArtworkService::workerLoop() {
    for (;;) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || hasPending_; });
            if (stopping_) return;
            request = pending_;
            hasPending_ = false;
            cancelCurrent_.store(false);
        }
        if (request.clearCache) {
            DiscDriveService::Snapshot disc;
            disc.system = request.system;
            disc.identifier = request.identifier;
            disc.title = request.title;
            disc.region = request.region;
            if (!clearDiscArtworkFiles(safeKey(disc))) {
                DebugLog::error("disc artwork: could not clear the current game's artwork cache");
            } else {
                DebugLog::info("disc artwork: current game's artwork cache cleared");
            }
        }
        Snapshot result = fetch(request, cancelCurrent_);
        const bool wasCancelled = cancelCurrent_.load();
        const State resultState = result.state;
        const bool hasBoxart = fileExists(result.boxart);
        const size_t screenshotCount = result.screenshots.size();
        const bool hasReleaseYear = !result.releaseYear.empty();
        DiscDriveService::Snapshot disc;
        disc.system = request.system;
        disc.identifier = request.identifier;
        disc.title = request.title;
        disc.region = request.region;
        const std::string expected = safeKey(disc);
        const std::string metadataPath = std::string(MISTER_PAT_ROOT) +
            "/disc-cache/" + expected + "/metadata.json";
        const bool retryTheGamesDb = client_.configured() &&
            !theGamesDbCheckedRecently(readText(metadataPath), metadataPath);
        bool published = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            if (!cancelCurrent_.load() && snapshot_.cacheKey == expected) {
                snapshot_ = std::move(result);
                retryAt_ = snapshot_.state == State::Missing || retryTheGamesDb
                    ? std::chrono::steady_clock::now() + std::chrono::seconds(30)
                    : std::chrono::steady_clock::time_point{};
                published = true;
            }
        }
        if (wasCancelled || !published)
            DebugLog::info("disc artwork: lookup cancelled after a media change");
        else
            DebugLog::info(std::string("disc artwork: lookup finished; state=") +
                (resultState == State::Ready ? "ready" :
                 resultState == State::Missing ? "missing" :
                 resultState == State::Searching ? "searching" : "idle") +
                "; boxart=" + (hasBoxart ? "yes" : "no") +
                "; screenshots=" + std::to_string(screenshotCount) +
                "; release-year=" + (hasReleaseYear ? "yes" : "no"));
    }
}

bool DiscArtworkService::fetchFromTheGamesDB(const Request &request, Downloader &downloader,
                                             Snapshot &out, const std::string &gameDir,
                                             const std::atomic<bool> &cancel) {
    if (cancel.load()) return false;
    const std::string apiKey = client_.readKey();
    if (apiKey.empty()) return false;
    const std::string metadataPath = gameDir + "/metadata.json";
    const std::string existingMetadata = readText(metadataPath);
    const std::string detailsPath = gameDir + "/thegamesdb-details.json";
    const std::string key = Downloader::encodeComponent(apiKey);
    if (theGamesDbCheckedRecently(existingMetadata, metadataPath)) {
        DebugLog::info("disc artwork: TheGamesDB negative/result cache is still fresh");
        return false;
    }

    const int platform = request.system == DiscDriveService::System::PlayStation ? 10 : 17;
    // Search with the actual disc title. A leading article or publisher prefix can be
    // the only string that matches a TheGamesDB alternate title (e.g. "The Story of Thor 2").
    // Candidate comparison still normalizes those prefixes below.
    const std::string queryName = simpleTitle(request.title, request.system, false);
    if (queryName.empty()) return false;
    const std::string searchUrl = "https://api.thegamesdb.net/v1.1/Games/ByGameName?apikey=" + key +
        "&name=" + Downloader::encodeComponent(queryName) +
        "&fields=players%2Coverview%2Crating%2Ccoop%2Cpublishers%2Cgenres%2Calternates";
    const std::string searchFile = gameDir + "/tgdb-search.json";
    int httpStatus = 0;
    if (!downloader.fetchWithStatus(searchUrl, searchFile, httpStatus, 25, &cancel,
                                    512u * 1024u, false) || httpStatus != 200) {
        if (!cancel.load())
            DebugLog::warn(std::string("disc artwork: TheGamesDB title request failed; HTTP=") +
                           std::to_string(httpStatus));
        return false;
    }
    const std::string searchJson = readText(searchFile);
    unlink(searchFile.c_str());
    if (cancel.load()) return false;
    if (jsonNumberAt(searchJson, "code") != "200" || jsonStringAt(searchJson, "status") != "Success") {
        DebugLog::warn("disc artwork: TheGamesDB title response was not a valid success response");
        return false;
    }

    struct Candidate {
        std::string game;
        std::string id;
        std::string platformId;
        std::string regionId;
        std::string releaseDate;
        bool primaryPlatform = false;
        int regionOrder = 0;
        int titleOrder = 0;
        std::vector<std::string> images;
        bool hasBoxart = false;
        bool hasScreenshots = false;
    };
    const std::string wantedTitle = titleKey(simpleTitle(queryName, request.system));
    std::vector<Candidate> candidates;
    const auto appendCandidates = [&](const std::string &response, bool primaryOnly) {
        const auto games = objectsFromArray(response, "games");
        for (const std::string &game : games) {
            const std::string candidateTitle = jsonStringAt(game, "game_title");
            const std::string candidateKey = titleKey(simpleTitle(candidateTitle, request.system));
            int titleOrder = candidateKey == wantedTitle ? 0 :
                isYearSuffixVariant(candidateKey, wantedTitle) ? 1 : -1;
            if (titleOrder < 0) {
                for (const std::string &alternate : stringsFromArray(game, "alternates")) {
                    const std::string alternateKey = titleKey(simpleTitle(alternate, request.system));
                    if (alternateKey == wantedTitle) { titleOrder = 2; break; }
                    if (isYearSuffixVariant(alternateKey, wantedTitle)) titleOrder = 3;
                }
            }
            if (titleOrder < 0) continue;
            const std::string candidateId = jsonNumberAt(game, "id");
            if (candidateId.empty()) continue;
            const std::string candidatePlatform = jsonNumberAt(game, "platform");
            if (primaryOnly && candidatePlatform != std::to_string(platform)) continue;
            if (std::any_of(candidates.begin(), candidates.end(), [&](const Candidate &existing) {
                return existing.id == candidateId;
            })) continue;
            Candidate candidate;
            candidate.game = game;
            candidate.id = candidateId;
            candidate.platformId = candidatePlatform;
            candidate.primaryPlatform = candidate.platformId == std::to_string(platform);
            candidate.regionId = jsonNumberAt(game, "region_id");
            candidate.releaseDate = jsonStringAt(game, "release_date");
            candidate.regionOrder = regionRank(request.region, candidate.regionId);
            candidate.titleOrder = titleOrder;
            candidates.push_back(std::move(candidate));
        }
        return games.size();
    };
    const size_t initialResultCount = appendCandidates(searchJson, false);
    const size_t primaryCandidateCount = size_t(std::count_if(candidates.begin(), candidates.end(),
        [](const Candidate &candidate) { return candidate.primaryPlatform; }));
    const size_t alternateTitleCount = size_t(std::count_if(candidates.begin(), candidates.end(),
        [](const Candidate &candidate) { return candidate.titleOrder >= 2; }));
    DebugLog::info(std::string("disc artwork: TheGamesDB title search returned ") +
        std::to_string(initialResultCount) + " results; query=\"" + queryName +
        "\"; exact-title candidates=" +
        std::to_string(candidates.size()) + "; disc-platform=" +
        std::to_string(primaryCandidateCount) + "; other-platforms=" +
        std::to_string(candidates.size() - primaryCandidateCount) +
        "; alternate-title candidates=" + std::to_string(alternateTitleCount));
    if (primaryCandidateCount == 0) {
        // Broad title searches can be capped before the Saturn/PlayStation result
        // appears. Retry only this miss with the API's platform filter, preserving
        // the initial cross-platform results for screenshot fallback. Normal hits
        // therefore keep the existing two-call lookup budget.
        const std::string platformSearchUrl = searchUrl + "&filter%5Bplatform%5D=" +
            std::to_string(platform);
        httpStatus = 0;
        if (downloader.fetchWithStatus(platformSearchUrl, searchFile, httpStatus, 25, &cancel,
                                       512u * 1024u, false) && httpStatus == 200) {
            const std::string platformJson = readText(searchFile);
            unlink(searchFile.c_str());
            if (!cancel.load() && jsonNumberAt(platformJson, "code") == "200" &&
                jsonStringAt(platformJson, "status") == "Success") {
                const size_t filteredResults = appendCandidates(platformJson, true);
                DebugLog::info(std::string("disc artwork: platform-filtered title search returned ") +
                    std::to_string(filteredResults) + " results; exact disc-platform candidates=" +
                    std::to_string(std::count_if(candidates.begin(), candidates.end(),
                        [](const Candidate &candidate) { return candidate.primaryPlatform; })));
            } else if (!cancel.load()) {
                DebugLog::warn("disc artwork: TheGamesDB platform-filtered title response was invalid");
            }
        } else if (!cancel.load()) {
            unlink(searchFile.c_str());
            DebugLog::warn(std::string("disc artwork: TheGamesDB platform-filtered title request failed; HTTP=") +
                           std::to_string(httpStatus));
        }
        if (cancel.load()) return false;
        const size_t filteredCandidateCount = size_t(std::count_if(candidates.begin(), candidates.end(),
            [](const Candidate &candidate) { return candidate.primaryPlatform; }));
        if (filteredCandidateCount == 0) {
            DebugLog::info(candidates.empty()
                ? "disc artwork: TheGamesDB has no exact-title candidates for the disc platform"
                : "disc artwork: TheGamesDB has exact-title matches, but none for the disc platform");
            recordTheGamesDbCheck(metadataPath, request.title, request.identifier);
            return false;
        }
    }

    // The search result includes platform and regional variants, but the public
    // Images endpoint only returns assets for IDs. Fetch all exact candidates in
    // one request; keep boxart/metadata on the disc platform and use another
    // platform only when the disc platform has no screenshots.
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.primaryPlatform != b.primaryPlatform) return a.primaryPlatform;
        if (a.regionOrder != b.regionOrder) return a.regionOrder < b.regionOrder;
        return a.titleOrder < b.titleOrder;
    });
    if (candidates.size() > 40) candidates.resize(40);
    std::string candidateIds;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (!candidateIds.empty()) candidateIds.push_back(',');
        candidateIds += candidates[i].id;
    }
    const std::string imageUrl = "https://api.thegamesdb.net/v1/Games/Images?apikey=" + key +
        "&games_id=" + Downloader::encodeComponent(candidateIds) +
        "&filter%5Btype%5D=boxart%2Cscreenshot";
    const std::string imageJsonPath = gameDir + "/tgdb-images.json";
    httpStatus = 0;
    const bool gotImageList = downloader.fetchWithStatus(imageUrl, imageJsonPath, httpStatus, 25,
        &cancel, 256u * 1024u, false) && httpStatus == 200;
    const std::string imageJson = gotImageList ? readText(imageJsonPath) : std::string();
    unlink(imageJsonPath.c_str());
    if (cancel.load()) return false;
    const bool validImageList = gotImageList && jsonNumberAt(imageJson, "code") == "200" &&
        jsonStringAt(imageJson, "status") == "Success";
    if (!validImageList) {
        DebugLog::warn(std::string("disc artwork: TheGamesDB batch image metadata request failed; HTTP=") +
                       std::to_string(httpStatus) + "; reason=" + downloader.lastError());
    } else {
        const size_t imagesPos = propertyPosition(imageJson, "images");
        for (Candidate &candidate : candidates) {
            candidate.images = objectsFromArray(imageJson, candidate.id,
                                                 imagesPos == std::string::npos ? 0 : imagesPos);
            for (const std::string &image : candidate.images) {
                const std::string type = jsonStringAt(image, "type");
                const std::string filename = jsonStringAt(image, "filename");
                if (filename.empty() || filename.find("..") != std::string::npos || filename[0] == '/')
                    continue;
                if (type == "boxart") {
                    const std::string side = jsonStringAt(image, "side");
                    if (side.empty() || side == "front") candidate.hasBoxart = true;
                } else if (type == "screenshot") candidate.hasScreenshots = true;
            }
        }
    }

    size_t selected = 0;
    bool foundPreferredCover = false;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i].primaryPlatform && candidates[i].hasBoxart) {
            selected = i;
            foundPreferredCover = true;
            break;
        }
    }
    if (!foundPreferredCover) {
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (candidates[i].primaryPlatform && candidates[i].hasScreenshots) {
                selected = i;
                break;
            }
        }
    }
    size_t screenshotSelected = selected;
    bool foundScreenshot = false;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i].primaryPlatform && candidates[i].hasScreenshots) {
            screenshotSelected = i;
            foundScreenshot = true;
            break;
        }
    }
    if (!foundScreenshot) {
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (!candidates[i].primaryPlatform && candidates[i].hasScreenshots) {
                screenshotSelected = i;
                foundScreenshot = true;
                break;
            }
        }
    }
    std::vector<size_t> screenshotCandidateOrder;
    if (foundScreenshot) screenshotCandidateOrder.push_back(screenshotSelected);
    for (size_t i = 0; i < candidates.size() && screenshotCandidateOrder.size() < 2; ++i) {
        if (!candidates[i].hasScreenshots || i == screenshotSelected) continue;
        screenshotCandidateOrder.push_back(i);
    }
    Candidate &chosen = candidates[selected];
    Candidate &screenshotVariant = candidates[screenshotSelected];
    const std::string gameId = chosen.id;
    const std::string selectedGame = chosen.game;
    std::string releaseYear;
    if (chosen.releaseDate.size() >= 4 &&
        std::all_of(chosen.releaseDate.begin(), chosen.releaseDate.begin() + 4, [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        })) releaseYear = chosen.releaseDate.substr(0, 4);
    DebugLog::info(std::string("disc artwork: TheGamesDB selected game=") + gameId +
        "; platform=" + chosen.platformId +
        "; region-id=" + (chosen.regionId.empty() ? "unknown" : chosen.regionId) +
        "; disc-region=" + (request.region.empty() ? "unknown" : request.region) +
        "; has-cover=" + (chosen.hasBoxart ? "yes" : "no") +
        "; has-screenshots=" + (chosen.hasScreenshots ? "yes" : "no") +
        "; screenshot-game=" + (foundScreenshot ? screenshotVariant.id : "none") +
        "; screenshot-platform=" + (foundScreenshot ? screenshotVariant.platformId : "none"));
    if (validImageList) {
        size_t screenshotAssets = 0;
        for (const Candidate &candidate : candidates) {
            screenshotAssets += size_t(std::count_if(candidate.images.begin(), candidate.images.end(),
                [](const std::string &image) { return jsonStringAt(image, "type") == "screenshot"; }));
        }
        DebugLog::info(std::string("disc artwork: TheGamesDB image metadata parsed; candidates=") +
            std::to_string(candidates.size()) + "; screenshot-assets=" +
            std::to_string(screenshotAssets));
    }
    out.releaseYear = releaseYear;
    out.players = jsonNumberAt(selectedGame, "players");
    out.overview = jsonStringAt(selectedGame, "overview");
    out.rating = jsonStringAt(selectedGame, "rating");
    out.coop = jsonStringAt(selectedGame, "coop");
    const auto serializeDetails = [&](const std::vector<size_t> &screenshotSources) {
        const Candidate &primaryScreenshot = screenshotSources.empty()
            ? screenshotVariant : candidates[screenshotSources.front()];
        std::string json = "{\"game_id\":" + jsonQuoted(gameId) +
            ",\"region_id\":" + (chosen.regionId.empty() ? "null" : chosen.regionId) +
            ",\"country_id\":" + (jsonNumberAt(selectedGame, "country_id").empty()
                ? "null" : jsonNumberAt(selectedGame, "country_id")) +
            ",\"release_date\":" + jsonQuoted(chosen.releaseDate) +
            ",\"players\":" + jsonQuoted(out.players) +
            ",\"overview\":" + jsonQuoted(out.overview) +
            ",\"rating\":" + jsonQuoted(out.rating) +
            ",\"coop\":" + jsonQuoted(out.coop) +
            ",\"screenshot_game_id\":" + (foundScreenshot ? jsonQuoted(primaryScreenshot.id) : "null") +
            ",\"screenshot_region_id\":" + (foundScreenshot && !primaryScreenshot.regionId.empty()
                ? primaryScreenshot.regionId : "null") +
            ",\"screenshot_platform_id\":" + (foundScreenshot && !primaryScreenshot.platformId.empty()
                ? primaryScreenshot.platformId : "null") +
            ",\"screenshot_game_ids\":[";
        for (size_t i = 0; i < screenshotSources.size(); ++i) {
            if (i) json += ",";
            json += jsonQuoted(candidates[screenshotSources[i]].id);
        }
        json += "],\"game\":" + selectedGame + "}\n";
        return json;
    };
    if (!writeTextAtomically(detailsPath, serializeDetails({})))
        DebugLog::warn("disc artwork: could not save TheGamesDB game details");
    else
        DebugLog::info(std::string("disc artwork: cached TheGamesDB metadata with title search; players=") +
            (out.players.empty() ? "no" : "yes") + "; overview=" +
            (out.overview.empty() ? "no" : "yes") + "; rating=" +
            (out.rating.empty() ? "no" : "yes") + "; coop=" +
            (out.coop.empty() ? "no" : "yes"));
    const std::string priorMetadata = readText(metadataPath);
    const std::string metadataWithYear = recordArtworkMetadata(priorMetadata, request.title,
        request.identifier, fileExists(out.boxart), !out.screenshots.empty(),
        jsonStringAt(priorMetadata, "boxart_provider"),
        jsonStringAt(priorMetadata, "screenshot_provider"), releaseYear, gameId);
    writeTextAtomically(metadataPath, metadataWithYear);

    if (!validImageList) return false;
    const size_t dataPos = propertyPosition(imageJson, "data");
    std::string baseUrl = jsonStringAt(imageJson, "original", dataPos == std::string::npos ? 0 : dataPos);
    if (baseUrl.empty() || baseUrl.compare(0, 8, "https://") != 0) {
        DebugLog::warn("disc artwork: TheGamesDB image host is missing or not HTTPS");
        return false;
    }
    const size_t hostEnd = baseUrl.find('/', 8);
    const std::string host = baseUrl.substr(8, hostEnd == std::string::npos ? std::string::npos : hostEnd - 8);
    if (host != "cdn.thegamesdb.net") {
        DebugLog::warn("disc artwork: TheGamesDB image host did not match the allowed CDN");
        return false;
    }
    if (baseUrl.back() != '/') baseUrl.push_back('/');

    const auto &images = chosen.images;
    const std::string boxPath = gameDir + "/boxart.jpg";
    std::vector<std::string> screenshotPaths;
    std::vector<size_t> downloadedScreenshotSources;
    std::string boxFilename;
    for (const std::string &image : images) {
        const std::string type = jsonStringAt(image, "type");
        const std::string filename = jsonStringAt(image, "filename");
        if (filename.empty() || filename.find("..") != std::string::npos || filename[0] == '/') continue;
        if (type == "boxart" && boxFilename.empty()) {
            const std::string side = jsonStringAt(image, "side");
            if (side.empty() || side == "front") boxFilename = filename;
        }
    }
    if (boxFilename.empty() && screenshotCandidateOrder.empty()) {
        DebugLog::info("disc artwork: TheGamesDB match has no supported boxart or screenshots");
        recordTheGamesDbCheck(metadataPath, request.title, request.identifier, gameId);
        return false;
    }
    const auto downloadImage = [&](const std::string &filename, const std::string &target) {
        if (filename.empty() || cancel.load()) return false;
        const std::string url = baseUrl + encodeUrlPath(filename);
        if (!Downloader::safeUrl(url)) return false;
        const std::string temp = target + ".download";
        unlink(temp.c_str());
        int imageStatus = 0;
        if (!downloader.fetchWithStatus(url, temp, imageStatus, 45, &cancel,
                                       12u * 1024u * 1024u) ||
            imageStatus != 200) {
            unlink(temp.c_str());
            return false;
        }
        if (cancel.load()) { unlink(temp.c_str()); return false; }
        struct stat info {};
        const bool valid = stat(temp.c_str(), &info) == 0 && info.st_size > 0 &&
                           info.st_size <= 12 * 1024 * 1024 && saveJpeg(Image::load(temp), target);
        unlink(temp.c_str());
        return valid;
    };
    bool gotBoxart = !boxFilename.empty() && downloadImage(boxFilename, boxPath);
    if (gotBoxart) out.boxart = boxPath;
    for (size_t sourceIndex : screenshotCandidateOrder) {
        size_t attempted = 0;
        size_t downloadedFromSource = 0;
        for (const std::string &image : candidates[sourceIndex].images) {
            if (cancel.load()) return false;
            if (jsonStringAt(image, "type") != "screenshot") continue;
            const std::string filename = jsonStringAt(image, "filename");
            if (filename.empty() || filename.find("..") != std::string::npos || filename[0] == '/')
                continue;
            // Try up to twelve reported files for each of the two best variants,
            // but cache no more than four validated screenshots overall.
            if (attempted++ >= 12 || screenshotPaths.size() >= 4) break;
            const std::string target = gameDir + "/screenshot-" +
                std::to_string(screenshotPaths.size() + 1) + ".jpg";
            if (downloadImage(filename, target)) {
                screenshotPaths.push_back(target);
                ++downloadedFromSource;
                if (downloadedScreenshotSources.empty() ||
                    downloadedScreenshotSources.back() != sourceIndex)
                    downloadedScreenshotSources.push_back(sourceIndex);
            }
        }
        if (attempted > 0 && downloadedFromSource == 0) {
            DebugLog::warn(std::string("disc artwork: TheGamesDB screenshot files could not be downloaded or decoded; game=") +
                candidates[sourceIndex].id + "; attempted=" + std::to_string(attempted) +
                "; reason=" + (downloader.lastError().empty() ? "invalid image data" : downloader.lastError()));
        } else if (downloadedFromSource > 0) {
            DebugLog::info(std::string("disc artwork: TheGamesDB screenshots downloaded; game=") +
                candidates[sourceIndex].id + "; count=" + std::to_string(downloadedFromSource));
        }
        if (screenshotPaths.size() >= 4) break;
    }
    out.screenshots = screenshotPaths;
    if (!downloadedScreenshotSources.empty() &&
        !writeTextAtomically(detailsPath, serializeDetails(downloadedScreenshotSources)))
        DebugLog::warn("disc artwork: could not update TheGamesDB screenshot source details");
    if (gotBoxart || !screenshotPaths.empty()) {
        std::string json = "{\"thegamesdb_checked\":true,\"thegamesdb_checked_at\":" +
            std::to_string(static_cast<long long>(time(nullptr))) +
            ",\"thegamesdb_lookup_version\":" + std::to_string(kTheGamesDbLookupVersion) +
            ",\"provider\":\"TheGamesDB\",\"game_id\":" + jsonQuoted(gameId) +
            ",\"title\":" + jsonQuoted(request.title) +
            ",\"disc_id\":" + jsonQuoted(request.identifier);
        if (!releaseYear.empty()) json += ",\"release_year\":" + jsonQuoted(releaseYear);
        json += ",\"image_types\":[";
        bool first = true;
        if (gotBoxart) { json += "\"boxart\""; first = false; }
        if (!screenshotPaths.empty()) { if (!first) json += ","; json += "\"screenshot\""; }
        json += "]}\n";
        writeTextAtomically(metadataPath, json);
        DebugLog::info(std::string("disc artwork: downloaded TheGamesDB images; boxart=") +
            (gotBoxart ? "yes" : "no") + "; screenshots=" +
            std::to_string(screenshotPaths.size()));
        out.state = State::Ready;
        return true;
    }
    DebugLog::warn("disc artwork: TheGamesDB image downloads produced no valid images");
    return false;
}

DiscArtworkService::Snapshot DiscArtworkService::fetch(const Request &request,
                                                       const std::atomic<bool> &cancel) {
    const std::string platformKey = request.system == DiscDriveService::System::PlayStation ? "PSX" : "Saturn";
    DiscDriveService::Snapshot disc;
    disc.system = request.system;
    disc.identifier = request.identifier;
    disc.title = request.title;
    disc.region = request.region;
    const std::string key = safeKey(disc);
    const std::string cacheRoot = std::string(MISTER_PAT_ROOT) + "/disc-cache";
    const std::string gameDir = cacheRoot + "/" + key;
    Snapshot out;
    out.cacheKey = key;
    out.boxart = gameDir + "/boxart.jpg";
    if (!ensureDirectory(MISTER_PAT_ROOT) || !ensureDirectory(cacheRoot) ||
        !ensureDirectory(gameDir)) {
        DebugLog::error("disc artwork: cache directories are unavailable or unsafe");
        out.state = State::Missing;
        return out;
    }
    const std::string metadataPath = gameDir + "/metadata.json";
    struct utimbuf accessed {};
    accessed.actime = time(nullptr);
    accessed.modtime = accessed.actime;
    utime(gameDir.c_str(), &accessed);
    std::string metadata = readText(metadataPath);
    out.releaseYear = jsonStringAt(metadata, "release_year");
    const auto loadTheGamesDbDetails = [&] {
        const std::string details = readText(gameDir + "/thegamesdb-details.json");
        if (jsonStringAt(details, "game_id") != jsonStringAt(metadata, "thegamesdb_game_id"))
            return;
        out.players = jsonStringAt(details, "players");
        out.overview = jsonStringAt(details, "overview");
        out.rating = jsonStringAt(details, "rating");
        out.coop = jsonStringAt(details, "coop");
    };
    loadTheGamesDbDetails();
    std::string boxartProvider = jsonStringAt(metadata, "boxart_provider");
    std::string screenshotProvider = jsonStringAt(metadata, "screenshot_provider");
    const auto inferLegacyProviders = [&] {
        const std::string provider = jsonStringAt(metadata, "provider");
        if (provider == "TheGamesDB" || provider == "Libretro.com") {
            if (boxartProvider.empty() && hasImageType(metadata, "boxart"))
                boxartProvider = provider;
            if (screenshotProvider.empty() && hasImageType(metadata, "screenshot"))
                screenshotProvider = provider;
        }
    };
    inferLegacyProviders();
    const std::string screenshot = gameDir + "/screenshot-1.jpg";
    const auto refreshScreenshots = [&] {
        out.screenshots.clear();
        for (int i = 1; i <= 4; ++i) {
            const std::string shot = gameDir + "/screenshot-" + std::to_string(i) + ".jpg";
            if (fileExists(shot)) out.screenshots.push_back(shot);
        }
    };
    refreshScreenshots();
    DebugLog::info(std::string("disc artwork: cache lookup; boxart=") +
        (fileExists(out.boxart) ? "hit" : "miss") +
        "; screenshots=" + std::to_string(out.screenshots.size()) +
        "; release-year=" + (out.releaseYear.empty() ? "miss" : "hit"));
    if (fileExists(out.boxart) && !out.screenshots.empty()) out.state = State::Ready;

    Downloader downloader;
    const std::string indexCache = cacheRoot + "/index";
    const bool indexCacheReady = ensureDirectory(indexCache);
    const bool tgdbChecked = theGamesDbCheckedRecently(metadata, metadataPath);
    const bool hasApiKey = client_.configured();
    if (!cancel.load() && hasApiKey && !tgdbChecked) {
        const bool gotTheGamesDbImage =
            fetchFromTheGamesDB(request, downloader, out, gameDir, cancel);
        metadata = readText(metadataPath);
        if (out.releaseYear.empty()) out.releaseYear = jsonStringAt(metadata, "release_year");
        loadTheGamesDbDetails();
        refreshScreenshots();
        inferLegacyProviders();
        if (gotTheGamesDbImage) {
            if (hasImageType(metadata, "boxart")) boxartProvider = "TheGamesDB";
            if (hasImageType(metadata, "screenshot")) screenshotProvider = "TheGamesDB";
        }
        if (gotTheGamesDbImage && !out.screenshots.empty() && fileExists(out.boxart)) {
            const std::string saved = recordArtworkMetadata(metadata, request.title,
                request.identifier, fileExists(out.boxart), true, boxartProvider, screenshotProvider);
            if (writeTextAtomically(metadataPath, saved)) metadata = saved;
            pruneDiscCache(cacheRoot, gameDir);
            return out;
        }
    } else if (!cancel.load()) {
        DebugLog::info(std::string("disc artwork: skipping TheGamesDB; key=") +
            (hasApiKey ? "configured" : "not-configured") +
            "; negative-cache=" + (tgdbChecked ? "fresh" : "stale-or-missing"));
    }
    if (cancel.load()) return out;
    if (fileExists(out.boxart) && !out.screenshots.empty()) {
        out.state = State::Ready;
        const std::string saved = recordArtworkMetadata(metadata, request.title,
            request.identifier, true, true, boxartProvider, screenshotProvider);
        if (writeTextAtomically(metadataPath, saved)) metadata = saved;
        pruneDiscCache(cacheRoot, gameDir);
        return out;
    }
    if (!indexCacheReady) {
        DebugLog::warn("disc artwork: Libretro lookup skipped because its cache directory is unavailable");
        out.state = out.screenshots.empty() && !fileExists(out.boxart) ? State::Missing : State::Ready;
        const std::string saved = recordArtworkMetadata(metadata, request.title,
            request.identifier, fileExists(out.boxart), !out.screenshots.empty(),
            boxartProvider, screenshotProvider);
        if (writeTextAtomically(metadataPath, saved)) metadata = saved;
        return out;
    }
    LibretroIndex index;
    if (!index.open(LibretroIndex::platformFor(platformKey), downloader, indexCache,
                    "Named_Boxarts", &cancel, true)) {
        if (cancel.load()) return out;
        DebugLog::warn(std::string("disc artwork: could not open the Libretro artwork index; reason=") +
                       index.lastError());
        out.state = out.screenshots.empty() && !fileExists(out.boxart) ? State::Missing : State::Ready;
        const std::string saved = recordArtworkMetadata(metadata, request.title,
            request.identifier, fileExists(out.boxart), !out.screenshots.empty(),
            boxartProvider, screenshotProvider);
        if (writeTextAtomically(metadataPath, saved)) metadata = saved;
        return out;
    }
    if (cancel.load()) return out;
    std::string imageName = index.match(request.title);
    if (imageName.empty()) imageName = index.matchExact(request.title);
    DebugLog::info(std::string("disc artwork: Libretro boxart index match=") +
                   (imageName.empty() ? "miss" : "hit"));
    if (!imageName.empty() && !fileExists(out.boxart)) {
        const std::string url = std::string(LibretroIndex::kHost) + "/" +
            Downloader::encodeComponent(LibretroIndex::platformFor(platformKey)) +
            "/Named_Boxarts/" + Downloader::encodeComponent(imageName + ".png");
        const std::string temp = gameDir + "/boxart.download";
        int imageStatus = 0;
        const bool fetched = !cancel.load() &&
            downloader.fetchWithStatus(url, temp, imageStatus, 45, &cancel,
                                       12u * 1024u * 1024u, true) && imageStatus == 200;
        if (fetched) {
            if (cancel.load()) { unlink(temp.c_str()); return out; }
            struct stat info {};
            if (stat(temp.c_str(), &info) == 0 && info.st_size > 0 && info.st_size <= 12 * 1024 * 1024 &&
                saveJpeg(Image::load(temp), out.boxart)) boxartProvider = "Libretro.com";
            unlink(temp.c_str());
        } else if (!cancel.load()) {
            DebugLog::warn(std::string("disc artwork: Libretro boxart download failed; HTTP=") +
                           std::to_string(imageStatus) + "; reason=" + downloader.lastError());
        }
        if (fetched && !fileExists(out.boxart))
            DebugLog::warn("disc artwork: downloaded Libretro boxart could not be decoded or saved as JPEG");
    }
    if (!cancel.load() && !imageName.empty() && out.screenshots.empty() && !fileExists(screenshot)) {
        LibretroIndex snaps;
        std::string snapName = imageName;
        const bool hasScreenshotIndex = snaps.open(LibretroIndex::platformFor(platformKey),
            downloader, indexCache, "Named_Snaps", &cancel, true);
        if (hasScreenshotIndex && !cancel.load()) {
            std::string match = snaps.match(request.title);
            if (match.empty()) match = snaps.matchExact(request.title);
            if (!match.empty()) snapName = match;
        } else if (!cancel.load()) {
            DebugLog::warn(std::string("disc artwork: could not open Libretro screenshot index; reason=") +
                           snaps.lastError());
        }
        const std::string url = std::string(LibretroIndex::kHost) + "/" +
            Downloader::encodeComponent(LibretroIndex::platformFor(platformKey)) +
            "/Named_Snaps/" + Downloader::encodeComponent(snapName + ".png");
        const std::string temp = gameDir + "/screenshot.download";
        int imageStatus = 0;
        const bool fetched = !cancel.load() &&
            downloader.fetchWithStatus(url, temp, imageStatus, 45, &cancel,
                                       12u * 1024u * 1024u, true) && imageStatus == 200;
        if (fetched) {
            if (cancel.load()) { unlink(temp.c_str()); return out; }
            struct stat info {};
            if (stat(temp.c_str(), &info) == 0 && info.st_size > 0 && info.st_size <= 12 * 1024 * 1024 &&
                saveJpeg(Image::load(temp), screenshot)) screenshotProvider = "Libretro.com";
            unlink(temp.c_str());
        } else if (!cancel.load()) {
            DebugLog::warn(std::string("disc artwork: Libretro screenshot download failed; HTTP=") +
                           std::to_string(imageStatus) + "; reason=" + downloader.lastError());
        }
        if (fetched && !fileExists(screenshot))
            DebugLog::warn("disc artwork: downloaded Libretro screenshot could not be decoded or saved as JPEG");
        DebugLog::info(std::string("disc artwork: Libretro screenshot fetch=") +
                       (fileExists(screenshot) ? "available" : "missing"));
    }
    refreshScreenshots();
    if (fileExists(out.boxart) || !out.screenshots.empty()) out.state = State::Ready;
    else out.state = State::Missing;
    const std::string saved = recordArtworkMetadata(metadata, request.title,
        request.identifier, fileExists(out.boxart), !out.screenshots.empty(),
        boxartProvider, screenshotProvider);
    if (writeTextAtomically(metadataPath, saved)) metadata = saved;
    pruneDiscCache(cacheRoot, gameDir);
    return out;
}
