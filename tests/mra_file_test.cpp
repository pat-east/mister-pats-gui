// Host-side checks for MraFile: parsing an .mra, and the two lookups it replicates from
// MiSTer's own arcade loader — see ARCADE.md, "Two independent root lookups", and its Killer
// Instinct case study, which is exactly the bug the resolveRom() checks below exist to catch
// before it ever reaches a real device again.
//
// Build and run:  make -f tests/Makefile

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "../src/Archive.h"
#include "../src/MraFile.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

void writeFile(const std::string &path, const std::string &content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

void makeDirs(const std::string &path) {
    std::string cmd = "mkdir -p '" + path + "'";
    system(cmd.c_str());
}

void removeAll(const std::string &path) {
    std::string cmd = "rm -rf '" + path + "'";
    system(cmd.c_str());
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

// Central directory plus end record only — Archive::list() never reads local headers or
// compressed data, so that is all a fixture needs. See archive_test.cpp for the same approach.
std::string buildFakeZip(const std::vector<ArchiveEntry> &entries) {
    std::string central;
    for (const ArchiveEntry &e : entries) {
        central += std::string("PK\x01\x02", 4);
        writeU16(central, 0);
        writeU16(central, 0);
        writeU16(central, 0);
        writeU16(central, 0);
        writeU16(central, 0);
        writeU16(central, 0);
        writeU32(central, e.crc32);
        writeU32(central, 0);
        writeU32(central, 0);
        writeU16(central, uint16_t(e.name.size()));
        writeU16(central, 0);
        writeU16(central, 0);
        writeU16(central, 0);
        writeU16(central, 0);
        writeU32(central, 0);
        writeU32(central, 0);
        central += e.name;
    }

    std::string eocd;
    eocd += std::string("PK\x05\x06", 4);
    writeU16(eocd, 0);
    writeU16(eocd, 0);
    writeU16(eocd, uint16_t(entries.size()));
    writeU16(eocd, uint16_t(entries.size()));
    writeU32(eocd, uint32_t(central.size()));
    writeU32(eocd, 0);
    writeU16(eocd, 0);

    return central + eocd;
}

const char *kSampleMra = R"MRA(<misterromdescription>
  <name>Test Game (v1.0)</name>
  <year>1994</year>
  <manufacturer>Test &amp; Co</manufacturer>
  <category>Fighter</category>
  <rbf>TestCore</rbf>

  <!-- Game selector: 00 = Test Game -->
  <rom index="0">
    <part>00</part>
  </rom>

  <!-- Combined ROM image -->
  <rom index="1" zip="test.zip">
    <part name="prog.u1" crc="11111111"/>
    <part name="prog.u2" crc="22222222"/>
  </rom>
</misterromdescription>
)MRA";

// Confirmed on real hardware against a real arcade library: a large share of real-world .mra
// files use <description> instead of <name>, and nest their named parts inside <interleave>
// rather than as a <rom> block's direct children — both taken verbatim from the shape of a
// real ST-V title's .mra (Decathlete), with the values changed. A repeat-filled part with no
// name attribute at all (the very first <rom> block below) is real too, and must be skipped
// rather than counted as a part to check.
const char *kDescriptionOnlyMra = R"MRA(<misterromdescription>
  <rbf>TestCore2</rbf>
  <setname>test2</setname>
  <description>Test Game 2 (JUET 960709 V1.001)</description>
  <year>1996</year>
  <manufacturer>Sega</manufacturer>

  <rom index="1" zip="test2.zip" address="0x32000000">
    <part repeat="0x80">00</part>
  </rom>
  <rom index="2" zip="bios2.zip" address="0x30000000">
    <interleave output="64">
      <part crc="d1be2adf" name="epr-1.ic8" map="21436587" />
    </interleave>
    <interleave output="64">
      <part crc="f688ae60" name="epr-2.ic8" map="21436587" />
    </interleave>
  </rom>
</misterromdescription>
)MRA";

// A third real convention, confirmed against a real library: a bare <n> tag, seen for
// "Vanguard" and "Elevator Action" among others. Deliberately includes an <nvram> sibling tag
// to prove the boundary check that keeps a search for "n" from matching it (or "<name") by
// accident.
const char *kShortNameTagMra = R"MRA(<misterromdescription>
  <n>Test Game 3</n>
  <nvram index="3" size="256"/>
  <rbf>TestCore3</rbf>
</misterromdescription>
)MRA";

} // namespace

int main() {
    const std::string root = "/tmp/mister_gui_mra_test_root";
    removeAll(root);

    // --- parse() -------------------------------------------------------------------------
    makeDirs(root + "/_Arcade");
    const std::string mraPath = root + "/_Arcade/Test Game (v1.0).mra";
    writeFile(mraPath, kSampleMra);

    ArcadeEntry entry;
    const bool parsed = MraFile::parse(mraPath, entry);
    check(parsed, "parse() reads the file");
    check(entry.root == root, "the volume is derived from the path, stopping before /_Arcade");
    check(entry.name == "Test Game (v1.0)", "name");
    check(entry.year == "1994", "year");
    check(entry.manufacturer == "Test & Co", "manufacturer, with &amp; decoded");
    check(entry.category == "Fighter", "category");
    check(entry.rbf == "TestCore", "rbf");
    check(entry.romBlocks.size() == 1,
          "the index=0 game-selector block is not kept - it names no zip");
    if (entry.romBlocks.size() == 1) {
        const ArcadeRomBlock &block = entry.romBlocks[0];
        check(block.zip == "test.zip", "the real rom block's zip attribute");
        check(block.parts.size() == 2, "both parts of the real rom block");
        if (block.parts.size() == 2) {
            check(block.parts[0].name == "prog.u1" && block.parts[0].crc == 0x11111111,
                  "first part name and CRC");
            check(block.parts[1].name == "prog.u2" && block.parts[1].crc == 0x22222222,
                  "second part name and CRC");
        }
    }

    check(!MraFile::parse(root + "/_Arcade/does-not-exist.mra", entry),
          "parsing a missing file fails cleanly rather than crashing");

    // --- parse(), <description>-only real-world shape --------------------------------------
    const std::string descMraPath = root + "/_Arcade/Test Game 2.mra";
    writeFile(descMraPath, kDescriptionOnlyMra);

    ArcadeEntry descEntry;
    check(MraFile::parse(descMraPath, descEntry), "parse() reads a <description>-only file");
    check(descEntry.name == "Test Game 2 (JUET 960709 V1.001)",
          "falls back to <description> when <name> is absent");
    check(descEntry.romBlocks.size() == 2,
          "both zip-backed rom blocks kept, including the repeat-filled one");
    for (const ArcadeRomBlock &block : descEntry.romBlocks) {
        if (block.zip == "test2.zip") {
            check(block.parts.empty(),
                  "a repeat-filled part with no name attribute is not a part to check");
        } else if (block.zip == "bios2.zip") {
            check(block.parts.size() == 2,
                  "parts nested inside <interleave> are still found");
            if (block.parts.size() == 2) {
                check(block.parts[0].name == "epr-1.ic8" && block.parts[0].crc == 0xd1be2adf,
                      "first interleaved part");
                check(block.parts[1].name == "epr-2.ic8" && block.parts[1].crc == 0xf688ae60,
                      "second interleaved part");
            }
        } else {
            check(false, "unexpected rom block zip");
        }
    }

    // --- parse(), <n>-only real-world shape -------------------------------------------------
    const std::string nMraPath = root + "/_Arcade/Test Game 3.mra";
    writeFile(nMraPath, kShortNameTagMra);

    ArcadeEntry nEntry;
    check(MraFile::parse(nMraPath, nEntry), "parse() reads an <n>-only file");
    check(nEntry.name == "Test Game 3",
          "falls back to <n> when neither <name> nor <description> is present");

    // --- resolveCore() ---------------------------------------------------------------------
    makeDirs(root + "/_Arcade/cores");
    writeFile(root + "/_Arcade/cores/Arcade-TestCore.rbf", "old");
    writeFile(root + "/_Arcade/cores/Arcade-TestCore_20260101.rbf", "new");
    // Shares the same prefix but is a different core entirely — the boundary check (next
    // character must be '.' or '_') is what has to keep this from matching "TestCore".
    writeFile(root + "/_Arcade/cores/Arcade-TestCoreExtra.rbf", "unrelated");

    ArcadeEntry core;
    core.root = root;
    core.rbf = "TestCore";
    MraFile::resolveCore(core);
    check(core.coreFound, "a core is found");
    check(core.coreFile == "Arcade-TestCore_20260101.rbf",
          "the dated release wins over the undated copy, matching MiSTer's own get_rbf");

    // Most real cores carry no "Arcade-" prefix: Alpha68k_20221223.rbf, not Arcade-Alpha68k_….
    writeFile(root + "/_Arcade/cores/Bare_20240101.rbf", "");
    writeFile(root + "/_Arcade/cores/Bare_20260101.rbf", "");
    writeFile(root + "/_Arcade/cores/Bare2_20990101.rbf", "");
    ArcadeEntry bare;
    bare.root = root;
    bare.rbf = "Bare";
    MraFile::resolveCore(bare);
    check(bare.coreFound && bare.coreFile == "Bare_20260101.rbf",
          "a core without the Arcade- prefix is found, the newest dated one winning");

    ArcadeEntry noCore;
    noCore.root = root;
    noCore.rbf = "NoSuchCore";
    MraFile::resolveCore(noCore);
    check(!noCore.coreFound, "a core that is not installed is reported as not found");

    // --- resolveRom() ------------------------------------------------------------------------
    makeDirs(root + "/games/mame");
    writeFile(root + "/games/mame/test.zip",
             buildFakeZip({{"prog.u1", 0x11111111}, {"prog.u2", 0x22222222}}));

    ArcadeEntry good;
    good.root = root;
    good.romBlocks.push_back({1, "test.zip", {{"prog.u1", 0x11111111, true, ""},
                                              {"prog.u2", 0x22222222, true, ""}}});
    MraFile::resolveRom(good, {root});
    check(good.romFound, "the zip is found under games/mame");
    check(good.romCrcOk, "every part's CRC matches");
    check(good.romName == "test.zip", "the rom name column names the zip the .mra asked for");

    ArcadeEntry badCrc;
    badCrc.root = root;
    badCrc.romBlocks.push_back({1, "test.zip", {{"prog.u1", 0xDEADBEEF, true, ""}}});
    MraFile::resolveRom(badCrc, {root});
    check(badCrc.romFound, "the zip itself is still found");
    check(!badCrc.romCrcOk, "a wrong CRC is caught, not silently accepted");

    ArcadeEntry missingZip;
    missingZip.root = root;
    missingZip.romBlocks.push_back({1, "nosuch.zip", {{"prog.u1", 0x11111111, true, ""}}});
    MraFile::resolveRom(missingZip, {root});
    check(!missingZip.romFound, "a zip that is not there at all is reported missing");
    check(!missingZip.romCrcOk, "and its CRCs cannot have been confirmed either");
    check(missingZip.romName == "nosuch.zip",
          "the rom name column stays informative even when nothing was found");

    ArcadeEntry noBlocks;
    noBlocks.root = root;
    MraFile::resolveRom(noBlocks, {root});
    check(!noBlocks.romFound && !noBlocks.romCrcOk,
          ".mra with no zip-backed rom block claims neither found nor missing");

    // --- misterGameSearchOrder() ------------------------------------------------------------
    const std::vector<std::string> ordered = MraFile::misterGameSearchOrder(
        {"/media/fat", "/media/usb2", "/media/usb0", "/media/usb7"});
    const std::vector<std::string> expected = {"/media/usb0", "/media/usb2", "/media/fat"};
    check(ordered == expected,
          "USB volumes first (ascending, usb0..usb5 only), the SD card last");

    removeAll(root);

    if (failures) {
        std::printf("\n%d FAILURE(S)\n", failures);
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}
