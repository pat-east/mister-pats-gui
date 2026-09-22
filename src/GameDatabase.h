#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Paths.h"

// One system as the database records it.
struct DatabaseSystem {
    std::string key;     // directory name; also the name of this system's game list file
    std::string name;    // display name
    std::string group;   // "Consoles", "Computers", …
    std::string core;    // e.g. "_Console/PSX"
    std::string dir;     // the directory the games were found in
    size_t count = 0;    // games recorded
    bool discBased = false;
};

// The GUI's own record of what is installed, written by a scan and read back at startup.
//
// Deliberately not one big file. The catalogue is a handful of lines and is read every start;
// a system's game list is only read when that system is opened. With a few thousand games on
// one console that difference decides whether opening the Systems tab costs nothing or costs
// a second — and on a marginally powered drive, reading less is also what keeps it on the bus.
//
//   gamesdb/roots.tsv     <id> \t <mount path> \t <probe path relative to that root>
//   gamesdb/catalog.tsv   <key> \t <name> \t <group> \t <core> \t <disc> \t <count> \t <dir>
//   gamesdb/<key>.tsv     <root id> \t <path relative to that root>
//
// Game paths are stored relative to a root, and the root is re-resolved every time the
// database is loaded. MiSTer hands out /media/usb0, usb1 … in the order devices appear, so
// the same drive can come back at a different mount point after a reboot. Storing relative
// paths alone would not survive that — the recorded root would still name the old place. The
// probe path is how a root is recognised again: the first game directory found on it.
class GameDatabase {
public:
    // The directory is a parameter so the class can be exercised against a scratch folder;
    // everything in the application uses the default.
    explicit GameDatabase(std::string directory = kDirectory)
        : directory_(std::move(directory)) {}

    // Where to look when a recorded root is no longer at its old mount point. Defaults to
    // the MiSTer's own mount points; a test can point it at scratch directories instead.
    void setMountPoints(std::vector<std::string> points) { candidates_ = std::move(points); }

    bool exists() const;

    // Reads roots and catalogue only — never a game list.
    bool load();

    const std::vector<DatabaseSystem> &systems() const { return systems_; }
    const std::vector<std::string> &roots() const { return roots_; }
    size_t totalGames() const;
    bool empty() const { return systems_.empty(); }

    // Roots whose drive was not found this time. Their games are unreachable, and saying so
    // is better than showing a system whose every entry fails to start.
    const std::vector<std::string> &missingRoots() const { return missingRoots_; }

    // True when a root now sits at a different mount point than when it was scanned. Worth
    // reporting: it means the drive order changed, not that anything is broken.
    bool rootsMoved() const { return rootsMoved_; }

    // Reads one system's list, on demand. Absolute paths, sorted as written.
    std::vector<std::string> pathsFor(const std::string &key) const;

    // Writing. Everything goes to a staging directory and is only moved into place by
    // `finishWrite`, so a cancelled or failed scan leaves the working database untouched.
    bool beginWrite(const std::vector<std::string> &roots);
    bool writeSystem(const DatabaseSystem &system, const std::vector<std::string> &paths);
    bool finishWrite();

    // Makes headway on deleting whatever the last `finishWrite` swapped out, one filesystem
    // operation at a time — a no-op once there is nothing left, so it is safe to call every
    // frame. Deliberately not part of `finishWrite` itself: on a card that forces a sync on
    // every write, deleting an old generation's files in one unbroken burst is exactly what
    // made this step look hung in the first place. Meant to be called from the *next* scan's
    // own stepping, where a few extra filesystem operations disappear into work that already
    // takes a while.
    void pruneOldGenerationStep();

    const std::string &lastError() const { return error_; }

    // Where MiSTer mounts things. Used both to re-resolve a moved root and, by the scanner,
    // to decide what to look at in the first place.
    static std::vector<std::string> mountPoints();

    static constexpr const char *kDirectory = MISTER_PAT_ROOT "/gamesdb";

private:
    struct Root {
        std::string path;   // where it was when scanned
        std::string probe;  // a directory that must exist on it, relative to the root
    };

    std::string pathOf(const std::string &file) const;
    std::string stagingPathOf(const std::string &file) const;
    std::string stagingDirectory() const { return directory_ + ".new"; }
    int rootIdFor(const std::string &absolutePath) const;
    void resolveRoots(const std::vector<Root> &recorded);

    std::string directory_;
    std::vector<std::string> candidates_ = mountPoints();
    std::vector<std::string> roots_;        // resolved, indexed by id
    std::vector<std::string> missingRoots_;
    bool rootsMoved_ = false;

    std::vector<Root> writing_;             // roots being written, with their probes
    std::vector<DatabaseSystem> systems_;
    std::vector<DatabaseSystem> pending_;   // collected during a write
    std::string error_;

    // Pruning state for the previous generation's leftovers. `void *` rather than `DIR *` so
    // this header does not need <dirent.h>.
    void *pruneHandle_ = nullptr;
    bool pruneChecked_ = false;
};
