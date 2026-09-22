// Host-side checks for the game database and the catalogue rules.
//
// The database is state on disk that survives restarts, so the things worth pinning down are
// the ones that only show up later: that what was written reads back identically, that paths
// stay relative to the right root, that a database from an older layout is refused rather
// than misread, and that a rescan does not leave systems behind that have since gone.
//
// Build and run:  make -f tests/Makefile

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "../src/CoreIndex.h"
#include "../src/GameDatabase.h"
#include "../src/Library.h"
#include "../src/SystemCatalog.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

std::string scratch() {
    const char *base = std::getenv("TMPDIR");
    std::string dir = std::string(base ? base : "/tmp") + "/mister-pat-gamesdb-test";
    std::string cmd = "rm -rf '" + dir + "' && mkdir -p '" + dir + "'";
    if (std::system(cmd.c_str()) != 0) std::printf("  (could not prepare %s)\n", dir.c_str());
    return dir;
}

// `root` matters: the database recognises a drive again by the directory of its largest
// system, so that directory has to actually sit under the root being written.
DatabaseSystem makeSystem(const std::string &root, const std::string &key,
                          const std::string &name, bool disc) {
    DatabaseSystem system;
    system.key = key;
    system.name = name;
    system.group = "Consoles";
    system.core = "_Console/" + key;
    system.dir = root + "/games/" + key;
    system.discBased = disc;
    return system;
}

bool fileExists(const std::string &path) {
    std::ifstream in(path);
    return bool(in);
}

} // namespace

int main() {
    const std::string dir = scratch();

    // Two scratch volumes standing in for a card and a drive. They have to exist: a root is
    // only accepted if the library it recorded can still be found on it.
    const std::string card = dir + "-card";
    const std::string drive = dir + "-drive";
    std::system(("rm -rf '" + card + "' '" + drive + "' && mkdir -p '" + card +
                 "/games/NES' '" + drive + "/games/PSX'").c_str());
    const std::vector<std::string> roots = {card, drive};

    // --- round trip -------------------------------------------------------------------
    {
        GameDatabase db(dir);
        db.setMountPoints(roots);
        check(!db.exists(), "an empty directory holds no database");

        check(db.beginWrite(roots), "beginWrite succeeds");
        db.writeSystem(makeSystem(drive, "PSX", "PlayStation", true),
                       {drive + "/games/PSX/Wipeout", drive + "/games/PSX/Tekken 3"});
        db.writeSystem(makeSystem(card, "NES", "NES", false), {card + "/games/NES/Zelda.nes"});
        check(db.finishWrite(), "finishWrite succeeds");
    }

    {
        GameDatabase db(dir);
        db.setMountPoints(roots);
        check(db.exists(), "the written database is recognised");
        check(db.load(), "the written database loads");
        check(db.systems().size() == 2, "both systems come back");
        check(db.totalGames() == 3, "the game count is right");

        const std::vector<std::string> psx = db.pathsFor("PSX");
        check(psx.size() == 2, "a system's list is read on demand");
        check(psx.size() == 2 && psx[0] == drive + "/games/PSX/Wipeout",
              "absolute paths are reconstructed from the root");

        const std::vector<std::string> nes = db.pathsFor("NES");
        check(nes.size() == 1 && nes[0] == card + "/games/NES/Zelda.nes",
              "a second root resolves to its own mount point");

        check(db.pathsFor("SATURN").empty(), "an unknown system yields nothing");

        bool discFlag = false;
        for (const DatabaseSystem &s : db.systems())
            if (s.key == "PSX") discFlag = s.discBased;
        check(discFlag, "the disc-based flag survives a round trip");
    }

    // --- one file per system, not one big one -----------------------------------------
    check(fileExists(dir + "/PSX.tsv") && fileExists(dir + "/NES.tsv"),
          "each system has its own file");
    check(fileExists(dir + "/catalog.tsv") && fileExists(dir + "/roots.tsv"),
          "catalogue and roots are separate");

    // --- a rescan must not leave orphans ----------------------------------------------
    {
        GameDatabase db(dir);
        db.setMountPoints(roots);
        check(db.beginWrite(roots), "rescan starts");
        db.writeSystem(makeSystem(card, "NES", "NES", false), {card + "/games/NES/Zelda.nes"});
        check(db.finishWrite(), "rescan finishes");
    }
    check(!fileExists(dir + "/PSX.tsv"), "a system that disappeared loses its file");
    {
        GameDatabase db(dir);
        db.setMountPoints(roots);
        db.load();
        check(db.systems().size() == 1, "and is gone from the catalogue");
    }

    // --- refuse a database from another layout ----------------------------------------
    {
        std::ofstream out(dir + "/catalog.tsv", std::ios::trunc);
        out << "#mister-pat gamesdb 99\n";
    }
    {
        GameDatabase db(dir);
        check(!db.exists(), "a future version is not claimed as usable");
        check(!db.load(), "and is refused rather than misread");
    }

    // --- catalogue rules ---------------------------------------------------------------
    check(SystemCatalog::displayName("TGFX16") == "TurboGrafx-16", "a cryptic name is mapped");
    check(SystemCatalog::displayName("Amiga") == "Amiga", "a clear name is left alone");

    const std::vector<std::string> nesExt = SystemCatalog::extensionsFor("NES");
    check(!nesExt.empty(), "a known system has extensions");
    check(SystemCatalog::extensionsFor("SomeNewMachine").empty(),
          "an unknown system has none");

    check(SystemCatalog::looksLikeGame("Zelda.nes", nesExt), "a matching extension is a game");
    check(!SystemCatalog::looksLikeGame("Zelda.png", nesExt), "box art is not a game");
    check(SystemCatalog::looksLikeGame("Zelda.zip", nesExt), "a zip is a game everywhere");
    check(!SystemCatalog::looksLikeGame("readme", nesExt), "a file without extension is not");

    const std::vector<std::string> none;
    check(SystemCatalog::looksLikeGame("Whatever.xyz", none),
          "an unknown system keeps unknown files");
    check(!SystemCatalog::looksLikeGame("notes.txt", none),
          "an unknown system still drops obvious non-games");

    check(SystemCatalog::isIgnoredDirectory("media"), "the artwork folder is skipped");
    check(SystemCatalog::isIgnoredDirectory(".hidden"), "hidden folders are skipped");
    check(!SystemCatalog::isIgnoredDirectory("Wipeout"), "a game folder is not skipped");

    const std::vector<std::string> parents = SystemCatalog::gameParents("/media/usb0");
    check(parents.size() == 2 && parents[0] == "/media/usb0/games",
          "both library layouts are searched");

    // --- display names ------------------------------------------------------------------
    // Folder-based disc games carry dots in their titles. Cutting at the last dot turned
    // "Capcom vs. SNK - Millennium Fight 2000 Pro" into "Capcom vs" — wrong on screen, and
    // it looked for box art under that name too.
    {
        GameSystem disc;
        disc.discBased = true;
        GameSystem cart;

        const Game a = Library::makeGame(disc, "/games/PSX/Capcom vs. SNK - Millennium Fight");
        check(a.name == "Capcom vs. SNK - Millennium Fight", "a disc folder keeps its dots");

        const Game b = Library::makeGame(disc, "/games/PSX/Bio F.R.E.A.K.S");
        check(b.name == "Bio F.R.E.A.K.S", "initials survive in a folder name");

        const Game c = Library::makeGame(disc, "/games/PSX/Wipeout.chd");
        check(c.name == "Wipeout", "a single disc image still loses its extension");

        const Game d = Library::makeGame(cart, "/games/NES/Bio F.R.E.A.K.S.nes");
        check(d.name == "Bio F.R.E.A.K.S", "a cartridge file loses only its extension");

        const Game e = Library::makeGame(disc, "/games/PSX/Wipeout");
        check(e.boxart == "/games/PSX/media/Wipeout.png", "box art follows the display name");
        check(e.background == "/games/PSX/media/Wipeout-BG.png", "so does the background");
    }

    // --- artwork the scraper actually wrote is found, not just the PNG default -----------
    // The scraper writes .jpg — a third the size of a PNG, the entire reason for scraping our
    // own — but the path Game::boxart builds has to match what is really on disk, or the
    // lookup just misses. This is exactly what happened once: 641 covers were fetched,
    // written correctly, and never shown, because the path built here still said ".png".
    {
        const std::string media = dir + "-vol-a/games/PSX/media";
        std::system(("rm -rf '" + dir + "-vol-a' && mkdir -p '" + media + "'").c_str());
        std::ofstream(media + "/Wipeout.jpg").put('x');

        GameSystem disc;
        disc.discBased = true;

        const Game withJpeg = Library::makeGame(disc, dir + "-vol-a/games/PSX/Wipeout");
        check(withJpeg.boxart == media + "/Wipeout.jpg",
              "a scraped .jpg cover is found ahead of the unwritten .png default");

        // A cover placed by Console Mode, or by hand, is still a plain .png with nothing
        // beside it — that has to keep working exactly as before.
        const Game pngOnly = Library::makeGame(disc, dir + "-vol-a/games/PSX/Tekken 3");
        check(pngOnly.boxart == media + "/Tekken 3.png",
              "a system with no .jpg falls back to .png as before");
    }

    // --- a drive that comes back at a different mount point -----------------------------
    // MiSTer numbers USB volumes in the order they appear, so the same drive can be usb0 one
    // boot and usb1 the next. Relative paths alone do not survive that: the recorded root
    // still names the old place. The probe directory is what lets it be found again.
    {
        const std::string a = dir + "-vol-a";
        const std::string b = dir + "-vol-b";
        std::system(("rm -rf '" + a + "' '" + b + "' && mkdir -p '" + a + "/games/PSX'").c_str());

        {
            GameDatabase db(dir);
            db.setMountPoints({a, b});
            check(db.beginWrite({a}), "scan of a drive at its first mount point");
            db.writeSystem(makeSystem(a, "PSX", "PlayStation", true), {a + "/games/PSX/Wipeout"});
            check(db.finishWrite(), "written");
        }

        {
            GameDatabase db(dir);
            db.setMountPoints({a, b});
            db.load();
            check(!db.rootsMoved(), "an unmoved drive is not reported as moved");
            const std::vector<std::string> p = db.pathsFor("PSX");
            check(p.size() == 1 && p[0] == a + "/games/PSX/Wipeout", "paths resolve as scanned");
        }

        // The drive reappears elsewhere.
        std::system(("rm -rf '" + a + "' && mkdir -p '" + b + "/games/PSX'").c_str());
        {
            GameDatabase db(dir);
            db.setMountPoints({a, b});
            db.load();
            check(db.rootsMoved(), "a moved drive is recognised");
            check(db.missingRoots().empty(), "and is not reported missing");
            const std::vector<std::string> p = db.pathsFor("PSX");
            check(p.size() == 1 && p[0] == b + "/games/PSX/Wipeout",
                  "paths follow the drive to its new mount point");
        }

        // The drive is not attached at all.
        std::system(("rm -rf '" + b + "'").c_str());
        {
            GameDatabase db(dir);
            db.setMountPoints({a, b});
            db.load();
            check(db.missingRoots().size() == 1, "an absent drive is reported, not guessed at");
            check(db.pathsFor("PSX").empty(), "and yields no unreachable paths");
        }
    }

    // --- a cancelled rebuild must not destroy a working database -------------------------
    {
        const std::string a = dir + "-vol-a";
        std::system(("rm -rf '" + dir + "' '" + a + "' && mkdir -p '" + a + "/games/NES'").c_str());

        {
            GameDatabase db(dir);
            db.setMountPoints({a});
            db.beginWrite({a});
            db.writeSystem(makeSystem(a, "NES", "NES", false), {a + "/games/NES/Zelda.nes"});
            db.finishWrite();
        }

        // Start a rebuild and walk away, as cancelling from the wizard does.
        {
            GameDatabase db(dir);
            db.setMountPoints({a});
            db.beginWrite({a});
            db.writeSystem(makeSystem(a, "PSX", "PlayStation", true), {a + "/games/PSX/X"});
        }

        GameDatabase db(dir);
        db.setMountPoints({a});
        check(db.exists(), "the old database is still valid after a cancelled rebuild");
        check(db.load() && db.systems().size() == 1, "and still holds what it held");
        check(db.pathsFor("NES").size() == 1, "including its game lists");
        check(db.pathsFor("PSX").empty(), "the abandoned half is not visible");
    }

    // --- paths that would corrupt the format ---------------------------------------------
    {
        const std::string a = dir + "-vol-a";
        GameDatabase db(dir);
        db.setMountPoints({a});
        db.beginWrite({a});
        db.writeSystem(makeSystem(a, "NES", "NES", false),
                       {a + "/games/NES/good.nes", a + "/games/NES/bad\tname.nes"});
        db.finishWrite();
        check(db.pathsFor("NES").size() == 1, "a path containing a tab is dropped, not stored");
    }

    // --- the real library must not lose to a stray leftover of the same name ------------
    // The SD card can happen to have its own small "games/PSX" folder — a leftover test, a
    // homebrew demo — while the real library sits on the USB drive. Discovering the SD card
    // first must not let its stub win just because it was seen first: the found 641-game
    // PlayStation library on a real device once lost to a 3-entry leftover this way.
    {
        const std::string small = dir + "-small";
        const std::string large = dir + "-large";
        std::system(("rm -rf '" + small + "' '" + large + "' && mkdir -p '" + small +
                     "/_Console' '" + small + "/games/PSX' '" + large + "/games/PSX'")
                        .c_str());
        std::ofstream(small + "/_Console/PSX.rbf").put('x');

        for (int i = 0; i < 2; ++i)
            std::system(
                ("mkdir -p '" + small + "/games/PSX/g" + std::to_string(i) + "'").c_str());
        for (int i = 0; i < 5; ++i)
            std::system(
                ("mkdir -p '" + large + "/games/PSX/g" + std::to_string(i) + "'").c_str());

        CoreIndex cores;
        cores.scan({small, large});

        // Small root first — the exact order that used to make the stub win.
        const std::vector<CatalogEntry> found = SystemCatalog::discover({small, large}, cores);

        const CatalogEntry *psx = nullptr;
        for (const CatalogEntry &e : found)
            if (e.key == "PSX") psx = &e;

        check(psx != nullptr, "PSX is found at all");
        check(psx && psx->dir == large + "/games/PSX",
              "the larger library wins over an earlier, smaller one of the same name");
    }

    std::printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
    return failures ? 1 : 0;
}
