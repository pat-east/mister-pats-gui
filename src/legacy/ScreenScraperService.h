#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ScreenScraperCache.h"
#include "ScreenScraperSettings.h"

// Finds and fetches the preview video of one game from ScreenScraper.fr, on one worker thread.
// Nothing here runs on the render or input path.
//
// What it does for a request: look the game up by title and system (never by ROM hash, never by
// title alone), remember the answer in the cache, download the video, check it, and report each
// step so the screen can say what is going on. The rules for the shared service are built in:
//
//   - one request at a time, at least 1.3 s apart;
//   - no automatic retries. A rate limit (429) pauses requests for a minute, an exhausted daily
//     quota (430, 431) for the rest of the session, a closed or overloaded API (401, 423) for ten
//     minutes, a rejected login (403) or a blocked version (426) for the session;
//   - failures are reported but never stored as results, so a game is tried again later;
//   - credentials go to curl on its standard input (never into its arguments or any log), and
//     nothing is sent without a certificate bundle to check the server against.
class ScreenScraperService {
public:
    enum class AccountState {
        Unknown,           // not checked yet
        Checking,
        Ok,
        NoDeveloperLogin,  // the settings file has no developer login
        NoUserLogin,       // no user name or password entered
        Rejected,          // ScreenScraper refused the user login
        DeveloperRejected, // ScreenScraper refused the developer login
        Unavailable,       // network or server problem; try again later
        Blocked,           // limits or a blocked version; not before restart
    };
    struct Account {
        AccountState state = AccountState::Unknown;
        std::string detail;   // plain words for the settings page
        int level = 0;
        int maxThreads = 0;
        long long requestsToday = 0;
        long long maxRequestsPerDay = 0;
        std::chrono::steady_clock::time_point checkedAt{};   // when the last check finished
    };

    enum class Phase {
        Idle,
        Scraping,     // asking ScreenScraper about the game
        Downloading,
        Verifying,
        Ready,        // the video file is on the card
        NotAvailable, // no such game or no video; the answer is stored
        Failed,       // could not finish; nothing stored; `message` says why
    };
    struct Snapshot {
        unsigned id = 0;            // changes with every request, so a screen can tell its own
        Phase phase = Phase::Idle;
        std::string videoPath;      // Ready
        std::string title;          // the game's name as ScreenScraper knows it
        bool fromStore = false;     // NotAvailable: the answer was already stored
        std::string message;        // Failed
    };

    struct Request {
        std::string systemKey;      // the GUI's key for the system, for the stored results
        int systemId = 0;           // ScreenScraper's system id
        std::string searchTitle;    // what to search for
        std::string titleKey;       // the comparison key of the title
        std::string region;         // "eu", "us" or "jp": preferred spelling of the title
        long long storageLimitBytes = 0;   // 0 = no limit
    };

    explicit ScreenScraperService(ScreenScraperCache &cache);
    ~ScreenScraperService();
    ScreenScraperService(const ScreenScraperService &) = delete;
    ScreenScraperService &operator=(const ScreenScraperService &) = delete;

    // Account. checkAccount() runs in the background; account() reads the latest state.
    void checkAccount();
    void forgetAccount();   // after the user changed the login: check again next time
    Account account() const;

    // A new request replaces and cancels the one in progress.
    void request(const Request &request);
    void cancel();
    Snapshot snapshot() const;

    // Stored-result answers need no network, so a screen can ask at once.
    ScreenScraperCache &cache() { return cache_; }

private:
    using Params = std::vector<std::pair<std::string, std::string>>;

    void workerLoop();
    void runAccountCheck();
    void runRequest(const Request &request, unsigned id);
    void finish(unsigned id, Snapshot snapshot);
    void setPhase(unsigned id, Phase phase);
    bool gate(unsigned id, std::string &message);          // may a request go out now?
    void noteStatus(int status);                            // learn from an HTTP status
    void pause(std::chrono::milliseconds duration);         // cancel-aware wait
    bool cancelled(unsigned id) const;
    // One GET; returns the HTTP status, or 0 when the request could not be made or was cancelled.
    int get(const std::string &endpoint, const Params &params, const std::string &outPath,
            long long maxBytes, int timeoutSeconds, unsigned id);
    Params withLogin(const Params &params) const;

    ScreenScraperCache &cache_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool started_ = false;
    bool stopping_ = false;

    // Pending work (latest request wins).
    bool hasRequest_ = false;
    Request pending_;
    unsigned requestId_ = 0;
    bool accountPending_ = false;

    ScreenScraperCredentials credentials_;   // read afresh at the start of each job
    Snapshot snapshot_;
    Account account_;
    std::atomic<unsigned> currentId_{0};
    std::atomic<bool> stop_{false};

    // Request rules; only the worker touches these.
    std::chrono::steady_clock::time_point lastRequest_{};
    std::chrono::steady_clock::time_point blockedUntil_{};
    std::atomic<bool> blockedForSession_{false};
    std::atomic<bool> loginBlocked_{false};   // the session block is a rejected login
    std::string blockReason_;                 // worker only
};
