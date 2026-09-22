#include "Library.h"

#include "SystemCatalog.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sys/stat.h>

namespace {

const char *kCoreDirs[] = {
    "/media/fat/_Console", "/media/fat/_Computer", "/media/fat/_Handheld",
    "/media/fat/_Arcade",  "/media/fat/_Other",    "/media/fat/_Utility",
    "/media/fat/_Ports",   "/media/fat/_ExtraCores",
};

// Section file per group, in the order they should appear.
struct GroupFile {
    const char *file;
    const char *label;
};

const GroupFile kGroups[] = {
    {"Console.ini", "Consoles"},
    {"Handheld.ini", "Handhelds"},
    {"Computer.ini", "Computers"},
    {"Arcade.ini", "Arcade"},
    {"Ports.ini", "Ports"},
};

// Which entry of the core's own OSD menu a game is handed to. The default, file slot 1, fits
// the cartridge cores; the ones below name a different slot in their Verilog CONF_STR, and
// sending a game to the wrong slot simply does nothing. Values read from each core's source:
//
//   PSX          "H7S1,CUECHD,Load CD"          MegaCD    "S0,CUECHD,Insert Disk"
//   Saturn       "S0,CUECHD,Insert Disc"        NeoGeo CD "S1,CUECHD,Load CD Image"
//   3DO          "S0,CUEISO,Insert Disk"        TGFX16    "FS0,PCEBIN,Load TurboGrafx"
//
// Names are matched verbatim against ConsoleMode's section files; systems.conf overrides them.
struct SlotDefault {
    const char *system;
    int index;
    char type;   // 'f' file slot, 's' disk slot
};

const SlotDefault kSlotDefaults[] = {
    {"PLAYSTATION", 1, 's'},
    {"SATURN", 0, 's'},
    {"SEGA CD", 0, 's'},
    {"TURBOGRAFX-16 CD", 0, 's'},
    {"NEO GEO CD", 1, 's'},
    {"3DO", 0, 's'},
    {"TURBOGRAFX-16", 0, 'f'},
};

bool directoryExists(const std::string &path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool fileExists(const std::string &path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// The scraper writes JPEG — a third the size of the PNGs Console Mode leaves behind, which is
// the point of scraping our own — but the extension in `Game::boxart` has to match whatever
// is actually on disk, or the lookup just misses. Checked once here, with a real stat(), not
// assumed: a stray non-.jpg or non-.png file sits there unfound either way, but a scraped
// cover no longer does. This is a read, not a write, so it costs nothing on a card that only
// makes writes expensive.
std::string resolveArtwork(const std::string &base) {
    if (fileExists(base + ".jpg")) return base + ".jpg";
    return base + ".png";
}

std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> splitList(const std::string &value) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t comma = value.find(',', start);
        const std::string piece = trim(value.substr(start, comma - start));
        if (!piece.empty()) out.push_back(piece);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

std::string stripTrailingSlash(const std::string &path) {
    if (path.size() > 1 && path.back() == '/') return path.substr(0, path.size() - 1);
    return path;
}

std::string lastComponent(const std::string &path) {
    const std::string clean = stripTrailingSlash(path);
    const size_t slash = clean.find_last_of('/');
    return (slash == std::string::npos) ? clean : clean.substr(slash + 1);
}

// Uppercase letters and digits only, so "ATARI 2600", "Atari2600" and "atari-2600" match.
std::string normalise(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c >= 'a' && c <= 'z') out.push_back(char(c - 32));
        else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out.push_back(c);
    }
    return out;
}

// "SNES_20260823.rbf" -> "SNES";  "Atari 2600.mgl" -> "Atari 2600"
std::string coreNameOf(const std::string &filename) {
    const size_t dot = filename.find_last_of('.');
    std::string base = (dot == std::string::npos) ? filename : filename.substr(0, dot);

    const size_t underscore = base.find_last_of('_');
    if (underscore != std::string::npos && base.size() - underscore == 9) {
        bool allDigits = true;
        for (size_t i = underscore + 1; i < base.size(); ++i)
            if (base[i] < '0' || base[i] > '9') allDigits = false;
        if (allDigits) base = base.substr(0, underscore);
    }

    return base;
}

// The extensions that make a single file a disc image rather than a folder of tracks.
bool isDiscExtension(std::string ext) {
    for (char &c : ext)
        if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return ext == "cue" || ext == "chd" || ext == "iso" || ext == "ccd";
}

bool hasSuffix(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() &&
           strcasecmp(s.c_str() + s.size() - suffix.size(), suffix.c_str()) == 0;
}

// The section files only know /media/fat; libraries often live on USB.
std::vector<std::string> mountVariants(const std::string &dir) {
    std::vector<std::string> out{dir};
    const std::string prefix = "/media/fat/";
    if (dir.compare(0, prefix.size(), prefix) == 0)
        out.push_back("/media/usb0/" + dir.substr(prefix.size()));
    return out;
}

} // namespace

bool Game::isArchive() const { return hasSuffix(path, ".zip"); }

void Library::indexCores() {
    for (const char *dir : kCoreDirs) {
        DIR *d = opendir(dir);
        if (!d) continue;

        while (dirent *entry = readdir(d)) {
            const std::string filename = entry->d_name;
            if (filename[0] == '.') continue;
            if (!hasSuffix(filename, ".rbf") && !hasSuffix(filename, ".mgl")) continue;

            CoreEntry core;
            core.key = normalise(coreNameOf(filename));
            core.relPath = std::string(lastComponent(dir)) + "/" + coreNameOf(filename);
            if (!core.key.empty()) cores_.push_back(core);
        }
        closedir(d);
    }
}

std::string Library::matchCore(const GameSystem &system) const {
    std::vector<std::string> keys;
    keys.push_back(normalise(system.name));
    for (const std::string &dir : system.romDirs) keys.push_back(normalise(lastComponent(dir)));

    for (const std::string &key : keys) {
        if (key.empty()) continue;
        for (const CoreEntry &core : cores_)
            if (core.key == key) return core.relPath;
    }

    // Fall back to a prefix match, which catches "GAMEBOYCOLOR" against "GBC"-style names.
    for (const std::string &key : keys) {
        if (key.size() < 3) continue;
        for (const CoreEntry &core : cores_)
            if (core.key.compare(0, key.size(), key) == 0 || key.compare(0, core.key.size(), core.key) == 0)
                return core.relPath;
    }

    return std::string();
}

bool Library::loadSectionFile(const std::string &file, const std::string &group) {
    std::ifstream in(file);
    if (!in) return false;

    std::vector<std::string> order;
    std::map<std::string, GameSystem> parsed;
    std::string section;
    std::string line;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = normalise(trim(line.substr(0, eq)));
        const std::string value = trim(line.substr(eq + 1));

        if (key == "CONSOLELIST") {
            order = splitList(value);
            continue;
        }
        if (section.empty()) continue;

        GameSystem &system = parsed[section];
        system.name = section;
        system.group = group;

        if (key == "ROMDIRS") {
            const bool known = index_.pathsFor(section) != nullptr;
            for (const std::string &dir : splitList(value))
                for (const std::string &variant : mountVariants(stripTrailingSlash(dir)))
                    // Only probe the filesystem when the index cannot vouch for the system.
                    if (known || directoryExists(variant)) system.romDirs.push_back(variant);
        } else if (key == "ROMEXTS") {
            system.romExts = splitList(value);
            for (const std::string &ext : system.romExts)
                if (isDiscExtension(ext)) system.discBased = true;
        }
    }

    for (const std::string &name : order) {
        auto it = parsed.find(name);
        if (it == parsed.end()) continue;

        GameSystem system = it->second;
        if (system.romDirs.empty() || system.romExts.empty()) continue;

        for (const std::string &dir : system.romDirs) {
            const std::string media = dir + "/media";
            // Recorded as a candidate without checking: a probe would touch the game volume,
            // and a missing artwork file costs nothing but a failed open later on.
            system.mediaDirs.push_back(media);
        }

        system.core = matchCore(system);
        systems_.push_back(system);
    }

    return true;
}

void Library::applySlotDefaults() {
    for (GameSystem &system : systems_) {
        for (const SlotDefault &slot : kSlotDefaults) {
            if (system.name != slot.system) continue;
            system.fileIndex = slot.index;
            system.fileType = slot.type;
            break;
        }
    }
}

void Library::applyOverrides() {
    std::ifstream in(kOverridesFile);
    if (!in) return;

    size_t applied = 0;
    std::string line;

    while (std::getline(in, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        // Name may contain spaces, so the numeric field is found from the right.
        const size_t lastSpace = trimmed.find_last_of(" \t");
        if (lastSpace == std::string::npos) continue;

        std::string rest = trim(trimmed.substr(lastSpace + 1));
        std::string head = trim(trimmed.substr(0, lastSpace));

        char type = 'f';
        if (rest.size() == 1 && (rest[0] == 'f' || rest[0] == 's')) {
            type = rest[0];
            const size_t prev = head.find_last_of(" \t");
            if (prev == std::string::npos) continue;
            rest = trim(head.substr(prev + 1));
            head = trim(head.substr(0, prev));
        }

        const int index = atoi(rest.c_str());
        for (GameSystem &system : systems_) {
            if (system.name != head) continue;
            system.fileIndex = index;
            system.fileType = type;
            ++applied;
        }
    }

    if (applied) std::printf("library: %zu system overrides from %s\n", applied, kOverridesFile);
}

bool Library::loadFromDatabase() {
    if (!database_.exists() || !database_.load()) return false;

    for (const DatabaseSystem &entry : database_.systems()) {
        GameSystem system;
        system.name = entry.name;
        system.group = entry.group;
        system.core = entry.core;
        system.dbKey = entry.key;
        system.discBased = entry.discBased;
        system.romDirs.push_back(entry.dir);

        // Needed to look inside a zip: MiSTer addresses an archived ROM as
        // "<archive>.zip/<inner file>", and finding that file means knowing which extension
        // to look for. Without this every archived game refuses to start — which is most of
        // a cartridge library.
        for (const std::string &ext : SystemCatalog::extensionsFor(entry.key))
            system.romExts.push_back("." + ext);
        system.mediaDirs.push_back(entry.dir + "/media");
        systems_.push_back(system);
    }

    return !systems_.empty();
}

bool Library::load(const std::string &sectionDir) {
    systems_.clear();
    cores_.clear();
    usingDatabase_ = false;

    // Our own database first. When it answers, nothing else is touched — no section files,
    // no foreign cache, and above all no walk of the game volume.
    if (loadFromDatabase()) {
        usingDatabase_ = true;
        indexCores();
        applySlotDefaults();
        applyOverrides();
        std::printf("library: %zu systems from the game database\n", systems_.size());
        return true;
    }

    index_.load();
    indexCores();

    for (const GroupFile &group : kGroups)
        loadSectionFile(sectionDir + "/" + group.file, group.label);

    applySlotDefaults();
    applyOverrides();

    std::printf("library: %zu cores indexed, %zu systems with directories (no own database)\n",
                cores_.size(), systems_.size());
    return !systems_.empty();
}

Game Library::makeGame(const GameSystem &system, const std::string &path) {
    Game game;
    game.path = path;

    const std::string filename = lastComponent(path);
    game.name = filename;

    // Cut off the extension — but on a disc system the entry is normally a folder, and
    // folder names carry dots as readily as any title does: "Capcom vs. SNK - Millennium
    // Fight 2000 Pro", "Bio F.R.E.A.K.S", "A.IV - Evolution Global". Cutting at the last dot
    // there leaves "Capcom vs" and, worse, looks for artwork under that name too.
    const size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        const std::string extension = filename.substr(dot + 1);
        if (!system.discBased || isDiscExtension(extension))
            game.name = filename.substr(0, dot);
    }

    // Artwork sits next to the game itself. Deriving it from the game's own directory keeps
    // it right no matter which volume the game came from — a system's directories can span
    // the SD card and a USB drive at the same time.
    const size_t slash = path.find_last_of('/');
    if (slash != std::string::npos) {
        const std::string media = path.substr(0, slash) + "/media/" + game.name;
        game.boxart = resolveArtwork(media);
        game.background = resolveArtwork(media + "-BG");
    }

    // Games organised into subfolders keep their artwork one level up, in the system's own
    // media folder, because that is where a scraper puts it.
    for (const std::string &dir : system.romDirs) {
        if (path.compare(0, dir.size(), dir) != 0) continue;
        const std::string media = dir + "/media/" + game.name;
        if (dir + "/media/" == path.substr(0, slash) + "/media/") break;   // already looking there
        game.boxartFallback = resolveArtwork(media);
        game.backgroundFallback = resolveArtwork(media + "-BG");
        break;
    }

    return game;
}

bool Library::hasGames(const GameSystem &system) const {
    if (!system.dbKey.empty()) return true;
    if (index_.pathsFor(system.name)) return true;
    if (!scanningAllowed_) return false;

    for (const std::string &dir : system.romDirs) {
        DIR *d = opendir(dir.c_str());
        if (!d) continue;

        while (dirent *entry = readdir(d)) {
            const std::string filename = entry->d_name;
            if (filename[0] == '.') continue;

            bool match = hasSuffix(filename, ".zip");
            for (const std::string &ext : system.romExts)
                if (hasSuffix(filename, ext)) match = true;

            if (match) { closedir(d); return true; }
        }
        closedir(d);
    }
    return false;
}

const GameSystem *Library::findSystem(const std::string &name) const {
    for (const GameSystem &system : systems_)
        if (system.name == name) return &system;
    return nullptr;
}

const GameSystem *Library::systemForPath(const std::string &path) const {
    const GameSystem *best = nullptr;
    size_t bestLength = 0;

    for (const GameSystem &system : systems_) {
        for (const std::string &dir : system.romDirs) {
            if (path.compare(0, dir.size(), dir) != 0) continue;
            if (dir.size() > bestLength) { best = &system; bestLength = dir.size(); }
        }
    }

    return best;
}

std::vector<Game> Library::gamesOf(const GameSystem &system) const {
    std::vector<Game> games;

    // One file, read only now that this system is actually being opened, and already sorted
    // when it was written.
    if (!system.dbKey.empty()) {
        const std::vector<std::string> paths = database_.pathsFor(system.dbKey);
        games.reserve(paths.size());
        for (const std::string &path : paths) games.push_back(makeGame(system, path));
        return games;
    }

    // The cached index avoids touching the game volume at all, which is what keeps a
    // marginally powered drive on the bus.
    if (const std::vector<std::string> *paths = index_.pathsFor(system.name)) {
        games.reserve(paths->size());
        for (const std::string &path : *paths) games.push_back(makeGame(system, path));

        std::sort(games.begin(), games.end(), [](const Game &a, const Game &b) {
            return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
        });
        return games;
    }

    if (!scanningAllowed_) return games;

    for (const std::string &dir : system.romDirs) {
        DIR *d = opendir(dir.c_str());
        if (!d) continue;

        while (dirent *entry = readdir(d)) {
            const std::string filename = entry->d_name;
            if (filename[0] == '.') continue;

            bool match = hasSuffix(filename, ".zip");
            for (const std::string &ext : system.romExts)
                if (hasSuffix(filename, ext)) match = true;
            if (!match) continue;

            games.push_back(makeGame(system, dir + "/" + filename));
        }
        closedir(d);
    }

    std::sort(games.begin(), games.end(), [](const Game &a, const Game &b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    return games;
}
