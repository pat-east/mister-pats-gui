// Host-side checks for Archive's central-directory reader — both the plain name listing every
// archived-ROM lookup already relied on, and the CRC32 extraction the arcade diagnostics
// screen needs on top of that (see MraFile.cpp).
//
// These fixtures are not real, valid zip files — Archive::entries()/list() never reads local
// file headers or compressed data at all, only the central directory and the end-of-central-
// directory record, so a fixture that is *only* those two pieces exercises exactly what is
// being tested, the same way canvas_damage_test drives Canvas with hand-built pixel data
// rather than a real screenshot.
//
// Build and run:  make -f tests/Makefile

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "../src/Archive.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

void writeU16(std::string &buf, uint16_t v) {
    buf += char(v & 0xFF);
    buf += char((v >> 8) & 0xFF);
}

void writeU32(std::string &buf, uint32_t v) {
    buf += char(v & 0xFF);
    buf += char((v >> 8) & 0xFF);
    buf += char((v >> 16) & 0xFF);
    buf += char((v >> 24) & 0xFF);
}

// A central directory plus its end record, and nothing else — see the file comment above for
// why that is enough.
std::string buildFakeZip(const std::vector<ArchiveEntry> &entries) {
    std::string central;
    for (const ArchiveEntry &e : entries) {
        central += std::string("PK\x01\x02", 4);
        writeU16(central, 0);   // version made by
        writeU16(central, 0);   // version needed
        writeU16(central, 0);   // flags
        writeU16(central, 0);   // method
        writeU16(central, 0);   // time
        writeU16(central, 0);   // date
        writeU32(central, e.crc32);
        writeU32(central, 0);   // compressed size
        writeU32(central, 0);   // uncompressed size
        writeU16(central, uint16_t(e.name.size()));
        writeU16(central, 0);   // extra length
        writeU16(central, 0);   // comment length
        writeU16(central, 0);   // disk number start
        writeU16(central, 0);   // internal attributes
        writeU32(central, 0);   // external attributes
        writeU32(central, 0);   // relative offset of local header
        central += e.name;
    }

    std::string eocd;
    eocd += std::string("PK\x05\x06", 4);
    writeU16(eocd, 0);
    writeU16(eocd, 0);
    writeU16(eocd, uint16_t(entries.size()));
    writeU16(eocd, uint16_t(entries.size()));
    writeU32(eocd, uint32_t(central.size()));
    writeU32(eocd, 0);  // the central directory starts at the very beginning of this file
    writeU16(eocd, 0);

    return central + eocd;
}

void writeFile(const std::string &path, const std::string &content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

} // namespace

int main() {
    const std::string path = "/tmp/mister_gui_archive_test.zip";

    writeFile(path, buildFakeZip({
                        {"ki-l15d.u98", 0x7b65ca3d},
                        {"u10-l1", 0xb6cc155f},
                        {"__MACOSX/", 0},  // a directory entry — must not appear in the listing
                    }));

    const std::vector<ArchiveEntry> list = Archive::list(path);
    check(list.size() == 2, "directory entries are excluded, only files remain");

    bool foundFirst = false, foundSecond = false;
    for (const ArchiveEntry &e : list) {
        if (e.name == "ki-l15d.u98" && e.crc32 == 0x7b65ca3d) foundFirst = true;
        if (e.name == "u10-l1" && e.crc32 == 0xb6cc155f) foundSecond = true;
    }
    check(foundFirst, "first entry's name and CRC32 both read correctly");
    check(foundSecond, "second entry's name and CRC32 both read correctly");

    const std::vector<std::string> names = Archive::entries(path);
    check(names.size() == 2 && names[0] == "ki-l15d.u98" && names[1] == "u10-l1",
          "entries() matches list() minus the CRCs");

    check(Archive::list("/tmp/mister_gui_archive_test_missing.zip").empty(),
          "a missing file yields an empty list, not a crash");

    if (failures) {
        std::printf("\n%d FAILURE(S)\n", failures);
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}
