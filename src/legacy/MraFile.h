#pragma once

#include <cstdint>
#include <string>
#include <vector>

// One ROM part an .mra's <rom> block asks for, with the CRC32 MiSTer checks the actual zip
// contents against. `zip` is empty in the common case, where the part uses its parent
// ArcadeRomBlock's own zip attribute instead of naming one of its own.
struct ArcadeRomPart {
    std::string name;
    uint32_t crc = 0;
    bool hasCrc = false;  // a part with no crc attribute at all is checked for presence only
    std::string zip;
};

// One <rom index="N" zip="..."> block that actually names a zip. An index="0" game-selector
// block (a literal inline byte, no external file at all) never becomes one of these — see
// MraFile::parse.
struct ArcadeRomBlock {
    int index = 0;
    std::string zip;  // "|"-separated alternates, exactly as the .mra wrote it
    std::vector<ArcadeRomPart> parts;
};

// One .mra file, parsed, plus what a scan of the actual drives found for it.
struct ArcadeEntry {
    std::string mraPath;  // absolute path to the .mra itself
    std::string root;     // the volume it lives on, e.g. "/media/fat"

    std::string name;
    std::string year;
    std::string manufacturer;
    std::string category;
    std::string rbf;  // <rbf> content, e.g. "KillerInstinct"

    std::vector<ArcadeRomBlock> romBlocks;

    // Filled by MraFile::resolveCore().
    bool coreFound = false;
    std::string coreFile;  // resolved filename, e.g. "Arcade-KillerInstinct_20260924.rbf"

    // Filled by MraFile::resolveRom().
    bool romFound = false;  // every zip a rom block needs was located somewhere
    bool romCrcOk = false;  // every part found in its zip matched the .mra's own CRC
    std::string romName;    // the zip file(s) this game wants, for display — populated from
                            // the .mra itself regardless of whether anything was found

    // The one column meant to answer "would this launch at all" — nothing finer than that.
    bool mightWork() const { return coreFound && romFound && romCrcOk; }
};

// Parses one .mra, and resolves the two lookups MiSTer's own arcade loader does at load time
// (see docs/ARCADE.md, "Two independent root lookups" — these are deliberately two different
// searches, not one, and conflating them is exactly the bug that document's own case study
// found on real hardware).
class MraFile {
public:
    // Reads name/year/manufacturer/category/rbf and every zip-backed <rom> block. `root` and
    // `mraPath` are set from `path` directly; core/rom presence is not touched — call
    // resolveCore()/resolveRom() separately. False when the file could not be read at all.
    static bool parse(const std::string &path, ArcadeEntry &entry);

    // "<volume>/_Arcade/cores", matching "Arcade-<rbf>*.rbf" case-insensitively and keeping the
    // lexicographically last match — the exact algorithm `get_rbf` in MiSTer's own
    // support/arcade/mra_loader.cpp uses, which is why a dated release sorts after an undated
    // copy of the same core rather than needing special-casing. `entry.root` decides which
    // volume's cores directory is searched; this never looks at any other volume, matching
    // MiSTer's own behaviour (the core stays on the same drive the .mra was loaded from).
    static void resolveCore(ArcadeEntry &entry);

    // The ROM zip search, using MiSTer's own global order — every mounted USB volume first,
    // ascending, then the SD card last — regardless of which volume the .mra itself is on.
    // `gameSearchRoots` must already be in that order; see misterGameSearchOrder().
    static void resolveRom(ArcadeEntry &entry, const std::vector<std::string> &gameSearchRoots);

    // Every mounted volume, in the order MiSTer's own file_io.cpp (findPrefixDir) actually
    // searches them for a "games/..." directory: /media/usb0 .. /media/usb5, then /media/fat
    // last. Deliberately not the order GameDatabase::mountPoints() uses — that order exists for
    // a different reason (device enumeration) and is the wrong one here; see docs/ARCADE.md, "Two
    // independent root lookups".
    static std::vector<std::string> misterGameSearchOrder(const std::vector<std::string> &mountedRoots);

    // Every currently mounted volume this project already knows how to find:
    // /media/fat, /media/usb0 .. /media/usb7 — filtered to the ones that actually exist.
    static std::vector<std::string> mountedRoots();
};
