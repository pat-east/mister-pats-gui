#include "SystemCatalog.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "CoreIndex.h"

namespace {

// Only where the directory name is cryptic or differs from what people call the machine.
// Everything not listed keeps its directory name, which already reads fine ("Amiga", "SNES").
struct Alias {
    const char *dir;
    const char *name;
};

const Alias kNames[] = {
    {"PSX", "PlayStation"},          {"SMS", "Master System"},
    {"S32X", "Genesis 32X"},         {"TGFX16", "TurboGrafx-16"},
    {"TGFX16-CD", "TurboGrafx-16 CD"}, {"NEOGEO", "Neo Geo"},
    {"NeoGeo-CD", "Neo Geo CD"},     {"NGPC", "Neo Geo Pocket Color"},
    {"GBA2P", "Game Boy Advance (2 Player)"},
    {"GAMEBOY2P", "Game Boy (2 Player)"},
    {"GAMEBOY", "Game Boy"},         {"GBC", "Game Boy Color"},
    {"GBA", "Game Boy Advance"},     {"SGB", "Super Game Boy"},
    {"MegaDrive", "Mega Drive"},     {"MegaCD", "Mega CD"},
    {"MegaDuck", "Mega Duck"},       {"GameGear", "Game Gear"},
    {"GameGear2P", "Game Gear (2 Player)"},
    {"AtariLynx", "Atari Lynx"},     {"Atari2600", "Atari 2600"},
    {"ATARI5200", "Atari 5200"},     {"ATARI7800", "Atari 7800"},
    {"ATARI800", "Atari 800"},       {"AtariST", "Atari ST"},
    {"ODYSSEY2", "Magnavox Odyssey 2"}, {"VECTREX", "Vectrex"},
    {"VirtualBoy", "Virtual Boy"},   {"WonderSwan", "WonderSwan"},
    {"WonderSwanColor", "WonderSwan Color"},
    {"PokemonMini", "Pokemon Mini"}, {"NeoGeoPocket", "Neo Geo Pocket"},
    {"NeoGeoPocket-Color", "Neo Geo Pocket Color"},
    {"ChannelF", "Channel F"},       {"Coleco", "ColecoVision"},
    {"Astrocade", "Bally Astrocade"},{"CreatiVision", "VTech CreatiVision"},
    {"Intellivision", "Intellivision"}, {"CD-i", "CD-i"},
    {"N64", "Nintendo 64"},          {"NES", "NES"},
    {"snes", "SNES"},                {"SNES", "SNES"},
    {"C64", "Commodore 64"},         {"C128", "Commodore 128"},
    {"C16", "Commodore 16"},         {"VIC20", "Commodore VIC-20"},
    {"PET2001", "Commodore PET"},    {"Spectrum", "ZX Spectrum"},
    {"ZXNext", "ZX Spectrum Next"},  {"ZX81", "ZX81"},
    {"MACPLUS", "Macintosh Plus"},   {"MACLC", "Macintosh LC"},
    {"AO486", "PC (486)"},           {"PCXT", "PC (XT)"},
    {"X68000", "Sharp X68000"},      {"SharpMZ", "Sharp MZ"},
    {"TI-99_4A", "TI-99/4A"},        {"TRS-80", "TRS-80"},
    {"Amstrad PCW", "Amstrad PCW"},  {"Apple-II", "Apple II"},
    {"Apple-IIgs", "Apple IIgs"},    {"APPLE-I", "Apple I"},
    {"BBCMicro", "BBC Micro"},       {"AcornAtom", "Acorn Atom"},
    {"AcornElectron", "Acorn Electron"},
    {"SuperVision", "Super Vision"}, {"SuperAcan", "Super A'Can"},
    {"GameCom", "Game.com"},         {"GameNWatch", "Game & Watch"},
    {"Game and Watch", "Game & Watch"},
    {"PocketChallengeV2", "Pocket Challenge V2"},
    {"PocketStation", "PocketStation"},
    {"Casio_PV-1000", "Casio PV-1000"}, {"Casio_PV-2000", "Casio PV-2000"},
    {"Sord M5", "Sord M5"},          {"VC4000", "VC 4000"},
    {"Studio-II", "Studio II"},      {"Jaguar", "Atari Jaguar"},
    {"Saturn", "Saturn"},            {"3DO", "3DO"},
    {"mame", "MAME"},                {"hbmame", "HBMAME"},
};

// Extensions per system, lower case, no dot. A system not listed here falls back to the
// deny-list in looksLikeGame, which is permissive on purpose: a missing entry should mean
// "show the games anyway", not "show nothing".
struct Extensions {
    const char *dir;
    const char *list;   // comma separated
};

const Extensions kExtensions[] = {
    {"NES", "nes,fds,nsf"},              {"snes", "sfc,smc,bs"},
    {"SNES", "sfc,smc,bs"},              {"N64", "n64,z64,v64,ndd"},
    {"GAMEBOY", "gb"},                   {"GBC", "gbc,gb"},
    {"GBA", "gba"},                      {"SGB", "gb"},
    {"VirtualBoy", "vb"},                {"PokemonMini", "min"},
    {"MegaDrive", "md,gen,bin,smd"},     {"S32X", "32x,bin"},
    {"SMS", "sms,sg,bin"},               {"GameGear", "gg"},
    {"MegaCD", "cue,chd,iso"},           {"Saturn", "cue,chd"},
    {"PSX", "cue,chd,exe,iso"},          {"3DO", "cue,iso,chd"},
    {"CD-i", "cue,chd,iso"},             {"TGFX16", "pce,sgx,bin"},
    {"TGFX16-CD", "cue,chd,iso"},        {"NEOGEO", "neo,zip"},
    {"NeoGeo-CD", "cue,chd,iso"},        {"NeoGeoPocket", "ngp,npc"},
    {"NeoGeoPocket-Color", "ngc,npc"},   {"NGPC", "ngc,npc"},
    {"WonderSwan", "ws"},                {"WonderSwanColor", "wsc"},
    {"AtariLynx", "lnx"},                {"Atari2600", "a26,bin"},
    {"ATARI5200", "a52,car,bin"},        {"ATARI7800", "a78,bin"},
    {"ATARI800", "atr,xex,car,rom,bin"}, {"Jaguar", "j64,jag,rom,bin"},
    {"VECTREX", "vec,bin"},              {"Coleco", "col,bin,sg,rom"},
    {"Intellivision", "int,bin,rom"},    {"ODYSSEY2", "bin,rom"},
    {"ChannelF", "bin,rom"},             {"Astrocade", "bin,rom"},
    {"MegaDuck", "bin"},                 {"Gamate", "bin"},
    {"GameCom", "bin"},                  {"SuperVision", "sv,bin"},
    {"MSX", "rom,dsk,cas,mx1,mx2"},      {"MSX1", "rom,dsk,cas,mx1"},
    {"C64", "prg,crt,d64,t64,tap,g64"},  {"VIC20", "prg,crt,d64,tap"},
    {"C16", "prg,d64,tap"},              {"Amiga", "adf,hdf"},
    {"Spectrum", "tap,tzx,z80,trd,sna"}, {"ZX81", "p,o,80"},
    {"Amstrad", "dsk,cdt,sna"},          {"AtariST", "st,msa,dim"},
    {"X68000", "d88,dim,hdf"},           {"TRS-80", "dsk,cas"},
    {"BBCMicro", "ssd,dsd,uef"},         {"Apple-II", "dsk,do,po,nib"},
    {"mame", "zip"},                     {"hbmame", "zip"},
};

// Never a game, whatever the system.
const char *kIgnoredDirs[] = {
    "media", "artwork", "boxart", "images", "covers", "screenshots", "video", "videos",
    "manuals", "saves", "savestates", "cheats", "config", "System Volume Information",
    "$RECYCLE.BIN", "FOUND.000", "palettes", "docs",
};

// Files that turn up beside games and are plainly not games.
const char *kDeniedExtensions[] = {
    "png", "jpg", "jpeg", "gif", "bmp", "txt", "dat", "xml", "ini", "cfg", "sav", "srm",
    "state", "db", "nfo", "url", "html", "htm", "sqlite", "torrent", "mp3", "mp4", "wav",
    "log", "bak", "lic", "sbi", "md5", "sha1", "json", "csv", "pdf", "exe.bak",
};

std::string lower(const std::string &s) {
    std::string out = s;
    for (char &c : out)
        if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return out;
}

std::string extensionOf(const std::string &filename) {
    const size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= filename.size()) return std::string();
    return lower(filename.substr(dot + 1));
}

std::vector<std::string> splitCommas(const std::string &s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        const size_t comma = s.find(',', start);
        const std::string piece = s.substr(start, comma - start);
        if (!piece.empty()) out.push_back(piece);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

bool isDirectory(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// A cheap way to tell a real library from a stray leftover: how many entries sit directly
// inside it. No recursion, no per-file stat — just enough to compare two candidates.
int countEntries(const std::string &path) {
    DIR *d = opendir(path.c_str());
    if (!d) return 0;

    int count = 0;
    while (dirent *entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        ++count;
    }
    closedir(d);
    return count;
}

} // namespace

std::string SystemCatalog::displayName(const std::string &directoryName) {
    for (const Alias &alias : kNames)
        if (directoryName == alias.dir) return alias.name;
    return directoryName;
}

std::vector<std::string> SystemCatalog::extensionsFor(const std::string &directoryName) {
    for (const Extensions &e : kExtensions)
        if (directoryName == e.dir) return splitCommas(e.list);

    // Try again case-insensitively, since drives disagree about "snes" versus "SNES".
    const std::string key = CoreIndex::normalise(directoryName);
    for (const Extensions &e : kExtensions)
        if (CoreIndex::normalise(e.dir) == key) return splitCommas(e.list);

    return std::vector<std::string>();
}

bool SystemCatalog::isIgnoredDirectory(const std::string &name) {
    if (name.empty() || name[0] == '.') return true;
    for (const char *ignored : kIgnoredDirs)
        if (strcasecmp(name.c_str(), ignored) == 0) return true;
    return false;
}

bool SystemCatalog::looksLikeGame(const std::string &filename,
                                  const std::vector<std::string> &extensions) {
    const std::string ext = extensionOf(filename);
    if (ext.empty()) return false;

    // A zip is a game on every system: the launcher reads the ROM out of it.
    if (ext == "zip") return true;

    if (!extensions.empty()) {
        for (const std::string &candidate : extensions)
            if (ext == candidate) return true;
        return false;
    }

    // Unknown system: keep everything that is not obviously something else. Being wrong in
    // this direction shows a stray file in the list; being wrong the other way hides games.
    for (const char *denied : kDeniedExtensions)
        if (ext == denied) return false;
    return true;
}

std::vector<std::string> SystemCatalog::gameParents(const std::string &root) {
    // "<volume>/games/<System>" is the usual layout; "<volume>/<System>" also occurs, and
    // both can be in use on the same drive.
    return {root + "/games", root};
}

std::vector<CatalogEntry> SystemCatalog::discover(const std::vector<std::string> &roots,
                                                  const CoreIndex &cores) {
    std::vector<CatalogEntry> found;

    for (const std::string &root : roots) {
        for (const std::string &parent : gameParents(root)) {
            DIR *d = opendir(parent.c_str());
            if (!d) continue;

            while (dirent *entry = readdir(d)) {
                const std::string name = entry->d_name;
                if (name.empty() || name[0] == '.' || name[0] == '_' || name[0] == '$')
                    continue;
                if (isIgnoredDirectory(name) || name == "games") continue;

                const std::string dir = parent + "/" + name;
                if (!isDirectory(dir)) continue;

                // The core decides. Without one the folder may be anything at all, and a
                // system nobody can start is not worth a tile.
                const std::string core = cores.find({name, displayName(name)});
                if (core.empty()) continue;

                // The same system can turn up twice: under both `gameParents()` of one root,
                // or — since only one external drive is meant to be attached at a time — as
                // a stray leftover folder on the SD card beside the real library on a USB
                // drive. Keeping whichever was found first would silently drop the real
                // library behind an empty-looking stub the moment the SD card happens to be
                // scanned before the drive; keeping whichever holds more instead makes that
                // choice about content, not about scan order.
                const auto existing = std::find_if(
                    found.begin(), found.end(), [&](const CatalogEntry &e) {
                        return CoreIndex::normalise(e.key) == CoreIndex::normalise(name);
                    });
                if (existing != found.end()) {
                    if (countEntries(dir) <= countEntries(existing->dir)) continue;
                    found.erase(existing);
                }

                CatalogEntry system;
                system.key = name;
                system.name = displayName(name);
                system.group = CoreIndex::groupOf(core);
                system.core = core;
                system.root = root;
                system.dir = dir;
                system.extensions = extensionsFor(name);
                for (const std::string &ext : system.extensions)
                    if (ext == "cue" || ext == "chd" || ext == "iso") system.discBased = true;

                found.push_back(system);
            }
            closedir(d);
        }
    }

    // Two directories can mean the same machine — "NGPC" and "NeoGeoPocket-Color" are both
    // the Neo Geo Pocket Color. Usually only one of them holds games and the empty one never
    // reaches the database, but when both do, two tiles with the same caption are no use.
    // The later one keeps its directory name so the two can be told apart.
    std::vector<std::string> used;
    for (CatalogEntry &system : found) {
        if (std::find(used.begin(), used.end(), system.name) != used.end())
            system.name = system.key;
        used.push_back(system.name);
    }

    std::sort(found.begin(), found.end(), [](const CatalogEntry &a, const CatalogEntry &b) {
        if (a.group != b.group) return a.group < b.group;
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    return found;
}
