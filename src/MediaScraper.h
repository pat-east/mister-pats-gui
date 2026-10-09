#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Downloader.h"
#include "GameDatabase.h"
#include "Image.h"
#include "LibretroIndex.h"

// Fetches box art and background images for everything in the game database.
//
// Writes JPEG fallbacks plus pre-sized, uncompressed BMPs for each view. A full library uses
// more space for those variants, but the GUI can decode them without a JPEG pass or a load-time
// resize. Existing local artwork is converted during the same paced job; missing artwork is
// fetched from the network.
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
        // A smaller JPEG fallback for installations where the view-specific BMP has not been
        // generated yet. The normal GUI path prefers the native-size BMP variant.
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
    // Estimated remaining seconds from elapsed time and completed games, or -1 before
    // the first game is complete.
    int64_t estimatedRemainingSeconds() const;
    std::string statusLine() const;

    size_t fetched() const { return fetched_; }
    size_t prepared() const { return prepared_; }
    size_t skipped() const { return skipped_; }
    size_t missing() const { return missing_; }
    const std::string &error() const { return error_; }

    // Where the list of titles nothing could be found for is written.
    static constexpr const char *kMissesFile = MISTER_PAT_ROOT "/scrape-misses.txt";

    // A sibling of gamesdb/, deliberately not inside it: that whole directory is treated as
    // one disposable, atomically-swapped unit by GameDatabase (gamesdb -> gamesdb.old,
    // gamesdb.new -> gamesdb), and its cleanup only ever knew about the .tsv files that
    // belong to it. A cache folder living inside it too meant that cleanup could never
    // actually empty gamesdb.old, which made every rebuild after the first fail outright —
    // rename() refusing to replace a directory that still has something in it.
    static constexpr const char *kIndexCache = MISTER_PAT_ROOT "/scrape-index";

private:
    struct Job {
        std::string path;       // the game
        std::string name;       // display name, which is also the artwork's name
        std::string mediaDir;   // where the artwork goes
    };

    bool prepareNextSystem();
    // `base` is the destination without an extension. A box-art download also creates the
    // view-specific BMPs from the decoded source image.
    bool fetchOne(const std::string &url, const std::string &base, bool makeViewVariants);
    // For a game scraped before `-sm.jpg` existed: makes the small copy from the full-size
    // file already on disk, no network access at all.
    bool refreshSmall(const std::string &base);
    bool prepareViewVariants(const std::string &base, ImagePtr source = nullptr,
                             bool overwrite = false);
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
    bool indexAvailable_ = false;

    size_t totalGames_ = 0;
    size_t doneGames_ = 0;
    int64_t startedAtMs_ = 0;
    size_t fetched_ = 0;
    size_t prepared_ = 0;
    size_t skipped_ = 0;
    size_t missing_ = 0;
    bool missesOpened_ = false;
    bool arcade_ = false;   // the system being worked on is Arcade — see prepareNextSystem()
    std::string error_;
};
