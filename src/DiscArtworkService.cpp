#include "DiscArtworkService.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <vector>

#include "Downloader.h"
#include "Image.h"
#include "LibretroIndex.h"
#include "Paths.h"

namespace {

bool fileExists(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) && info.st_size > 0;
}

bool ensureDirectory(const std::string &path) {
    if (mkdir(path.c_str(), 0755) == 0) return true;
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

std::string safeKey(const DiscDriveService::Snapshot &disc) {
    const std::string prefix = disc.system == DiscDriveService::System::PlayStation ? "psx-" : "saturn-";
    const std::string stable = disc.identifier.empty() ? LibretroIndex::normalise(disc.title)
                                                       : disc.identifier;
    std::string out = prefix;
    for (unsigned char ch : stable) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') out.push_back(char(ch));
        else if (out.back() != '_') out.push_back('_');
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
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
    const std::string part = target + ".part";
    unlink(part.c_str());
    if (!image || !image->saveJpeg(part, 85)) { unlink(part.c_str()); return false; }
    if (rename(part.c_str(), target.c_str()) != 0) { unlink(part.c_str()); return false; }
    return true;
}

size_t propertyPosition(const std::string &json, const std::string &key, size_t from = 0) {
    return json.find("\"" + key + "\"", from);
}

std::string jsonStringAt(const std::string &json, const std::string &key, size_t from = 0) {
    const size_t pos = propertyPosition(json, key, from);
    if (pos == std::string::npos) return std::string();
    const size_t colon = json.find(':', pos + key.size() + 2);
    if (colon == std::string::npos) return std::string();
    const size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos) return std::string();
    std::string out;
    bool escaped = false;
    for (size_t i = quote + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaped) {
            if (ch == 'n') out.push_back('\n');
            else if (ch == 'r') out.push_back('\r');
            else out.push_back(ch);
            escaped = false;
        } else if (ch == '\\') escaped = true;
        else if (ch == '"') return out;
        else out.push_back(ch);
    }
    return std::string();
}

std::string jsonNumberAt(const std::string &json, const std::string &key, size_t from = 0) {
    const size_t pos = propertyPosition(json, key, from);
    if (pos == std::string::npos) return std::string();
    const size_t colon = json.find(':', pos + key.size() + 2);
    if (colon == std::string::npos) return std::string();
    size_t begin = colon + 1;
    while (begin < json.size() && std::isspace(static_cast<unsigned char>(json[begin]))) ++begin;
    size_t end = begin;
    while (end < json.size() && json[end] >= '0' && json[end] <= '9') ++end;
    return json.substr(begin, end - begin);
}

bool writeTextAtomically(const std::string &path, const std::string &text);

bool theGamesDbCheckedRecently(const std::string &metadata) {
    if (metadata.find("\"thegamesdb_checked\":true") == std::string::npos) return false;
    const std::string timestamp = jsonNumberAt(metadata, "thegamesdb_checked_at");
    if (timestamp.empty()) return true; // Metadata written by earlier builds has no expiry.
    const long long checkedAt = std::strtoll(timestamp.c_str(), nullptr, 10);
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

void recordTheGamesDbCheck(const std::string &metadataPath, const std::string &title,
                           const std::string &discId, const std::string &gameId = {}) {
    std::string metadata = "{\"thegamesdb_checked\":true,\"thegamesdb_checked_at\":" +
        std::to_string(static_cast<long long>(time(nullptr))) +
        ",\"provider\":\"TheGamesDB\",\"title\":" + jsonQuoted(title) +
        ",\"disc_id\":" + jsonQuoted(discId);
    if (!gameId.empty()) metadata += ",\"game_id\":" + jsonQuoted(gameId);
    metadata += "}\n";
    writeTextAtomically(metadataPath, metadata);
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

std::string simpleTitle(const std::string &title, DiscDriveService::System system) {
    std::string out;
    bool inParens = false;
    for (char ch : title) {
        if (ch == '(') { inParens = true; continue; }
        if (ch == ')') { inParens = false; continue; }
        if (!inParens) out.push_back(ch);
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
    const char *prefixes[] = {"Terry Pratchett's ", "Terry Pratchett’s ", "The "};
    for (const char *prefix : prefixes) {
        const size_t length = std::char_traits<char>::length(prefix);
        if (out.size() >= length && out.compare(0, length, prefix) == 0) {
            out.erase(0, length);
            break;
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
    for (unsigned char ch : title)
        if (std::isalnum(ch)) out.push_back(char(std::tolower(ch)));
    return out;
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
    std::ifstream input(path.c_str(), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool writeTextAtomically(const std::string &path, const std::string &text) {
    const std::string part = path + ".part";
    {
        std::ofstream output(part.c_str(), std::ios::out | std::ios::trunc);
        if (!output) return false;
        output << text;
        if (!output) { unlink(part.c_str()); return false; }
    }
    if (rename(part.c_str(), path.c_str()) != 0) { unlink(part.c_str()); return false; }
    return true;
}

struct CacheDirectory {
    std::string path;
    time_t modified = 0;
    uint64_t bytes = 0;
};

void pruneDiscCache(const std::string &root, const std::string &keep) {
    DIR *directory = opendir(root.c_str());
    if (!directory) return;
    std::vector<CacheDirectory> games;
    uint64_t total = 0;
    while (dirent *entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name == "index") continue;
        CacheDirectory item;
        item.path = root + "/" + name;
        struct stat info {};
        if (lstat(item.path.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) continue;
        item.modified = info.st_mtime;
        DIR *files = opendir(item.path.c_str());
        if (!files) continue;
        while (dirent *file = readdir(files)) {
            if (!std::strcmp(file->d_name, ".") || !std::strcmp(file->d_name, "..")) continue;
            struct stat fileInfo {};
            const std::string path = item.path + "/" + file->d_name;
            if (lstat(path.c_str(), &fileInfo) == 0 && S_ISREG(fileInfo.st_mode))
                item.bytes += uint64_t(fileInfo.st_size);
        }
        closedir(files);
        total += item.bytes;
        games.push_back(item);
    }
    closedir(directory);
    std::sort(games.begin(), games.end(), [](const CacheDirectory &a, const CacheDirectory &b) {
        return a.modified < b.modified;
    });
    // Keep the on-device artwork cache bounded. The active disc's files are never pruned.
    size_t count = games.size();
    for (const CacheDirectory &item : games) {
        if ((total <= 128ULL * 1024 * 1024 && count <= 80) || item.path == keep) continue;
        DIR *files = opendir(item.path.c_str());
        if (!files) continue;
        while (dirent *file = readdir(files)) {
            if (!std::strcmp(file->d_name, ".") || !std::strcmp(file->d_name, "..")) continue;
            const std::string path = item.path + "/" + file->d_name;
            struct stat info {};
            if (lstat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode)) unlink(path.c_str());
        }
        closedir(files);
        if (rmdir(item.path.c_str()) == 0) {
            total -= item.bytes;
            --count;
        }
    }
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
    if (disc.state != DiscDriveService::MediaState::Readable || disc.title.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelCurrent_.store(true);
        hasPending_ = false;
        snapshot_ = Snapshot();
        return;
    }
    // A PlayStation boot serial without a matching offline title is not a useful
    // TheGamesDB search query. Keep the physical disc launchable, but don't scrape
    // artwork against the generic display label.
    if (disc.system == DiscDriveService::System::PlayStation &&
        disc.title == "PlayStation disc") {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelCurrent_.store(true);
        hasPending_ = false;
        snapshot_ = Snapshot();
        return;
    }
    const std::string key = safeKey(disc);
    if (key.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.cacheKey == key && snapshot_.state != State::Idle) return;
    if (hasPending_ && pending_.system == disc.system &&
        pending_.identifier == disc.identifier && pending_.title == disc.title) return;
    cancelCurrent_.store(true);
    pending_ = {disc.generation, disc.system, disc.title, disc.identifier};
    hasPending_ = true;
    snapshot_ = {State::Searching, key, std::string(), {}};
    wake_.notify_one();
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
        Snapshot result = fetch(request, cancelCurrent_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            const std::string key = request.system == DiscDriveService::System::PlayStation ? "psx-" : "saturn-";
            std::string safe;
            const std::string stable = request.identifier.empty() ? LibretroIndex::normalise(request.title)
                                                                  : request.identifier;
            for (unsigned char ch : stable) {
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') safe.push_back(char(ch));
                else if (!safe.empty() && safe.back() != '_') safe.push_back('_');
            }
            std::string expected = key + safe;
            while (!expected.empty() && expected.back() == '_') expected.pop_back();
            if (!cancelCurrent_.load() && snapshot_.cacheKey == expected)
                snapshot_ = std::move(result);
        }
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
    if (theGamesDbCheckedRecently(existingMetadata))
        return false;

    const int platform = request.system == DiscDriveService::System::PlayStation ? 10 : 17;
    const std::string queryName = simpleTitle(request.title, request.system);
    if (queryName.empty()) return false;
    const std::string key = Downloader::encodeComponent(apiKey);
    const std::string searchUrl = "https://api.thegamesdb.net/v1.1/Games/ByGameName?apikey=" + key +
        "&name=" + Downloader::encodeComponent(queryName) + "&filter%5Bplatform%5D=" +
        std::to_string(platform);
    const std::string searchFile = gameDir + "/tgdb-search.json";
    int httpStatus = 0;
    if (!downloader.fetchWithStatus(searchUrl, searchFile, httpStatus, 25, &cancel) || httpStatus != 200)
        return false;
    const std::string searchJson = readText(searchFile);
    unlink(searchFile.c_str());
    if (cancel.load()) return false;
    if (jsonNumberAt(searchJson, "code") != "200" || jsonStringAt(searchJson, "status") != "Success")
        return false;

    const std::string wantedTitle = titleKey(queryName);
    std::string gameId;
    bool ambiguous = false;
    for (const std::string &game : objectsFromArray(searchJson, "games")) {
        if (jsonNumberAt(game, "platform") != std::to_string(platform)) continue;
        const std::string candidateTitle = jsonStringAt(game, "game_title");
        if (titleKey(simpleTitle(candidateTitle, request.system)) != wantedTitle) continue;
        const std::string candidateId = jsonNumberAt(game, "id");
        if (candidateId.empty()) continue;
        if (gameId.empty()) gameId = candidateId;
        else if (gameId != candidateId) ambiguous = true;
    }
    if (gameId.empty() || ambiguous) {
        recordTheGamesDbCheck(metadataPath, request.title, request.identifier);
        return false;
    }

    const std::string imageUrl = "https://api.thegamesdb.net/v1/Games/Images?apikey=" + key +
        "&games_id=" + gameId + "&filter%5Btype%5D=boxart%2Cscreenshot";
    const std::string imageJsonPath = gameDir + "/tgdb-images.json";
    httpStatus = 0;
    if (!downloader.fetchWithStatus(imageUrl, imageJsonPath, httpStatus, 25, &cancel) || httpStatus != 200) {
        unlink(imageJsonPath.c_str());
        return false;
    }
    const std::string imageJson = readText(imageJsonPath);
    unlink(imageJsonPath.c_str());
    if (cancel.load()) return false;
    if (jsonNumberAt(imageJson, "code") != "200" || jsonStringAt(imageJson, "status") != "Success")
        return false;
    const size_t dataPos = propertyPosition(imageJson, "data");
    std::string baseUrl = jsonStringAt(imageJson, "original", dataPos == std::string::npos ? 0 : dataPos);
    if (baseUrl.empty() || baseUrl.compare(0, 8, "https://") != 0) return false;
    const size_t hostEnd = baseUrl.find('/', 8);
    const std::string host = baseUrl.substr(8, hostEnd == std::string::npos ? std::string::npos : hostEnd - 8);
    if (host != "cdn.thegamesdb.net") return false;
    if (baseUrl.back() != '/') baseUrl.push_back('/');

    const size_t imagesPos = propertyPosition(imageJson, "images");
    const auto images = objectsFromArray(imageJson, gameId,
                                         imagesPos == std::string::npos ? 0 : imagesPos);
    const std::string boxPath = gameDir + "/boxart.jpg";
    std::vector<std::string> screenshotPaths;
    std::string boxFilename;
    std::vector<std::string> screenshotFiles;
    for (const std::string &image : images) {
        const std::string type = jsonStringAt(image, "type");
        const std::string filename = jsonStringAt(image, "filename");
        if (filename.empty() || filename.find("..") != std::string::npos || filename[0] == '/') continue;
        if (type == "boxart" && boxFilename.empty()) {
            const std::string side = jsonStringAt(image, "side");
            if (side.empty() || side == "front") boxFilename = filename;
        } else if (type == "screenshot" && screenshotFiles.size() < 4) {
            screenshotFiles.push_back(filename);
        }
    }
    if (boxFilename.empty() && screenshotFiles.empty()) {
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
        if (!downloader.fetchWithStatus(url, temp, imageStatus, 45, &cancel) ||
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
    for (size_t i = 0; i < screenshotFiles.size(); ++i) {
        if (cancel.load()) return false;
        const std::string target = gameDir + "/screenshot-" + std::to_string(i + 1) + ".jpg";
        if (downloadImage(screenshotFiles[i], target)) screenshotPaths.push_back(target);
    }
    out.screenshots = screenshotPaths;
    if (gotBoxart || !screenshotPaths.empty()) {
        std::string json = "{\"thegamesdb_checked\":true,\"thegamesdb_checked_at\":" +
            std::to_string(static_cast<long long>(time(nullptr))) +
            ",\"provider\":\"TheGamesDB\",\"game_id\":" + jsonQuoted(gameId) +
            ",\"title\":" + jsonQuoted(request.title) +
            ",\"disc_id\":" + jsonQuoted(request.identifier) + ",\"image_types\":[";
        bool first = true;
        if (gotBoxart) { json += "\"boxart\""; first = false; }
        if (!screenshotPaths.empty()) { if (!first) json += ","; json += "\"screenshot\""; }
        json += "]}\n";
        writeTextAtomically(metadataPath, json);
        out.state = State::Ready;
        return true;
    }
    return false;
}

DiscArtworkService::Snapshot DiscArtworkService::fetch(const Request &request,
                                                       const std::atomic<bool> &cancel) {
    const std::string platformKey = request.system == DiscDriveService::System::PlayStation ? "PSX" : "Saturn";
    DiscDriveService::Snapshot disc;
    disc.system = request.system;
    disc.identifier = request.identifier;
    disc.title = request.title;
    const std::string key = safeKey(disc);
    const std::string cacheRoot = std::string(MISTER_PAT_ROOT) + "/disc-cache";
    const std::string gameDir = cacheRoot + "/" + key;
    ensureDirectory(cacheRoot);
    ensureDirectory(gameDir);
    Snapshot out;
    out.cacheKey = key;
    out.boxart = gameDir + "/boxart.jpg";
    const std::string metadataPath = gameDir + "/metadata.json";
    struct utimbuf accessed {};
    accessed.actime = time(nullptr);
    accessed.modtime = accessed.actime;
    utime(gameDir.c_str(), &accessed);
    const std::string metadata = readText(metadataPath);
    const std::string screenshot = gameDir + "/screenshot-1.jpg";
    for (int i = 1; i <= 4; ++i) {
        const std::string shot = gameDir + "/screenshot-" + std::to_string(i) + ".jpg";
        if (fileExists(shot)) out.screenshots.push_back(shot);
    }
    if (fileExists(out.boxart) && !out.screenshots.empty()) out.state = State::Ready;

    Downloader downloader;
    const std::string indexCache = cacheRoot + "/index";
    ensureDirectory(indexCache);
    const bool tgdbChecked = theGamesDbCheckedRecently(metadata);
    if (!cancel.load() && client_.configured() && !tgdbChecked) {
        if (fetchFromTheGamesDB(request, downloader, out, gameDir, cancel) &&
            !out.screenshots.empty() && fileExists(out.boxart)) {
            pruneDiscCache(cacheRoot, gameDir);
            return out;
        }
    }
    if (cancel.load()) return out;
    if (fileExists(out.boxart) && !out.screenshots.empty()) {
        out.state = State::Ready;
        pruneDiscCache(cacheRoot, gameDir);
        return out;
    }
    LibretroIndex index;
    if (!index.open(LibretroIndex::platformFor(platformKey), downloader, indexCache)) {
        out.state = out.screenshots.empty() && !fileExists(out.boxart) ? State::Missing : State::Ready;
        return out;
    }
    if (cancel.load()) return out;
    std::string imageName = index.match(request.title);
    if (imageName.empty()) imageName = index.matchExact(request.title);
    if (!imageName.empty() && !fileExists(out.boxart)) {
        const std::string url = std::string(LibretroIndex::kHost) + "/" +
            Downloader::encodeComponent(LibretroIndex::platformFor(platformKey)) +
            "/Named_Boxarts/" + Downloader::encodeComponent(imageName);
        const std::string temp = gameDir + "/boxart.download";
        if (!cancel.load() && downloader.fetch(url, temp, 45)) {
            if (cancel.load()) { unlink(temp.c_str()); return out; }
            struct stat info {};
            if (stat(temp.c_str(), &info) == 0 && info.st_size > 0 && info.st_size <= 12 * 1024 * 1024)
                saveJpeg(Image::load(temp), out.boxart);
            unlink(temp.c_str());
        }
    }
    if (!cancel.load() && !imageName.empty() && out.screenshots.empty() && !fileExists(screenshot)) {
        LibretroIndex snaps;
        std::string snapName = imageName;
        if (snaps.open(LibretroIndex::platformFor(platformKey), downloader, indexCache, "Named_Snaps") &&
            !cancel.load()) {
            std::string match = snaps.match(request.title);
            if (match.empty()) match = snaps.matchExact(request.title);
            if (!match.empty()) snapName = match;
        }
        const std::string url = std::string(LibretroIndex::kHost) + "/" +
            Downloader::encodeComponent(LibretroIndex::platformFor(platformKey)) +
            "/Named_Snaps/" + Downloader::encodeComponent(snapName);
        const std::string temp = gameDir + "/screenshot.download";
        if (!cancel.load() && downloader.fetch(url, temp, 45)) {
            if (cancel.load()) { unlink(temp.c_str()); return out; }
            struct stat info {};
            if (stat(temp.c_str(), &info) == 0 && info.st_size > 0 && info.st_size <= 12 * 1024 * 1024)
                saveJpeg(Image::load(temp), screenshot);
            unlink(temp.c_str());
        }
    }
    if (fileExists(screenshot)) out.screenshots.push_back(screenshot);
    if (fileExists(out.boxart) || !out.screenshots.empty()) out.state = State::Ready;
    else out.state = State::Missing;
    pruneDiscCache(cacheRoot, gameDir);
    return out;
}
