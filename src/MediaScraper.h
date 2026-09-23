#pragma once

#include <string>
#include <vector>

#include "Downloader.h"
#include "GameDatabase.h"
#include "LibretroIndex.h"

// Fetches box art and background images for everything in the game database.
//
// Shrinks and re-encodes each picture before it is written, so the drive only ever sees the
// finished file — roughly 76 KB instead of 400. Everything in between happens in /tmp, which
// is a RAM disk on a MiSTer. That matters more than it sounds: a full library is some fifteen
// thousand images, and writing a gigabyte to a marginally powered USB drive is exactly the
// load that knocks one off the bus.
//
// Runs one game per step, driven from the frame loop. A download takes about a second, so
// that also sets the pace: the drive sees a small write about once a second rather than a
// continuous stream. Interrupting costs nothing — artwork that is already there is skipped,
// so the next run carries on where this one stopped.
class MediaScraper {
public:
    enum class State { Idle, Preparing, Working, Done, Failed };

    struct Options {
        bool boxart = true;
        bool backgrounds = true;
        bool overwrite = false;   // off means "fill the gaps", which is also how it resumes
        int maxEdge = 512;        // long side after shrinking
        // A second, smaller copy written alongside the full-size one — `<name>-sm.jpg` — for
        // grid tiles, which never draw anything close to 512 pixels wide. 300 covers every
        // tile size up to Boxart large (see GamesScreen's kSmallArtworkMaxTile) with room to
        // spare, while still being a third of the source's pixel count to decode.
        int smallMaxEdge = 300;
        int quality = 85;

        // Both live on the card, never on the game drive. Parameters so the whole thing can
        // be exercised against scratch directories.
        std::string indexCacheDir = kIndexCache;
        std::string missesFile = kMissesFile;
    };

    // `systemKeys` empty means every system in the database.
    void start(const GameDatabase &database, const Options &options,
               const std::vector<std::string> &systemKeys = {});

    void step();
    void cancel();

    State state() const { return state_; }
    bool running() const { return state_ == State::Preparing || state_ == State::Working; }
    bool finished() const { return state_ == State::Done || state_ == State::Failed; }

    float progress() const;
    std::string statusLine() const;

    size_t fetched() const { return fetched_; }
    size_t skipped() const { return skipped_; }
    size_t missing() const { return missing_; }
    const std::string &error() const { return error_; }

    // Where the list of titles nothing could be found for is written.
    static constexpr const char *kMissesFile = MISTER_PAT_ROOT "/scrape-misses.txt";
    static constexpr const char *kIndexCache = MISTER_PAT_ROOT "/gamesdb/scrape-index";

private:
    struct Job {
        std::string path;       // the game
        std::string name;       // display name, which is also the artwork's name
        std::string mediaDir;   // where the artwork goes
    };

    bool prepareNextSystem();
    // `base` is the destination without an extension — this writes both `base.jpg` (full
    // size) and `base-sm.jpg` (grid size) from the one download.
    bool fetchOne(const std::string &url, const std::string &base);
    // For a game scraped before `-sm.jpg` existed: makes the small copy from the full-size
    // file already on disk, no network access at all.
    bool refreshSmall(const std::string &base);
    void noteMiss(const std::string &system, const std::string &title);

    State state_ = State::Idle;
    Options options_;
    const GameDatabase *database_ = nullptr;
    Downloader downloader_;
    LibretroIndex index_;

    std::vector<DatabaseSystem> systems_;
    size_t systemPosition_ = 0;
    std::string currentSystem_;
    std::string currentTitle_;

    std::vector<Job> jobs_;
    size_t jobPosition_ = 0;

    size_t totalGames_ = 0;
    size_t doneGames_ = 0;
    size_t fetched_ = 0;
    size_t skipped_ = 0;
    size_t missing_ = 0;
    bool missesOpened_ = false;
    std::string error_;
};
