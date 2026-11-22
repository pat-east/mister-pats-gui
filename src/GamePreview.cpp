#include "GamePreview.h"

#include <chrono>
#include <cmath>

#include "DebugLog.h"
#include "ScreenScraperNames.h"

namespace {
constexpr int kPlays = 3;            // a preview plays this many times, then the box art returns
}

GamePreview::GamePreview(ScreenScraperService &service, VideoPlayer &player,
                         Preferences &preferences)
    : service_(service), player_(player), preferences_(preferences) {}

void GamePreview::setText(const std::string &text) {
    if (text == text_) return;
    text_ = text;
    ++revision_;
}

void GamePreview::setMode(Mode mode, const std::string &text) {
    if (mode != mode_) {
        if (mode == Mode::Playing || mode_ == Mode::Playing) ++revision_;   // the picture changes
        mode_ = mode;
    }
    setText(text);
}

GamePreview::Gate GamePreview::gate(const GameSystem *system, const Game *game, std::string &note) {
    if (!system || !game || !preferences_.screenScraperEnabled()) return Gate::No;
    if (ScreenScraperNames::systemId(system->dbKey) <= 0) {
        // Arcade, computers and anything else not in the table: say so instead of staying silent.
        note = "No video preview for this system";
        return Gate::Note;
    }
    using State = ScreenScraperService::AccountState;
    const ScreenScraperService::Account account = service_.account();
    switch (account.state) {
    case State::Ok:
        return Gate::Yes;
    case State::Unknown:
        service_.checkAccount();   // the first check; its state flips to Checking at once
        note = "Checking login ...";
        return Gate::Note;
    case State::Checking:
        note = "Checking login ...";
        return Gate::Note;
    case State::Unavailable:
        // No network yet, or ScreenScraper was busy: try again, but at most every 30 seconds and
        // only while the user is looking at a game this feature applies to.
        if (std::chrono::steady_clock::now() - account.checkedAt > std::chrono::seconds(30))
            service_.checkAccount();
        note = "No connection, trying again soon";
        return Gate::Note;
    case State::NoUserLogin:
        note = "Enter your login in Settings";
        return Gate::Note;
    case State::NoDeveloperLogin:
        note = "Developer login missing";
        return Gate::Note;
    case State::Rejected:
        note = "Login not accepted, see Settings";
        return Gate::Note;
    case State::DeveloperRejected:
        note = "Developer login not accepted";
        return Gate::Note;
    case State::Blocked:
        note = account.detail.empty() ? std::string("Not available") : account.detail;
        return Gate::Note;
    }
    return Gate::No;
}

void GamePreview::deactivate() {
    if (mode_ == Mode::Off && key_.empty()) return;
    player_.stop();
    service_.cancel();
    key_.clear();
    mode_ = Mode::Off;
    if (!text_.empty()) { text_.clear(); ++revision_; }
    else ++revision_;   // a video that was showing is gone
}

void GamePreview::stop() { deactivate(); }

void GamePreview::focus(const GameSystem *system, const Game *game) {
    focusedThisFrame_ = true;
    std::string note;
    const Gate gateResult = gate(system, game, note);
    if (gateResult == Gate::No) {
        deactivate();
        return;
    }
    if (gateResult == Gate::Note) {
        if (mode_ != Mode::Note) {
            player_.stop();
            service_.cancel();
            key_.clear();
            mode_ = Mode::Note;
            ++revision_;
        }
        setText(note);
        return;
    }
    if (mode_ == Mode::Note) {   // the account is usable now: start from the beginning
        mode_ = Mode::Off;
        key_.clear();
    }
    // Keyed by title: another ROM version of the same game, or the Game options panel opened for
    // the game the list has selected, keeps what is already running instead of restarting it.
    const std::string key = system->dbKey + "|" + ScreenScraperNames::titleKey(game->name);
    if (key == key_) return;   // the same game: nothing to restart
    begin(system, game);
}

void GamePreview::begin(const GameSystem *system, const Game *game) {
    player_.stop();
    service_.cancel();
    key_ = system->dbKey + "|" + ScreenScraperNames::titleKey(game->name);
    systemKey_ = system->dbKey;
    systemId_ = ScreenScraperNames::systemId(system->dbKey);
    searchTitle_ = ScreenScraperNames::searchTitle(game->name);
    titleKey_ = ScreenScraperNames::titleKey(game->name);
    title_ = game->name;
    requestId_ = 0;
    if (titleKey_.empty()) { mode_ = Mode::Off; key_.clear(); setText(""); return; }

    // A stored "no video" answer is shown at once: nothing will play, so no countdown.
    ScreenScraperResult stored;
    if (service_.cache().find(systemKey_, titleKey_, stored) &&
        stored.status != ScreenScraperResult::Status::Video) {
        setMode(Mode::Known, "No video found");
        return;
    }
    remaining_ = float(preferences_.screenScraperDelay());
    setMode(Mode::Countdown, "Video plays in: " + std::to_string(preferences_.screenScraperDelay()) + " s");
}

void GamePreview::startNow() {
    ScreenScraperResult stored;
    const bool known = service_.cache().find(systemKey_, titleKey_, stored);
    if (known && stored.status == ScreenScraperResult::Status::Video &&
        service_.cache().hasVideo(stored.gameId, stored.videoType)) {
        // The video is already on the card.
        title_ = stored.title.empty() ? title_ : stored.title;
        player_.start(service_.cache().videoPath(stored.gameId, stored.videoType),
                      slotWidth_, slotHeight_, kPlays, preferences_.screenScraperVolume() > 0, preferences_.screenScraperVolume());
        setMode(Mode::Opening, "Video found: verifying...");
        return;
    }
    ScreenScraperService::Request request;
    request.systemKey = systemKey_;
    request.systemId = systemId_;
    request.searchTitle = searchTitle_;
    request.titleKey = titleKey_;
    request.region = preferences_.screenScraperRegion();
    request.storageLimitBytes = static_cast<long long>(preferences_.screenScraperStorageLimitMb()) * 1024 * 1024;
    service_.request(request);
    requestId_ = service_.snapshot().id;
    setMode(Mode::Working, "Scraping game info ...");
}

void GamePreview::update(float deltaSeconds, bool suspended) {
    if (!focusedThisFrame_ || suspended) deactivate();
    focusedThisFrame_ = false;

    switch (mode_) {
    case Mode::Countdown: {
        remaining_ -= deltaSeconds;
        if (remaining_ <= 0.0f) { startNow(); break; }
        setText("Video plays in: " + std::to_string(int(std::ceil(remaining_))) + " s");
        break;
    }
    case Mode::Working: {
        const ScreenScraperService::Snapshot snap = service_.snapshot();
        if (snap.id != requestId_) break;   // not ours (cannot normally happen)
        switch (snap.phase) {
        case ScreenScraperService::Phase::Scraping: setText("Scraping game info ..."); break;
        case ScreenScraperService::Phase::Downloading: setText("Downloading video ..."); break;
        case ScreenScraperService::Phase::Verifying: setText("Video found: verifying..."); break;
        case ScreenScraperService::Phase::Ready:
            if (!snap.title.empty()) title_ = snap.title;
            player_.start(snap.videoPath, slotWidth_, slotHeight_, kPlays,
                          preferences_.screenScraperVolume() > 0, preferences_.screenScraperVolume());
            setMode(Mode::Opening, "Video found: verifying...");
            break;
        case ScreenScraperService::Phase::NotAvailable:
            setMode(snap.fromStore ? Mode::Known : Mode::NotFound,
                    snap.fromStore ? "No video found" : "No video found: Not available");
            break;
        case ScreenScraperService::Phase::Failed:
            setMode(Mode::Failed, snap.message.empty() ? std::string("Not available") : snap.message);
            break;
        case ScreenScraperService::Phase::Idle: break;
        }
        break;
    }
    case Mode::Opening: {
        const VideoPlayer::State state = player_.state();
        if (state == VideoPlayer::State::Playing)
            setMode(Mode::Playing, "");   // the video is the message; no "Now playing" line
        else if (state == VideoPlayer::State::Failed) {
            DebugLog::warn("video preview: " + player_.error());
            setMode(Mode::CannotPlay, "Video found: " + player_.error());
        }
        else if (state == VideoPlayer::State::Finished)
            setMode(Mode::Done, "");
        break;
    }
    case Mode::Playing:
        if (player_.state() == VideoPlayer::State::Finished) {
            player_.stop();
            setMode(Mode::Done, "");
        } else if (player_.state() == VideoPlayer::State::Failed) {
            const std::string reason = player_.error();
            player_.stop();
            setMode(Mode::CannotPlay, "Video found: " + reason);
        }
        break;
    default:
        break;
    }
}
