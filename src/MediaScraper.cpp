#include "MediaScraper.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "DebugLog.h"
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

// Shared by a fresh download and a local re-shrink of art already on disk — the only
// difference is where the source image came from.
bool writeShrunk(ImagePtr image, const std::string &path, int maxEdge, int quality) {
    if (!image || !image->valid()) return false;

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
    const std::string temporary = path + ".part";
    if (!image->saveJpeg(temporary, quality)) return false;
    return rename(temporary.c_str(), path.c_str()) == 0;
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
        DebugLog::warn("scraper: " + error_);
        state_ = State::Failed;
        return;
    }

    // The game lists are read one system at a time, only when that system's turn comes.
    database_ = &database;
    state_ = State::Preparing;

    char line[160];
    std::snprintf(line, sizeof(line), "scraper: starting, %zu systems, %zu games", systems_.size(),
                 totalGames_);
    DebugLog::info(line);
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

bool MediaScraper::fetchOne(const std::string &url, const std::string &base) {
    if (!downloader_.fetch(url, kTempImage, 25)) return false;

    ImagePtr image = Image::load(kTempImage);
    unlink(kTempImage);
    if (!image) return false;

    // Two sizes from the one download: full-size for the rare view that wants it (Boxart
    // large, or the detail panel in List view), and a grid-size copy for everywhere else —
    // which is most of the time a picture is actually drawn. The small file's write is not
    // load-bearing for this job's own success; a full-size cover with no small copy still
    // renders fine, just not at its best decode speed until it is regenerated.
    const bool full = writeShrunk(image, base + ".jpg", options_.maxEdge, options_.quality);
    writeShrunk(image, base + "-sm.jpg", options_.smallMaxEdge, options_.quality);
    return full;
}

bool MediaScraper::refreshSmall(const std::string &base) {
    return writeShrunk(Image::load(base + ".jpg"), base + "-sm.jpg", options_.smallMaxEdge,
                       options_.quality);
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

            char line[256];
            std::snprintf(line, sizeof(line), "scraper: %s skipped, %s", system.name.c_str(),
                         index_.lastError().c_str());
            DebugLog::warn(line);

            doneGames_ += system.count;
            ++systemPosition_;
            continue;
        }

        // Its root did not resolve — the drive it lives on is not attached right now, so
        // there is nowhere to write artwork for it even if the thumbnail lookup succeeds.
        if (system.dir.empty()) {
            doneGames_ += system.count;
            ++systemPosition_;
            continue;
        }

        GameSystem shape;
        shape.discBased = system.discBased;
        shape.romDirs.push_back(system.dir);

        const std::string mediaDir = system.dir + "/media";
        for (const std::string &path : database_->pathsFor(system.key))
            jobs_.push_back({path, Library::nameFor(shape, path), mediaDir});

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

            char line[200];
            std::snprintf(line, sizeof(line),
                         "scraper: done, %zu fetched, %zu already there, %zu not found",
                         fetched_, skipped_, missing_);
            DebugLog::info(line);
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
            const bool hasFull = fileExists(base + ".jpg");

            // Scraped before -sm.jpg existed: make the small copy from the full-size file
            // already on disk instead of treating this as new work. No network involved, so
            // it runs regardless of whether the index below can even be reached.
            if (!options_.overwrite && hasFull && !fileExists(base + "-sm.jpg")) {
                if (refreshSmall(base)) ++fetched_;
                matched = true;
                continue;
            }

            // Artwork that is already there — ours or anyone else's — is left alone. That is
            // also what makes an interrupted run resume instead of starting over.
            if (!options_.overwrite && (hasFull || fileExists(base + ".png"))) {
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

            if (fetchOne(url, base)) ++fetched_;
        }

        if (anyWanted && !matched) noteMiss(currentSystem_, job.name);
        return;
    }

    default:
        return;
    }
}
