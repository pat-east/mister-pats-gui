#include "MediaScraper.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "DebugLog.h"
#include "Image.h"
#include "Library.h"
#include "SystemCatalog.h"

namespace {

// /tmp is a RAM disk on a MiSTer, so nothing here ever touches a drive.
const char *kTempImage = "/tmp/mister-pat-scrape.img";

int64_t monotonicMs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return int64_t(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

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

bool writeBmpVariant(const ImagePtr &source, const std::string &base,
                     ArtworkVariant variant, bool overwrite) {
    const ArtworkBounds bounds = artworkBounds(variant);
    const std::string path = base + bounds.suffix + ".bmp";
    if (!overwrite && fileExists(path)) return false;

    int width = source->width();
    int height = source->height();
    if (long(width) * bounds.height > long(height) * bounds.width) {
        height = std::max(1, int(long(height) * bounds.width / width));
        width = bounds.width;
    } else {
        width = std::max(1, int(long(width) * bounds.height / height));
        height = bounds.height;
    }

    ImagePtr sized = source->scaledTo(width, height);
    if (!sized) return false;

    const std::string temporary = path + ".part";
    if (!sized->saveBmp(temporary)) {
        unlink(temporary.c_str());
        return false;
    }
    if (rename(temporary.c_str(), path.c_str()) != 0) {
        unlink(temporary.c_str());
        return false;
    }
    return true;
}

} // namespace

void MediaScraper::start(const GameDatabase &database, const Options &options,
                         const std::vector<std::string> &systemKeys) {
    options_ = options;
    systems_.clear();
    jobs_.clear();
    systemPosition_ = jobPosition_ = 0;
    totalGames_ = doneGames_ = fetched_ = prepared_ = skipped_ = missing_ = 0;
    startedAtMs_ = monotonicMs();
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

int64_t MediaScraper::estimatedRemainingSeconds() const {
    if (state_ == State::Done || (totalGames_ && doneGames_ >= totalGames_)) return 0;
    if (!running() || !doneGames_) return -1;

    // Use the same completed/total counts as the progress display. Elapsed wall time
    // includes downloads, image conversion, index preparation and frame pacing.
    const int64_t elapsedMs = monotonicMs() - startedAtMs_;
    if (elapsedMs <= 0) return -1;
    const double remainingMs = double(elapsedMs) * double(totalGames_ - doneGames_) /
                               double(doneGames_);
    return int64_t((remainingMs + 999.0) / 1000.0);
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
        std::snprintf(buffer, sizeof(buffer),
                      "%zu fetched, %zu optimized for views, %zu already there, %zu not found",
                      fetched_, prepared_, skipped_, missing_);
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

bool MediaScraper::fetchOne(const std::string &url, const std::string &base,
                            bool makeViewVariants) {
    if (!downloader_.fetch(url, kTempImage, 25)) return false;

    ImagePtr image = Image::load(kTempImage);
    unlink(kTempImage);
    if (!image) return false;

    // Keep JPEG fallbacks for compatibility, then write all view-sized BMPs from the decoded
    // download so we do not decode the JPEG again for each presentation.
    const bool full = writeShrunk(image, base + ".jpg", options_.maxEdge, options_.quality);
    writeShrunk(image, base + "-sm.jpg", options_.smallMaxEdge, options_.quality);
    if (makeViewVariants && prepareViewVariants(base, image, true)) ++prepared_;
    return full;
}

bool MediaScraper::prepareViewVariants(const std::string &base, ImagePtr source, bool overwrite) {
    const ArtworkVariant variants[] = {ArtworkVariant::Home, ArtworkVariant::Grid,
                                       ArtworkVariant::Small, ArtworkVariant::Detail,
                                       ArtworkVariant::Arcade};
    bool needsWrite = overwrite;
    if (!overwrite) {
        for (ArtworkVariant variant : variants) {
            const ArtworkBounds bounds = artworkBounds(variant);
            if (!fileExists(base + bounds.suffix + ".bmp")) {
                needsWrite = true;
                break;
            }
        }
    }
    if (!needsWrite) return false;

    if (!source) source = Image::load(base + ".jpg");
    if (!source) source = Image::load(base + "-sm.jpg");
    if (!source) source = Image::load(base + ".png");
    if (!source || !source->valid()) return false;

    bool written = false;
    for (ArtworkVariant variant : variants)
        written = writeBmpVariant(source, base, variant, overwrite) || written;
    return written;
}

bool MediaScraper::refreshSmall(const std::string &base) {
    ImagePtr source = Image::load(base + ".jpg");
    const bool refreshed = writeShrunk(source, base + "-sm.jpg", options_.smallMaxEdge,
                                       options_.quality);
    if (prepareViewVariants(base, source, false)) ++prepared_;
    return refreshed;
}

bool MediaScraper::prepareNextSystem() {
    jobs_.clear();
    jobPosition_ = 0;

    while (systemPosition_ < systems_.size()) {
        const DatabaseSystem &system = systems_[systemPosition_];
        currentSystem_ = system.name;

        indexAvailable_ = index_.open(LibretroIndex::platformFor(system.key), downloader_,
                                      options_.indexCacheDir);
        if (!indexAvailable_) {
            // Existing local artwork can still be prepared as view-sized BMPs without the
            // network. Only missing artwork needs the thumbnail index.
            std::printf("scraper: %s index unavailable, local artwork only: %s\n",
                         system.name.c_str(),
                         index_.lastError().c_str());

            char line[256];
            std::snprintf(line, sizeof(line),
                          "scraper: %s index unavailable, local artwork only: %s",
                          system.name.c_str(), index_.lastError().c_str());
            DebugLog::warn(line);

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

        arcade_ = system.key == "Arcade";

        const std::string mediaDir = system.dir + "/media";
        for (const std::string &path : database_->pathsFor(system.key)) {
            // An older database may still contain boot media until it is rebuilt. Do not
            // waste thumbnail lookups on those entries or list them as missing artwork.
            if (SystemCatalog::isSystemFile(system.key, path)) {
                ++doneGames_;
                continue;
            }
            // Every other system keeps its games in one directory, so one media folder does
            // for all of them. Arcade's .mra files sit on every mounted volume, each with
            // its own _Arcade folder, and artwork belongs next to the volume its game is on
            // — the place Library::resolveArtwork looks.
            std::string where = mediaDir;
            if (arcade_) {
                const size_t marker = path.find("/_Arcade/");
                where = marker == std::string::npos ? mediaDir
                                                    : path.substr(0, marker) + "/_Arcade/media";
            }
            jobs_.push_back({path, Library::nameFor(shape, path), where});
        }

        ++systemPosition_;
        return true;
    }

    return false;
}

void MediaScraper::step() {
    switch (state_) {
    case State::Preparing:
        {
        if (!prepareNextSystem()) {
            state_ = State::Done;

            char line[200];
            std::snprintf(line, sizeof(line),
                         "scraper: done, %zu fetched, %zu optimized, %zu already there, %zu not found",
                         fetched_, prepared_, skipped_, missing_);
            DebugLog::info(line);
            return;
        }
        state_ = State::Working;
        return;
        }

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

            // MAME's "snaps" are gameplay screenshots, which may not make a pleasant
            // full-screen background the way console fan art does. Not looked at yet, so
            // not fetched — see docs/ARCADE.md. Box art only for Arcade until that is settled.
            if (arcade_ && kind.suffix[0] == '-') continue;

            anyWanted = true;

            const std::string base = job.mediaDir + "/" + job.name + kind.suffix;
            const bool hasFull = fileExists(base + ".jpg");

            if (kind.suffix[0] == '\0' && !options_.overwrite &&
                (hasFull || fileExists(base + "-sm.jpg") || fileExists(base + ".png")) &&
                prepareViewVariants(base))
                ++prepared_;

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
            if (!options_.overwrite &&
                (hasFull || fileExists(base + "-sm.jpg") || fileExists(base + ".png"))) {
                ++skipped_;
                matched = true;
                continue;
            }

            if (!indexAvailable_) {
                ++skipped_;
                matched = true;
                continue;
            }

            // Arcade first by the whole title, brackets and all: every ROM revision has its
            // own cover, and the loose match below would pick one of them at random. The
            // loose match stays as the fallback for a renamed .mra or a revision the server
            // does not carry, which gets a cover for the right game, if not the right build.
            std::string remote;
            if (arcade_) remote = index_.matchExact(job.name);
            if (remote.empty()) remote = index_.match(job.name);
            if (remote.empty()) continue;
            matched = true;

            if (!makeDirectory(job.mediaDir)) continue;

            const std::string url = std::string(LibretroIndex::kHost) + "/" +
                                    Downloader::encodeComponent(index_.platform()) + "/" +
                                    kind.folder + "/" +
                                    Downloader::encodeComponent(remote) + ".png";

            if (fetchOne(url, base, kind.suffix[0] == '\0')) ++fetched_;
        }

        if (anyWanted && !matched) noteMiss(currentSystem_, job.name);
        return;
    }

    default:
        return;
    }
}
