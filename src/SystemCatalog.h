#pragma once

#include <string>
#include <vector>

class CoreIndex;

// One playable system, as found on disk.
struct CatalogEntry {
    std::string key;         // the directory name, e.g. "PSX" — filesystem-safe, names the db file
    std::string name;        // what the user sees, e.g. "PlayStation"
    std::string group;       // "Consoles", "Computers", …, taken from where the core lives
    std::string core;        // e.g. "_Console/PSX"
    std::string root;        // the volume it was found on, e.g. "/media/usb0"
    std::string dir;         // the absolute game directory
    std::vector<std::string> extensions;   // empty means "anything not obviously not a game"
    bool discBased = false;  // games are folders holding a disc image, not single files
};

// Works out which systems exist by looking at what is actually on the drives, rather than
// carrying a list of every machine MiSTer supports.
//
// A system is present when a game directory with its name exists and a core of that name is
// installed. That second condition is what makes the result trustworthy: a stray folder is
// not a system, and a system nobody can play is not worth listing. The group comes from the
// core's own directory, so it needs no table at all.
class SystemCatalog {
public:
    // One readdir per candidate parent directory, no recursion. Cheap enough to run on a
    // marginally powered drive, which is the whole reason the index exists.
    static std::vector<CatalogEntry> discover(const std::vector<std::string> &roots,
                                              const CoreIndex &cores);

    // The pretty name for a game directory, e.g. "TGFX16" -> "TurboGrafx-16". Falls back to
    // the directory name, which is already readable for most systems.
    static std::string displayName(const std::string &directoryName);

    // Known ROM extensions, lower case and without the dot. Empty when the system is not in
    // the table, in which case `looksLikeGame` falls back to rejecting known non-game files.
    static std::vector<std::string> extensionsFor(const std::string &directoryName);

    // Whether a file inside a system's directory should be treated as a game.
    static bool looksLikeGame(const std::string &filename,
                              const std::vector<std::string> &extensions);

    // Directories that live inside a system's folder but never hold a game.
    static bool isIgnoredDirectory(const std::string &name);

    // Where games live, relative to a volume. Both layouts are in the wild.
    static std::vector<std::string> gameParents(const std::string &root);
};
