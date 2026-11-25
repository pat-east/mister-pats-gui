#include "ScreenScraperService.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <poll.h>
#include <spawn.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#include "DebugLog.h"
#include "Downloader.h"
#include "MiniJson.h"
#include "Paths.h"
#include "ScreenScraperNames.h"

extern char **environ;

namespace {

using Clock = std::chrono::steady_clock;

constexpr const char *kApiBase = "https://api.screenscraper.fr/api2/";
constexpr auto kMinSpacing = std::chrono::milliseconds(1300);
constexpr long long kMaxAnswerBytes = 12ll * 1024 * 1024;
constexpr long long kMaxVideoBytes = 64ll * 1024 * 1024;
constexpr long long kFreeSpaceMargin = 300ll * 1024 * 1024;   // always leave this much on the card

std::string escapeForCurlConfig(const std::string &value) {
    std::string out;
    for (char c : value) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

bool readFile(const std::string &path, std::string &out, size_t limit) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return out.size() <= limit;
}

// "Vérifier les identifiants utilisateurs" versus "... développeur": ScreenScraper uses one status
// (403) for both, and the text tells them apart.
enum class LoginProblem { Unknown, User, Developer };
LoginProblem loginProblem(const std::string &body) {
    std::string lower;
    for (char c : body) lower += char(std::tolower(static_cast<unsigned char>(c)));
    if (lower.find("utilisateur") != std::string::npos) return LoginProblem::User;
    if (lower.find("veloppeur") != std::string::npos) return LoginProblem::Developer;
    return LoginProblem::Unknown;
}

// A region's order of preference for the name shown: the chosen one, world, then the others.
int regionRank(const std::string &region, const std::string &preferred) {
    if (region == preferred) return 0;
    if (region == "wor") return 1;
    if (region == "us" || region == "eu" || region == "jp") return 2;
    return 3;
}

std::string pickName(const JsonValue &game, const std::string &preferred) {
    std::string best;
    int bestRank = 99;
    for (const JsonValue &entry : game["noms"].items) {
        const std::string text = entry["text"].asString();
        if (text.empty()) continue;
        const int rank = regionRank(entry["region"].asString(), preferred);
        if (rank < bestRank) { best = text; bestRank = rank; }
    }
    if (best.empty()) best = game["nom"].asString();
    return best;
}

bool gameHasName(const JsonValue &game, const std::string &titleKey) {
    for (const JsonValue &entry : game["noms"].items)
        if (ScreenScraperNames::titleKey(entry["text"].asString()) == titleKey) return true;
    return !game["nom"].asString().empty() &&
           ScreenScraperNames::titleKey(game["nom"].asString()) == titleKey;
}

unsigned long fileCrc32(const std::string &path, long long &size) {
    size = 0;
    std::ifstream in(path, std::ios::binary);
    if (!in) return 0;
    unsigned long crc = crc32(0L, Z_NULL, 0);
    char buffer[16384];
    while (in) {
        in.read(buffer, sizeof(buffer));
        const std::streamsize n = in.gcount();
        if (n <= 0) break;
        crc = crc32(crc, reinterpret_cast<const Bytef *>(buffer), static_cast<uInt>(n));
        size += n;
    }
    return crc;
}

}  // namespace

ScreenScraperService::ScreenScraperService(ScreenScraperCache &cache) : cache_(cache) {}

ScreenScraperService::~ScreenScraperService() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        stop_ = true;
        wake_.notify_all();
    }
    if (worker_.joinable()) worker_.join();
}

void ScreenScraperService::checkAccount() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (account_.state == AccountState::Checking) return;
    account_.state = AccountState::Checking;
    account_.detail = "Checking...";
    accountPending_ = true;
    if (!started_) {
        started_ = true;
        worker_ = std::thread([this] { workerLoop(); });
    }
    wake_.notify_all();
}

void ScreenScraperService::forgetAccount() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (account_.state == AccountState::Checking) return;
    account_ = Account();
    // A rejected login may be fixed by a new one; limits and a blocked version are not.
    if (loginBlocked_.load()) {
        loginBlocked_ = false;
        blockedForSession_ = false;
    }
}

ScreenScraperService::Account ScreenScraperService::account() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return account_;
}

void ScreenScraperService::request(const Request &request) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_ = request;
    hasRequest_ = true;
    ++requestId_;
    currentId_ = requestId_;
    snapshot_ = Snapshot();
    snapshot_.id = requestId_;
    snapshot_.phase = Phase::Scraping;
    if (!started_) {
        started_ = true;
        worker_ = std::thread([this] { workerLoop(); });
    }
    wake_.notify_all();
}

void ScreenScraperService::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    hasRequest_ = false;
    currentId_ = ++requestId_;   // the job in progress is now stale and stops at its next check
    snapshot_ = Snapshot();
    snapshot_.id = requestId_;
}

ScreenScraperService::Snapshot ScreenScraperService::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

// Id 0 is the account check: it is only stopped by shutdown, not by a newer game request.
bool ScreenScraperService::cancelled(unsigned id) const {
    return stop_.load() || (id != 0 && currentId_.load() != id);
}

void ScreenScraperService::setPhase(unsigned id, Phase phase) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.id == id) snapshot_.phase = phase;
}

void ScreenScraperService::finish(unsigned id, Snapshot result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.id != id) return;   // a newer request owns the snapshot
    result.id = id;
    snapshot_ = std::move(result);
}

void ScreenScraperService::workerLoop() {
    for (;;) {
        bool doAccount = false;
        bool doRequest = false;
        Request request;
        unsigned id = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || accountPending_ || hasRequest_; });
            if (stopping_) return;
            if (accountPending_) {
                accountPending_ = false;
                doAccount = true;
            } else if (hasRequest_) {
                hasRequest_ = false;
                request = pending_;
                id = requestId_;
                doRequest = true;
            }
        }
        if (doAccount) runAccountCheck();
        else if (doRequest) runRequest(request, id);
    }
}

void ScreenScraperService::pause(std::chrono::milliseconds duration) {
    const auto end = Clock::now() + duration;
    while (Clock::now() < end && !stop_.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

ScreenScraperService::Params ScreenScraperService::withLogin(const Params &params) const {
    Params all = params;
    all.emplace_back("devid", credentials_.devId);
    all.emplace_back("devpassword", credentials_.devPassword);
    all.emplace_back("softname", credentials_.softName);
    if (credentials_.hasUser()) {
        all.emplace_back("ssid", credentials_.userId);
        all.emplace_back("sspassword", credentials_.userPassword);
    }
    all.emplace_back("output", "json");
    return all;
}

// Learns from an HTTP status which pause, if any, applies from now on.
void ScreenScraperService::noteStatus(int status) {
    const auto now = Clock::now();
    switch (status) {
    case 429:
        blockedUntil_ = now + std::chrono::seconds(60);
        blockReason_ = "ScreenScraper asked us to slow down";
        break;
    case 430:
    case 431:
        blockedForSession_ = true;
        blockReason_ = "The daily limit of ScreenScraper is used up";
        break;
    case 401:
    case 423:
        blockedUntil_ = now + std::chrono::minutes(10);
        blockReason_ = "ScreenScraper is not available right now";
        break;
    case 426:
        blockedForSession_ = true;
        blockReason_ = "ScreenScraper has blocked this version of the software";
        break;
    default:
        break;
    }
}

bool ScreenScraperService::gate(unsigned id, std::string &message) {
    if (blockedForSession_) { message = blockReason_.empty() ? "ScreenScraper is not available" : blockReason_; return false; }
    if (Clock::now() < blockedUntil_) { message = blockReason_; return false; }
    // At least 1.3 s since the last request, whatever it was.
    if (lastRequest_ != Clock::time_point{}) {
        const auto since = Clock::now() - lastRequest_;
        if (since < kMinSpacing)
            pause(std::chrono::duration_cast<std::chrono::milliseconds>(kMinSpacing - since));
    }
    if (cancelled(id)) { message.clear(); return false; }
    return true;
}

int ScreenScraperService::get(const std::string &endpoint, const Params &params,
                              const std::string &outPath, long long maxBytes, int timeoutSeconds,
                              unsigned id) {
    const std::string certificate = Downloader::certificateFile();
    if (certificate.empty()) {
        DebugLog::error("ScreenScraper: no certificate bundle on this system; nothing was sent");
        return 0;
    }

    // The login and every other value go to curl on standard input, never into its arguments.
    std::string config = "get\nurl = \"" + std::string(kApiBase) + endpoint + "\"\n";
    for (const auto &param : params)
        config += "data-urlencode = \"" + param.first + "=" + escapeForCurlConfig(param.second) + "\"\n";

    int toChild[2], fromChild[2];
    if (pipe(toChild) != 0) return 0;
    if (pipe(fromChild) != 0) { close(toChild[0]); close(toChild[1]); return 0; }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, toChild[0], 0);
    posix_spawn_file_actions_adddup2(&actions, fromChild[1], 1);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, toChild[0]);
    posix_spawn_file_actions_addclose(&actions, toChild[1]);
    posix_spawn_file_actions_addclose(&actions, fromChild[0]);
    posix_spawn_file_actions_addclose(&actions, fromChild[1]);

    const std::string timeout = std::to_string(timeoutSeconds);
    const std::string limit = std::to_string(maxBytes);
    std::vector<std::string> args = {"curl", "-sS", "-4", "--proto", "=https", "--cacert", certificate,
                                     "--connect-timeout", "12", "-m", timeout, "--max-filesize", limit,
                                     "-o", outPath, "-w", "%{http_code}", "-K", "-"};
    std::vector<char *> argv;
    for (std::string &a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);

    pid_t child = 0;
    const int spawned = posix_spawnp(&child, "curl", &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(toChild[0]);
    close(fromChild[1]);
    if (spawned != 0) {
        close(toChild[1]);
        close(fromChild[0]);
        DebugLog::error("ScreenScraper: could not start curl");
        return 0;
    }

    // Hand over the request and close, so curl sees the end of its configuration.
    signal(SIGPIPE, SIG_IGN);
    size_t written = 0;
    while (written < config.size()) {
        const ssize_t n = write(toChild[1], config.data() + written, config.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        written += size_t(n);
    }
    close(toChild[1]);
    std::fill(config.begin(), config.end(), '\0');

    std::string status;
    bool killed = false;
    for (;;) {
        struct pollfd pfd = {fromChild[0], POLLIN, 0};
        const int ready = poll(&pfd, 1, 100);
        if (cancelled(id) && !killed) { kill(child, SIGTERM); killed = true; }
        if (ready < 0 && errno != EINTR) break;
        if (ready > 0) {
            char buffer[64];
            const ssize_t n = read(fromChild[0], buffer, sizeof(buffer));
            if (n <= 0) break;
            status.append(buffer, size_t(n));
            if (status.size() > 16) break;
        }
    }
    close(fromChild[0]);
    int wait = 0;
    waitpid(child, &wait, 0);
    if (killed) return 0;
    return std::atoi(status.c_str());
}

void ScreenScraperService::runAccountCheck() {
    credentials_ = ScreenScraperSettings::load();
    Account result;
    if (!credentials_.hasDeveloper()) {
        result.state = AccountState::NoDeveloperLogin;
        result.detail = "Developer login missing in the settings file";
    } else if (!credentials_.hasUser()) {
        result.state = AccountState::NoUserLogin;
        result.detail = "Enter your ScreenScraper user name and password";
    } else {
        std::string message;
        const unsigned id = 0;
        if (!gate(id, message)) {
            result.state = AccountState::Unavailable;
            result.detail = message.empty() ? "Stopped" : message;
        } else {
            const std::string out = cache_.directory() + "/account.tmp";
            mkdir(cache_.directory().c_str(), 0755);
            const int status = get("ssuserInfos.php", withLogin({}), out, kMaxAnswerBytes, 40, id);
            lastRequest_ = Clock::now();
            std::string body;
            readFile(out, body, size_t(kMaxAnswerBytes));
            unlink(out.c_str());
            noteStatus(status);
            if (status == 200) {
                JsonValue json;
                std::string error;
                if (parseJson(body, json, error) && json["response"]["ssuser"].isObject()) {
                    const JsonValue &user = json["response"]["ssuser"];
                    result.state = AccountState::Ok;
                    result.level = int(user["niveau"].asInteger());
                    result.maxThreads = int(user["maxthreads"].asInteger());
                    result.requestsToday = user["requeststoday"].asInteger();
                    result.maxRequestsPerDay = user["maxrequestsperday"].asInteger();
                    result.detail = "Logged in (level " + std::to_string(result.level) + ")";
                } else {
                    result.state = AccountState::Unavailable;
                    result.detail = "ScreenScraper gave an answer this software cannot read";
                }
            } else if (status == 403) {
                const LoginProblem problem = loginProblem(body);
                if (problem == LoginProblem::Developer) {
                    result.state = AccountState::DeveloperRejected;
                    result.detail = "ScreenScraper did not accept the developer login";
                } else {
                    result.state = AccountState::Rejected;
                    result.detail = "ScreenScraper did not accept your user name or password";
                }
                blockedForSession_ = true;
                loginBlocked_ = true;
                blockReason_ = result.detail;
            } else if (status == 0) {
                result.state = AccountState::Unavailable;
                result.detail = "Could not reach ScreenScraper";
            } else if (blockedForSession_) {
                result.state = AccountState::Blocked;
                result.detail = blockReason_;
            } else {
                result.state = AccountState::Unavailable;
                result.detail = blockReason_.empty() ? "ScreenScraper is not available right now" : blockReason_;
            }
        }
    }
    result.checkedAt = Clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    account_ = result;
}

void ScreenScraperService::runRequest(const Request &request, unsigned id) {
    credentials_ = ScreenScraperSettings::load();
    Snapshot done;
    const std::string &key = request.titleKey;

    if (!credentials_.hasDeveloper() || !credentials_.hasUser() || request.systemId <= 0 || key.empty()) {
        done.phase = Phase::Failed;
        done.message = "ScreenScraper is not set up";
        finish(id, done);
        return;
    }

    // 1. What do we already know about this game?
    ScreenScraperResult result;
    bool known = cache_.find(request.systemKey, key, result);
    if (known && result.status != ScreenScraperResult::Status::Video) {
        done.phase = Phase::NotAvailable;
        done.fromStore = true;
        done.title = result.title;
        finish(id, done);
        return;
    }

    // 2. Ask ScreenScraper. Only the answer "no such game / no video / here is a video" is stored.
    if (!known) {
        std::string message;
        if (!gate(id, message)) {
            done.phase = Phase::Failed;
            done.message = message;
            if (!cancelled(id)) finish(id, done);
            return;
        }
        const std::string answerPath = cache_.directory() + "/answer.tmp";
        mkdir(cache_.directory().c_str(), 0755);
        const int status = get("jeuRecherche.php",
                               withLogin({{"recherche", request.searchTitle},
                                          {"systemeid", std::to_string(request.systemId)}}),
                               answerPath, kMaxAnswerBytes, 60, id);
        lastRequest_ = Clock::now();
        if (cancelled(id)) { unlink(answerPath.c_str()); return; }
        std::string body;
        readFile(answerPath, body, size_t(kMaxAnswerBytes));
        unlink(answerPath.c_str());
        noteStatus(status);

        if (status == 403) {
            done.phase = Phase::Failed;
            done.message = loginProblem(body) == LoginProblem::Developer
                               ? "ScreenScraper did not accept the developer login"
                               : "ScreenScraper did not accept your login";
            blockedForSession_ = true;
            loginBlocked_ = true;
            blockReason_ = done.message;
            finish(id, done);
            return;
        }
        if (status == 404) {
            // "Not found": ScreenScraper has no game by this title for this system.
            result = ScreenScraperResult();
            result.status = ScreenScraperResult::Status::NoGame;
            result.fetched = static_cast<long long>(std::time(nullptr));
            cache_.store(request.systemKey, key, result);
            done.phase = Phase::NotAvailable;
            finish(id, done);
            return;
        }
        if (status != 200) {
            done.phase = Phase::Failed;
            done.message = status == 0 ? "Could not reach ScreenScraper"
                                       : (blockReason_.empty() ? "ScreenScraper is not available right now"
                                                               : blockReason_);
            finish(id, done);
            return;
        }

        JsonValue json;
        std::string error;
        if (!parseJson(body, json, error) || !json["response"].isObject()) {
            done.phase = Phase::Failed;
            done.message = "ScreenScraper gave an answer this software cannot read";
            DebugLog::error("ScreenScraper: unreadable search answer: " + error);
            finish(id, done);
            return;
        }

        // The first game with exactly this title (the answer is ordered by likelihood).
        const JsonValue *match = nullptr;
        for (const JsonValue &game : json["response"]["jeux"].items) {
            if (!game.isObject() || game["id"].asInteger() <= 0) continue;
            if (gameHasName(game, key)) { match = &game; break; }
        }
        result = ScreenScraperResult();
        result.fetched = static_cast<long long>(std::time(nullptr));
        if (!match) {
            result.status = ScreenScraperResult::Status::NoGame;
        } else {
            result.gameId = (*match)["id"].asInteger();
            result.title = pickName(*match, request.region);
            // The smaller "normalized" video is preferred; the plain one is the fallback.
            const JsonValue *chosen = nullptr;
            std::string chosenType;
            for (const char *type : {"video-normalized", "video"}) {
                for (const JsonValue &media : (*match)["medias"].items) {
                    if (media["type"].asString() == type && media["format"].asString() == "mp4" &&
                        media["size"].asInteger() > 0 && media["size"].asInteger() <= kMaxVideoBytes) {
                        chosen = &media;
                        chosenType = type;
                        break;
                    }
                }
                if (chosen) break;
            }
            if (!chosen) {
                result.status = ScreenScraperResult::Status::NoVideo;
            } else {
                result.status = ScreenScraperResult::Status::Video;
                result.videoType = chosenType;
                result.size = (*chosen)["size"].asInteger();
                result.crc = std::strtoul((*chosen)["crc"].asString().c_str(), nullptr, 16);
            }
        }
        cache_.store(request.systemKey, key, result);
        if (result.status != ScreenScraperResult::Status::Video) {
            done.phase = Phase::NotAvailable;
            done.title = result.title;
            finish(id, done);
            return;
        }
    }

    // 3. The video: already on the card, or download and check it.
    done.title = result.title;
    if (cache_.hasVideo(result.gameId, result.videoType)) {
        done.phase = Phase::Ready;
        done.videoPath = cache_.videoPath(result.gameId, result.videoType);
        finish(id, done);
        return;
    }

    const long long free = cache_.freeBytes();
    if ((free >= 0 && free < result.size + kFreeSpaceMargin) ||
        !cache_.makeRoom(result.size, request.storageLimitBytes)) {
        done.phase = Phase::Failed;
        done.message = "Not enough space for the video";
        finish(id, done);
        return;
    }

    setPhase(id, Phase::Downloading);
    std::string message;
    if (!gate(id, message)) {
        done.phase = Phase::Failed;
        done.message = message;
        if (!cancelled(id)) finish(id, done);
        return;
    }
    const std::string part = cache_.partialPath(result.gameId, result.videoType);
    const size_t slash = part.find_last_of('/');
    mkdir(cache_.directory().c_str(), 0755);
    mkdir(part.substr(0, slash).c_str(), 0755);
    const int status = get("mediaVideoJeu.php",
                           withLogin({{"systemeid", std::to_string(request.systemId)},
                                      {"jeuid", std::to_string(result.gameId)},
                                      {"media", result.videoType}}),
                           part, result.size + 1024 * 1024, 300, id);
    lastRequest_ = Clock::now();
    if (cancelled(id)) { unlink(part.c_str()); return; }
    noteStatus(status);
    if (status != 200) {
        unlink(part.c_str());
        done.phase = Phase::Failed;
        done.message = status == 0 ? "The download did not finish"
                                   : (blockReason_.empty() ? "ScreenScraper did not send the video" : blockReason_);
        finish(id, done);
        return;
    }

    // 4. Check what arrived: the size and CRC ScreenScraper reported, and an MP4 header.
    setPhase(id, Phase::Verifying);
    long long size = 0;
    const unsigned long crc = fileCrc32(part, size);
    bool headerOk = false;
    {
        std::ifstream in(part, std::ios::binary);
        char head[12] = {0};
        in.read(head, sizeof(head));
        headerOk = in.gcount() >= 8 && std::memcmp(head + 4, "ftyp", 4) == 0;
    }
    if (cancelled(id)) { unlink(part.c_str()); return; }
    const bool crcKnown = result.crc != 0;
    if (!headerOk || size != result.size || (crcKnown && crc != result.crc)) {
        unlink(part.c_str());
        DebugLog::error("ScreenScraper: a downloaded video did not match what was announced");
        done.phase = Phase::Failed;
        done.message = "The downloaded video did not verify";
        finish(id, done);
        return;
    }
    const std::string path = cache_.videoPath(result.gameId, result.videoType);
    if (std::rename(part.c_str(), path.c_str()) != 0) {
        unlink(part.c_str());
        done.phase = Phase::Failed;
        done.message = "Could not save the video";
        finish(id, done);
        return;
    }
    done.phase = Phase::Ready;
    done.videoPath = path;
    finish(id, done);
}
