#include "Icons.h"

#include <cstdio>
#include <dirent.h>

namespace {

struct Alias {
    const char *system;   // normalised system name
    const char *icon;     // icon basename without extension
};

// Only the cases where the names differ; an exact match is tried first.
const Alias kAliases[] = {
    // Consoles
    {"3DO", "PANASONIC"},
    {"ATARI2600", "ATARI2600"},
    {"CHANNELF", "CHANNELF"},
    {"COLECOVISION", "COLECO"},
    {"FAMICOMDISKSYSTEM", "FDS"},
    {"GENESIS", "MD"},
    {"GENESIS32X", "SEGA32X"},
    {"JAGUARCD", "ATARIST"},
    {"MAGNAVOXODYSSEY2", "VIDEOPAC"},
    {"MASTERSYSTEM", "MS"},
    {"MEGADRIVE", "MD"},
    {"NES", "FC"},
    {"NEOGEOCD", "NEOCD"},
    {"NEOGEOMVSAES", "NEOGEO"},
    {"NINTENDO64", "N64"},
    {"PLAYSTATION", "PS"},
    {"SG1000", "SG1000"},
    {"SNES", "SFC"},
    {"SEGACD", "MEGACD"},
    {"TURBOGRAFX16", "PCE"},
    {"TURBOGRAFX16CD", "PCECD"},
    {"VIRTUALBOY", "VB"},
    {"BALLYASTROCADE", "FLASHBACK"},

    // Handhelds
    {"ATARILYNX", "LYNX"},
    {"GAMEWATCH", "GW"},
    {"GAMEGEAR", "GG"},
    {"GAMEBOY", "GB"},
    {"GAMEBOY2PLAYER", "GB"},
    {"GAMEBOYADVANCE", "GBA"},
    {"GAMEBOYADVANCE2PLAYER", "GBA"},
    {"GAMEBOYCOLOR", "GBC"},
    {"MEGADUCK", "MEGADUCK"},
    {"NEOGEOPOCKET", "NGP"},
    {"NEOGEOPOCKETCOLOR", "NGP"},
    {"POKEMONMINI", "POKEMINI"},
    {"SUPERGAMEBOY", "SGB"},
    {"WONDERSWAN", "WS"},
    {"WONDERSWANCOLOR", "WSC"},

    // Computers
    {"AMIGACD32", "AMIGACD"},
    {"AMSTRADCPC", "CPC"},
    {"AMSTRAD", "CPC"},
    {"ATARI800XL", "ATARI800"},
    {"COMMODORE64", "C64"},
    {"COMMODORE16", "CPLUS4"},
    {"COMMODOREPET2001", "CPET"},
    {"COMMODOREVIC20", "VIC20"},
    {"MACINTOSHPLUS", "VMAC"},
    {"PC486SX", "DOS"},
    {"PCXT", "DOS"},
    {"ZXSPECTRUM", "ZXS"},
    {"ZXSPECTRUMNEXT", "ZXS"},
    {"TSCONFIG", "ZXS"},
    {"MSX1", "MSX"},
    {"SAMCOUPE", "ZXS"},
};

// Keep this list in sync with the public assets/icons directory. It lets a fresh install
// fetch the complete set even when it has no local PNG files to discover names from.
const char *kRepositoryIcons[] = {
    "ADVMAME", "AMIGA", "AMIGACD", "AMIGACDTV", "ARCADE", "ARCADE_FBNEO", "ARDUBOY",
    "ATARI2600", "ATARI5200", "ATARI7800", "ATARI800", "ATARI800-alt", "ATARIST",
    "ATOMISWAVE", "C64", "CANNONBALL", "CAVESTORY", "CHAILOVE", "CHANNELF", "COLECO",
    "COLSGM", "CPC", "CPET", "CPLUS4", "CPS1", "CPS2", "CPS3", "DAPHNE", "DC",
    "DINOTHAWR", "DOOM", "DOS", "EASYRPG", "ENTERPRISE", "FBNEO", "FC", "FDS",
    "FLASHBACK", "GB", "GBA", "GBC", "GG", "GW", "INTELLIVISION", "LOWRESNX",
    "LUTRO", "LYNX", "MAME", "MAME2003PLUS", "MAME2010", "MD", "MDMSU", "MEGACD",
    "MEGADUCK", "MS", "MSX", "MSX2", "N64", "N64DD", "NAOMI", "NDS", "NEOCD",
    "NEOGEO", "NGC", "NGP", "OPENBOR", "PALMOS", "PANASONIC", "PC88", "PC98", "PCE",
    "PCECD", "PCFX", "PGM", "PICO", "POKEMINI", "PORTS", "PS", "PSP", "PSPMINIS",
    "SATELLAVIEW", "SATURN", "SCUMMVM", "SEGA32X", "SEGACD", "SFC", "SFCMSU", "SFX",
    "SG1000", "SGB", "SUFAMI", "SUPERVISION", "SUPERVISION-alt", "THOMSON", "TI83",
    "TIC", "TYRQUAKE", "UZEBOX", "VB", "VECTREX", "VIC20", "VIDEOPAC", "VIDEOPAC-alt",
    "VIDEOS", "VIDEOTON", "VMAC", "WS", "WSC", "X1", "X68000", "XRICK", "ZXS",
    "cpc-alt",
};

std::string normalise(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c >= 'a' && c <= 'z') out.push_back(char(c - 32));
        else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out.push_back(c);
    }
    return out;
}

} // namespace

bool Icons::load(const std::string &directory) {
    directory_ = directory;
    bitmaps_.clear();
    pngs_.clear();

    DIR *dir = opendir(directory.c_str());
    if (!dir) {
        return false;
    }

    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".bmp") == 0)
            bitmaps_.insert(name.substr(0, name.size() - 4));
        else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".png") == 0)
            pngs_.insert(name.substr(0, name.size() - 4));
    }
    closedir(dir);

    return !bitmaps_.empty() || !pngs_.empty();
}

std::string Icons::pathFor(const std::string &systemName) const {
    if (bitmaps_.empty() && pngs_.empty()) return std::string();

    const auto pathForIcon = [this](const std::string &icon) {
        if (bitmaps_.count(icon)) return directory_ + "/" + icon + ".bmp";
        if (pngs_.count(icon)) return directory_ + "/" + icon + ".png";
        return std::string();
    };

    const std::string key = normalise(systemName);

    // Direct hit, e.g. "SATURN", "VECTREX", "MSX".
    const std::string directPath = pathForIcon(key);
    if (!directPath.empty()) return directPath;

    for (const Alias &alias : kAliases) {
        if (key == alias.system) {
            const std::string path = pathForIcon(alias.icon);
            if (!path.empty()) return path;
        }
    }

    return pathForIcon(kFallbackIcon);
}

std::vector<std::string> Icons::paths() const {
    std::vector<std::string> result;
    std::set<std::string> names = bitmaps_;
    names.insert(pngs_.begin(), pngs_.end());
    result.reserve(names.size());
    for (const std::string &name : names)
        result.push_back(directory_ + "/" + name +
                         (bitmaps_.count(name) ? ".bmp" : ".png"));
    return result;
}

std::vector<std::string> Icons::missingBitmaps() const {
    std::vector<std::string> result;
    for (const char *name : kRepositoryIcons)
        if (!bitmaps_.count(name)) result.emplace_back(name);
    return result;
}
