#include "MediaScraper.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "Image.h"
#include "Library.h"

namespace {

// /tmp is a RAM disk on a MiSTer, so nothing here ever touches a drive.
const char *kTempImage = "/tmp/mister-pat-scrape.img";

bool fileExists(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

bool makeDirectory(const std::string &path) {
    if (mkdir(path.c_str(), 0777) == 0) return true;
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace

void MediaScraper::start(const GameDatabase &database, const Options &options,
                         const std::vector<std::string> &systemKeys) {
    options_ = options;
    systems_.clear();
    jobs_.clear();
    systemPosition_ = jobPosition_ = 0;
    totalGames_ = doneGames_ = fetched_ = skipped_ = missing_ = 0;
    missesOpened_ = false;
    currentSystem_.clear();
    currentTitle_.clear();
    error_.clear();

    for (const DatabaseSystem &system : database.systems()) {
        if (!systemKeys.empty()) {
            bool wanted = false;
            for (const std::string &key : systemKeys)
                if (key == system.key) wanted = true;
            if (!wanted) continue;
        }

        // A system with no thumbnail set on the server is left out here rather than failing
        // game by game, so the count the user sees is the work that will actually happen.
        if (LibretroIndex::platformFor(system.key).empty()) continue;

        systems_.push_back(system);
        totalGames_ += system.count;
    }

    if (systems_.empty()) {
        error_ = "none of these systems have artwork on the thumbnail server";
        state_ = State::Failed;
        return;
    }

    // The game lists are read one system at a time, only when that system's turn comes.
    database_ = &database;
    state_ = State::Preparing;
}

void MediaScraper::cancel() {
    if (running()) {
        error_ = "cancelled";
        state_ = State::Failed;
    }
}

float MediaScraper::progress() const {
    if (state_ == State::Done) return 1.0f;
    if (!totalGames_) return 0.0f;
    return float(doneGames_) / float(totalGames_);
}

std::string MediaScraper::statusLine() const {
    char buffer[200];

    switch (state_) {
    case State::Idle:
        return "not started";
    case State::Preparing:
        return "Fetching the index for " + currentSystem_ + " …";
    case State::Working:
        std::snprintf(buffer, sizeof(buffer), "%zu / %zu  ·  %s  ·  %s", doneGames_,
                      totalGames_, currentSystem_.c_str(), currentTitle_.c_str());
        return buffer;
    case State::Done:
        std::snprintf(buffer, sizeof(buffer), "%zu fetched, %zu already there, %zu not found",
                      fetched_, skipped_, missing_);
        return buffer;
    case State::Failed:
    default:
        return error_.empty() ? "failed" : error_;
    }
}

void MediaScraper::noteMiss(const std::string &system, const std::string &title) {
    ++missing_;

    std::ofstream out(options_.missesFile, missesOpened_ ? std::ios::app : std::ios::trunc);
    if (!out) return;

    if (!missesOpened_) {
        const std::time_t now = std::time(nullptr);
        char stamp[32] = "";
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", std::localtime(&now));
        out << "# artwork not found, run of " << stamp << "\n";
        missesOpened_ = true;
    }

    out << system << "\t" << title << "\n";
}

bool MediaScraper::fetchOne(const std::string &url, const std::string &destination,
                            int maxEdge) {
    if (!downloader_.fetch(url, kTempImage, 25)) return false;

    ImagePtr image = Image::load(kTempImage);
    unlink(kTempImage);
    if (!image) return false;

    // Shrink the long side. A cover is drawn at about 260 pixels; carrying four times that
    // around costs decoding time on every single frame it appears in.
    int width = image->width(), height = image->height();
    if (width > maxEdge || height > maxEdge) {
        if (width >= height) {
            height = int(long(height) * maxEdge / width);
            width = maxEdge;
        } else {
            width = int(long(width) * maxEdge / height);
            height = maxEdge;
        }
        if (ImagePtr scaled = image->scaledTo(width, height)) image = scaled;
    }

    // Write beside the target and move into place, so an interrupted write cannot leave a
    // half-finished file that later looks like "already scraped".
    const std::string temporary = destination + ".part";
    if (!image->saveJpeg(temporary, options_.quality)) return false;
    return rename(temporary.c_str(), destination.c_str()) == 0;
}

bool MediaScraper::prepareNextSystem() {
    jobs_.clear();
    jobPosition_ = 0;

    while (systemPosition_ < systems_.size()) {
        const DatabaseSystem &system = systems_[systemPosition_];
        currentSystem_ = system.name;

        if (!index_.open(LibretroIndex::platformFor(system.key), downloader_,
                         options_.indexCacheDir)) {
            // One unreachable index is not a reason to abandon the rest.
            std::printf("scraper: %s skipped, %s\n", system.name.c_str(),
                        index_.lastError().c_str());
            doneGames_ += system.count;
            ++systemPosition_;
            continue;
        }

        GameSystem shape;
        shape.discBased = system.discBased;
        shape.romDirs.push_back(system.dir);

        const std::string mediaDir = system.dir + "/media";
        for (const std::string &path : database_->pathsFor(system.key)) {
            const Game game = Library::makeGame(shape, path);
            jobs_.push_back({path, game.name, mediaDir});
        }

        ++systemPosition_;
        return true;
    }

    return false;
}

void MediaScraper::step() {
    switch (state_) {
    case State::Preparing:
        if (!prepareNextSystem()) {
            state_ = State::Done;
            return;
        }
        state_ = State::Working;
        return;

    case State::Working: {
        if (jobPosition_ >= jobs_.size()) {
            state_ = State::Preparing;
            return;
        }

        const Job &job = jobs_[jobPosition_++];
        ++doneGames_;
        currentTitle_ = job.name;

        struct Wanted {
            bool enabled;
            const char *suffix;
            const char *folder;
        };
        const Wanted wanted[] = {
            {options_.boxart, "", "Named_Boxarts"},
            {options_.backgrounds, "-BG", "Named_Snaps"},
        };

        bool matched = false;
        bool anyWanted = false;

        for (const Wanted &kind : wanted) {
            if (!kind.enabled) continue;
            anyWanted = true;

            const std::string base = job.mediaDir + "/" + job.name + kind.suffix;
            // Artwork that is already there — ours or anyone else's — is left alone. That is
            // also what makes an interrupted run resume instead of starting over.
            if (!options_.overwrite && (fileExists(base + ".jpg") || fileExists(base + ".png"))) {
                ++skipped_;
                matched = true;
                continue;
            }

            const std::string remote = index_.match(job.name);
            if (remote.empty()) continue;
            matched = true;

            if (!makeDirectory(job.mediaDir)) continue;

            const std::string url = std::string(LibretroIndex::kHost) + "/" +
                                    Downloader::encodeComponent(index_.platform()) + "/" +
                                    kind.folder + "/" +
                                    Downloader::encodeComponent(remote) + ".png";

            if (fetchOne(url, base + ".jpg", options_.maxEdge)) ++fetched_;
        }

        if (anyWanted && !matched) noteMiss(currentSystem_, job.name);
        return;
    }

    default:
        return;
    }
}
