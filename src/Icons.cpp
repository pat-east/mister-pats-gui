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
    {"SEGACD", "SEGACD"},
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
    available_.clear();

    DIR *dir = opendir(directory.c_str());
    if (!dir) {
        std::printf("icons: %s not found, systems will use typography\n", directory.c_str());
        return false;
    }

    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".png") == 0)
            available_.insert(name.substr(0, name.size() - 4));
    }
    closedir(dir);

    std::printf("icons: %zu available in %s\n", available_.size(), directory.c_str());
    return !available_.empty();
}

std::string Icons::pathFor(const std::string &systemName) const {
    if (available_.empty()) return std::string();

    const std::string key = normalise(systemName);

    // Direct hit, e.g. "SATURN", "VECTREX", "MSX".
    if (available_.count(key)) return directory_ + "/" + key + ".png";

    for (const Alias &alias : kAliases) {
        if (key == alias.system && available_.count(alias.icon))
            return directory_ + "/" + std::string(alias.icon) + ".png";
    }

    if (available_.count(kFallbackIcon))
        return directory_ + "/" + std::string(kFallbackIcon) + ".png";

    return std::string();
}
