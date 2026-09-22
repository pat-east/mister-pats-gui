#pragma once

#include "Paths.h"

#include <string>
#include <vector>

#include "GameDatabase.h"
#include "GameIndex.h"

struct Game {
    std::string path;        // ROM or archive on disk
    std::string name;        // display name, extension stripped
    std::string boxart;      // candidate path, may not exist
    std::string background;  // candidate "-BG" path, may not exist

    // A quarter of a real library sits in subfolders — "NES/Japan/…" and the like — while
    // the artwork stays in the system's own media folder. These are that second place to
    // look, and are empty when the game sits directly in the system directory.
    std::string boxartFallback;
    std::string backgroundFallback;

    bool isArchive() const;
};

struct GameSystem {
    std::string name;                     // e.g. "SNES"
    std::string group;                    // e.g. "Consoles"
    std::string core;                     // e.g. "_Console/SNES", empty when unmatched
    std::string dbKey;                    // this system's file in the game database, if any
    std::vector<std::string> romDirs;     // existing directories only
    std::vector<std::string> romExts;
    std::vector<std::string> mediaDirs;   // existing boxart directories
    int fileIndex = 1;                    // file slot in the core menu
    char fileType = 'f';                  // 'f' file, 's' disk slot (CD based cores)
    bool discBased = false;               // games are folders of tracks, not single files

    bool launchable() const { return !core.empty(); }
};

// The catalogue of systems and the games in them.
//
// Three sources, in order of preference: our own game database, Console Mode's cache, and —
// only when the user asks — a live walk of the drives. The database is what makes the GUI
// independent; the other two remain so that an installation without one still works.
class Library {
public:
    bool load(const std::string &sectionDir = kDefaultSectionDir);

    // True when the catalogue came from our own database rather than Console Mode.
    bool usingDatabase() const { return usingDatabase_; }
    const GameDatabase &database() const { return database_; }

    const std::vector<GameSystem> &systems() const { return systems_; }
    std::vector<Game> gamesOf(const GameSystem &system) const;

    // Stops at the first match, so filtering the system list stays cheap.
    bool hasGames(const GameSystem &system) const;

    const GameIndex &index() const { return index_; }

    // Walking the game volume is what drops a marginally powered drive off the bus,
    // so it never happens on its own — only when the user asks for a rescan.
    void setScanningAllowed(bool allowed) { scanningAllowed_ = allowed; }
    bool scanningAllowed() const { return scanningAllowed_; }

    const GameSystem *findSystem(const std::string &name) const;

    // Resolves the owning system from a ROM path, for entries that only store one.
    const GameSystem *systemForPath(const std::string &path) const;

    // Rebuilds a game entry from a stored path, e.g. when restoring favourites.
    static Game makeGame(const GameSystem &system, const std::string &path);

    static constexpr const char *kDefaultSectionDir =
        "/media/fat/ConsoleMode/themeconfig/section_groups";

    // Optional overrides, one system per line:  <NAME>  <index>  [f|s]
    // The right slot depends on the core's own menu and cannot be derived from the
    // filesystem, so it stays adjustable without rebuilding.
    static constexpr const char *kOverridesFile = MISTER_PAT_ROOT "/systems.conf";

private:
    struct CoreEntry {
        std::string key;      // normalised name used for matching
        std::string relPath;  // e.g. "_Console/SNES"
    };

    void indexCores();
    std::string matchCore(const GameSystem &system) const;
    bool loadFromDatabase();
    bool loadSectionFile(const std::string &file, const std::string &group);
    void applySlotDefaults();
    void applyOverrides();

    std::vector<GameSystem> systems_;
    std::vector<CoreEntry> cores_;
    GameIndex index_;
    GameDatabase database_;
    bool usingDatabase_ = false;
    bool scanningAllowed_ = false;
};
