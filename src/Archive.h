#pragma once

#include <cstdint>
#include <string>
#include <vector>

// One file inside a zip, as its central directory records it.
struct ArchiveEntry {
    std::string name;
    uint32_t crc32 = 0;
};

// Reads entry names (and, where needed, CRC32s) from a zip's central directory. MiSTer
// addresses an archived ROM as "<archive>.zip/<inner path>"; a path pointing at the bare
// archive loads nothing.
class Archive {
public:
    static std::vector<std::string> entries(const std::string &path);

    // Same central directory walk as `entries()`, with each entry's stored CRC32 alongside its
    // name — what an arcade ROM-set check needs and a plain listing does not.
    static std::vector<ArchiveEntry> list(const std::string &path);

    // First entry matching one of the extensions, empty when none fits.
    static std::string findRom(const std::string &path, const std::vector<std::string> &extensions);
};
