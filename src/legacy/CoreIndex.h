#pragma once

#include <string>
#include <vector>

// The cores installed on the SD card, by name.
//
// Two things fall out of this that the catalogue would otherwise have to be told: whether a
// system is playable at all (no core, no point listing it) and which group it belongs to,
// because MiSTer already sorts its cores into _Console, _Computer, _Handheld and so on.
class CoreIndex {
public:
    // Cores normally live on the SD card, but nothing stops someone putting them on a USB
    // volume, so every mounted root is looked at. Passing the roots in also lets the index be
    // exercised against a drive mounted somewhere else entirely.
    void scan(const std::vector<std::string> &roots);

    // Relative path as an MGL wants it, e.g. "_Console/PSX". Empty when nothing matches.
    // `names` are tried in order, each first exactly and then by prefix, which catches
    // "GAMEBOYCOLOR" against a core called "GBC".
    std::string find(const std::vector<std::string> &names) const;

    // "_Console/PSX" -> "Consoles". Empty for a path that carries no group.
    static std::string groupOf(const std::string &relPath);

    // Uppercase letters and digits only, so "ATARI 2600", "Atari2600" and "atari-2600" match.
    static std::string normalise(const std::string &s);

    size_t size() const { return cores_.size(); }
    bool empty() const { return cores_.empty(); }

private:
    struct Entry {
        std::string key;      // normalised core name
        std::string relPath;  // e.g. "_Console/SNES"
    };

    std::vector<Entry> cores_;
};
