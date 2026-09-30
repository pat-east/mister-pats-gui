// Host-side check that LibraryScan records Arcade in the game database the way ARCADE.md plans
// it: the games that actually work (core, ROM zip and CRCs all present — judged by the same
// check the diagnostic table uses), each once however many volumes hold it, sorted by name —
// plus one list per manufacturer and per category, with a catalogue naming them.
//
// Build and run:  make -C tests

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "../src/GameDatabase.h"
#include "../src/LibraryScan.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

void touch(const std::string &path) {
    std::system(("mkdir -p \"$(dirname '" + path + "')\" && : > '" + path + "'").c_str());
}

void write(const std::string &path, const std::string &content) {
    std::system(("mkdir -p \"$(dirname '" + path + "')\"").c_str());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

void u16(std::string &b, unsigned v) { b += char(v & 0xFF); b += char((v >> 8) & 0xFF); }
void u32(std::string &b, unsigned v) { u16(b, v & 0xFFFF); u16(b, v >> 16); }

// Central directory plus end record only — all Archive::list() ever reads (see mra_file_test).
std::string zipWith(const std::string &name, unsigned crc) {
    std::string central("PK\x01\x02", 4);
    for (int i = 0; i < 6; ++i) u16(central, 0);
    u32(central, crc);
    u32(central, 0);
    u32(central, 0);
    u16(central, unsigned(name.size()));
    for (int i = 0; i < 4; ++i) u16(central, 0);
    u32(central, 0);
    u32(central, 0);
    central += name;

    std::string end("PK\x05\x06", 4);
    u16(end, 0); u16(end, 0); u16(end, 1); u16(end, 1);
    u32(end, unsigned(central.size()));
    u32(end, 0);
    u16(end, 0);
    return central + end;
}

std::string mra(const std::string &rbf, const std::string &zip,
                const std::string &manufacturer = "", const std::string &category = "") {
    std::string out = "<misterromdescription><name>x</name>";
    if (!manufacturer.empty()) out += "<manufacturer>" + manufacturer + "</manufacturer>";
    if (!category.empty()) out += "<category>" + category + "</category>";
    return out + "<rbf>" + rbf + "</rbf><rom index=\"1\" zip=\"" + zip +
           "\"><part name=\"a\" crc=\"11111111\"/></rom></misterromdescription>";
}

bool endsWith(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool any(const std::vector<std::string> &paths, const std::string &suffix) {
    for (const std::string &p : paths)
        if (endsWith(p, suffix)) return true;
    return false;
}

const DatabaseGroup *find(const std::vector<DatabaseGroup> &groups, const std::string &name) {
    for (const DatabaseGroup &g : groups)
        if (g.name == name) return &g;
    return nullptr;
}

} // namespace

int main() {
    const std::string base = "/tmp/mister-gui-arcade-test-" + std::to_string(getpid());
    const std::string card = base + "-card";
    const std::string drive = base + "-drive";
    const std::string dir = base + "-db";
    const std::string clean = "rm -rf '" + card + "' '" + drive + "' '" + dir + "' '" + dir +
                              ".new' '" + dir + ".old'";
    std::system(clean.c_str());

    // A card with one ordinary system (a scan refuses to run with none) and an arcade tree.
    touch(card + "/_Console/NES_20200101.rbf");
    touch(card + "/games/NES/Zelda.nes");
    touch(card + "/_Arcade/cores/Arcade-Core.rbf");
    write(card + "/games/mame/good.zip", zipWith("a", 0x11111111));
    write(card + "/games/mame/badcrc.zip", zipWith("a", 0xDEADBEEF));

    write(card + "/_Arcade/Zaxxon.mra", mra("Core", "good.zip", "Sega", "Shooter"));
    write(card + "/_Arcade/Fighters/Killer Instinct.mra",
          mra("Core", "good.zip", "Rare / Nintendo", "Fighter"));
    write(card + "/_Arcade/Nameless.mra", mra("Core", "good.zip"));
    // The same category spelled two ways is one category.
    write(card + "/_Arcade/Brawler A.mra", mra("Core", "good.zip", "Capcom", "Beat 'Em Up"));
    write(card + "/_Arcade/Brawler B.mra", mra("Core", "good.zip", "CAPCOM", "Beat 'em  Up"));
    write(card + "/_Arcade/No ROM.mra", mra("Core", "missing.zip", "Sega", "Shooter"));
    write(card + "/_Arcade/Bad CRC.mra", mra("Core", "badcrc.zip", "Sega", "Shooter"));
    write(card + "/_Arcade/No Core.mra", mra("Absent", "good.zip", "Sega", "Shooter"));
    // Valid games, but in folders that are not part of the library proper.
    write(card + "/_Arcade/_Organized/Capcom/Organized Only.mra", mra("Core", "good.zip"));
    write(card + "/_Arcade/media/Media Only.mra", mra("Core", "good.zip"));
    write(card + "/_Arcade/cores/Cores Only.mra", mra("Core", "good.zip"));
    touch(card + "/_Arcade/readme.txt");

    // A second volume holding a copy of one game and one of its own. Its core is looked up on
    // that volume alone, so this one needs a core there to count.
    touch(drive + "/_Arcade/cores/Arcade-Core.rbf");
    write(drive + "/_Arcade/Zaxxon.mra", mra("Core", "good.zip", "Sega", "Shooter"));
    write(drive + "/_Arcade/Bubble Bobble.mra", mra("Core", "good.zip", "Taito", "Platform"));

    LibraryScan scan(dir);
    scan.setRomSearchOrder({card, drive});
    scan.start({card, drive});
    for (int guard = 0; !scan.finished() && guard < 10000; ++guard) scan.step();

    check(scan.state() == LibraryScan::State::Done, "the scan finishes");
    check(scan.systemsFound() == 2, "NES and Arcade are both recorded");
    check(scan.gamesFound() == 7, "one NES game plus six working .mra files");

    GameDatabase db(dir);
    db.setMountPoints({card, drive});
    check(db.load(), "the database loads");

    const DatabaseSystem *arcade = nullptr;
    for (const DatabaseSystem &s : db.systems())
        if (s.key == "Arcade") arcade = &s;
    check(arcade != nullptr, "Arcade is a catalogue entry");
    check(arcade && arcade->group == "Arcade", "in its own group");
    check(arcade && arcade->core.empty(), "with no single core");
    check(arcade && arcade->count == 6, "counting only games that work, each only once");

    const std::vector<std::string> paths = db.pathsFor("Arcade");
    check(paths.size() == 6, "six paths read back");
    check(any(paths, "/_Arcade/Bubble Bobble.mra"), "a game on the second volume is found");
    check(any(paths, "/_Arcade/Fighters/Killer Instinct.mra"), "a game in a subfolder is found");
    check(any(paths, "/_Arcade/Zaxxon.mra"), "a game on both volumes is there");
    check(any(paths, "/_Arcade/Nameless.mra"), "a game with no metadata is there too");
    check(!any(paths, "No ROM.mra"), "a game whose ROM zip is missing is left out");
    check(!any(paths, "Bad CRC.mra"), "a game whose ROM fails its CRC is left out");
    check(!any(paths, "No Core.mra"), "a game whose core is not installed is left out");
    check(!any(paths, "Organized Only.mra") && !any(paths, "Media Only.mra") &&
              !any(paths, "Cores Only.mra"),
          "cores, media and _Organized are not listed");
    check(paths.size() == 6 && paths[0].find("Brawler A") != std::string::npos,
          "sorted by name");

    // --- the groups --------------------------------------------------------------------
    const std::vector<DatabaseGroup> makers = db.groupsFor(GameDatabase::kArcadeManufacturers);
    const std::vector<DatabaseGroup> categories = db.groupsFor(GameDatabase::kArcadeCategories);
    check(makers.size() == 5, "five manufacturers, counting the unknown one");
    check(categories.size() == 5, "five categories, counting the uncategorized one");

    const DatabaseGroup *sega = find(makers, "Sega");
    check(sega && sega->count == 1, "Sega has its one working game, the broken ones uncounted");
    const DatabaseGroup *rare = find(makers, "Rare / Nintendo");
    check(rare && rare->key.find('/') == std::string::npos && rare->key.find(' ') == std::string::npos,
          "a manufacturer with a slash gets a filename-safe key");
    check(find(makers, "(Unknown)") && find(makers, "(Unknown)")->count == 1,
          "a game with no manufacturer lands in (Unknown)");
    check(find(categories, "(Uncategorized)") != nullptr, "and in (Uncategorized) likewise");

    const std::vector<std::string> sega1 = sega ? db.pathsFor(sega->key) : std::vector<std::string>();
    check(sega1.size() == 1 && endsWith(sega1[0], "/_Arcade/Zaxxon.mra"),
          "a group's list reads back with the ordinary pathsFor()");
    const std::vector<std::string> fighters =
        find(categories, "Fighter") ? db.pathsFor(find(categories, "Fighter")->key)
                                    : std::vector<std::string>();
    check(fighters.size() == 1 && endsWith(fighters[0], "Killer Instinct.mra"),
          "a category's list too");
    check(db.systems().size() == 2, "groups are not catalogue systems");

    const DatabaseGroup *brawl = find(categories, "Beat 'Em Up");
    check(brawl && brawl->count == 2, "two spellings of one category are one group");
    const DatabaseGroup *capcom = find(makers, "Capcom");
    check(capcom && capcom->count == 2 && !find(makers, "CAPCOM"),
          "and likewise a manufacturer, shown under the first spelling");

    // --- a machine with only arcade games ------------------------------------------------
    {
        std::system(clean.c_str());
        touch(card + "/_Arcade/cores/Arcade-Core.rbf");
        write(card + "/games/mame/good.zip", zipWith("a", 0x11111111));
        write(card + "/_Arcade/Zaxxon.mra", mra("Core", "good.zip", "Sega", "Shooter"));

        LibraryScan only(dir);
        only.setRomSearchOrder({card});
        only.start({card});
        for (int guard = 0; !only.finished() && guard < 10000; ++guard) only.step();
        check(only.state() == LibraryScan::State::Done,
              "a machine with only arcade games can still be scanned");
        check(only.gamesFound() == 1, "and finds its game");
    }

    std::system(clean.c_str());
    std::printf("\n%s\n", failures ? "FAILURES" : "all good");
    return failures ? 1 : 0;
}
