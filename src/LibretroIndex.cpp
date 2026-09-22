#include "LibretroIndex.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

#include "Downloader.h"

namespace {

struct Platform {
    const char *key;        // the MiSTer game directory
    const char *platform;   // the directory on thumbnails.libretro.com
};

// Checked one by one against the server's own index of 123 platforms. Names that looked
// obvious but are not there — Apple II, BBC Micro, Oric, TRS-80, TI-99, Mega Duck, Gamate,
// VC 4000, Bally Astrocade — are deliberately absent: an entry that 404s would leave the
// system silently without artwork and no hint as to why.
const Platform kPlatforms[] = {
    {"3DO", "The 3DO Company - 3DO"},
    {"AO486", "DOS"},
    {"ATARI5200", "Atari - 5200"},
    {"ATARI7800", "Atari - 7800"},
    {"ATARI800", "Atari - 8-bit"},
    {"Amiga", "Commodore - Amiga"},
    {"AmigaCD32", "Commodore - CD32"},
    {"Amstrad", "Amstrad - CPC"},
    {"Arcadia", "Emerson - Arcadia 2001"},
    {"Atari2600", "Atari - 2600"},
    {"AtariLynx", "Atari - Lynx"},
    {"AtariST", "Atari - ST"},
    {"C16", "Commodore - Plus-4"},
    {"C64", "Commodore - 64"},
    {"CD-i", "Philips - CD-i"},
    {"Casio_PV-1000", "Casio - PV-1000"},
    {"ChannelF", "Fairchild - Channel F"},
    {"Coleco", "Coleco - ColecoVision"},
    {"CreatiVision", "VTech - CreatiVision"},
    {"FDS", "Nintendo - Family Computer Disk System"},
    {"GAMEBOY", "Nintendo - Game Boy"},
    {"GAMEBOY2P", "Nintendo - Game Boy"},
    {"GBA", "Nintendo - Game Boy Advance"},
    {"GBA2P", "Nintendo - Game Boy Advance"},
    {"GBC", "Nintendo - Game Boy Color"},
    {"GameCom", "Tiger - Game.com"},
    {"GameGear", "Sega - Game Gear"},
    {"GameGear2P", "Sega - Game Gear"},
    {"Intellivision", "Mattel - Intellivision"},
    {"Jaguar", "Atari - Jaguar"},
    {"MSX", "Microsoft - MSX"},
    {"MSX1", "Microsoft - MSX"},
    {"MegaCD", "Sega - Mega-CD - Sega CD"},
    {"MegaDrive", "Sega - Mega Drive - Genesis"},
    {"N64", "Nintendo - Nintendo 64"},
    {"NEOGEO", "SNK - Neo Geo"},
    {"NES", "Nintendo - Nintendo Entertainment System"},
    {"NGPC", "SNK - Neo Geo Pocket Color"},
    {"NeoGeo-CD", "SNK - Neo Geo CD"},
    {"NeoGeoPocket", "SNK - Neo Geo Pocket"},
    {"NeoGeoPocket-Color", "SNK - Neo Geo Pocket Color"},
    {"ODYSSEY2", "Magnavox - Odyssey2"},
    {"PCXT", "DOS"},
    {"PET2001", "Commodore - PET"},
    {"PSX", "Sony - PlayStation"},
    {"PokemonMini", "Nintendo - Pokemon Mini"},
    {"S32X", "Sega - 32X"},
    {"SCV", "Epoch - Super Cassette Vision"},
    {"SG1000", "Sega - SG-1000"},
    {"SGB", "Nintendo - Game Boy"},
    {"SMS", "Sega - Master System - Mark III"},
    {"SNES", "Nintendo - Super Nintendo Entertainment System"},
    {"SVI328", "Spectravideo - SVI-318 - SVI-328"},
    {"Saturn", "Sega - Saturn"},
    {"Spectrum", "Sinclair - ZX Spectrum"},
    {"SuperVision", "Watara - Supervision"},
    {"TGFX16", "NEC - PC Engine - TurboGrafx 16"},
    {"TGFX16-CD", "NEC - PC Engine CD - TurboGrafx-CD"},
    {"VECTREX", "GCE - Vectrex"},
    {"VIC20", "Commodore - VIC-20"},
    {"Vectrex", "GCE - Vectrex"},
    {"VirtualBoy", "Nintendo - Virtual Boy"},
    {"WonderSwan", "Bandai - WonderSwan"},
    {"WonderSwanColor", "Bandai - WonderSwan Color"},
    {"X68000", "Sharp - X68000"},
    {"ZX81", "Sinclair - ZX 81"},
    {"ZXNext", "Sinclair - ZX Spectrum"},
    {"snes", "Nintendo - Super Nintendo Entertainment System"},
};

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// The listing is HTML, so names arrive percent-encoded and with the handful of entities a
// directory index produces.
std::string decodeHref(const std::string &text) {
    std::string out;
    out.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const int hi = hexValue(text[i + 1]), lo = hexValue(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(char(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        if (text[i] == '&') {
            if (text.compare(i, 5, "&amp;") == 0)  { out.push_back('&'); i += 4; continue; }
            if (text.compare(i, 4, "&lt;") == 0)   { out.push_back('<'); i += 3; continue; }
            if (text.compare(i, 4, "&gt;") == 0)   { out.push_back('>'); i += 3; continue; }
            if (text.compare(i, 6, "&quot;") == 0) { out.push_back('"'); i += 5; continue; }
            if (text.compare(i, 6, "&#39;") == 0)  { out.push_back('\''); i += 4; continue; }
        }
        out.push_back(text[i]);
    }
    return out;
}

bool nonEmptyFile(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

} // namespace

std::string LibretroIndex::platformFor(const std::string &systemKey) {
    for (const Platform &entry : kPlatforms)
        if (systemKey == entry.key) return entry.platform;

    for (const Platform &entry : kPlatforms)
        if (strcasecmp(systemKey.c_str(), entry.key) == 0) return entry.platform;

    return std::string();
}

std::string LibretroIndex::normalise(const std::string &title) {
    std::string out;
    int depth = 0;

    for (char c : title) {
        if (c == '(' || c == '[') { ++depth; continue; }
        if (c == ')' || c == ']') { if (depth) --depth; continue; }
        if (depth) continue;

        if (c >= 'a' && c <= 'z') out.push_back(c);
        else if (c >= 'A' && c <= 'Z') out.push_back(char(c + 32));
        else if (c >= '0' && c <= '9') out.push_back(c);
    }
    return out;
}

int LibretroIndex::regionRank(const std::string &filename) {
    static const char *kTags[] = {"(USA", "(World", "(Europe", "(Japan"};
    for (int i = 0; i < 4; ++i)
        if (filename.find(kTags[i]) != std::string::npos) return i;
    return 4;
}

bool LibretroIndex::parse(const std::string &htmlPath) {
    std::ifstream in(htmlPath);
    if (!in) {
        error_ = "cannot read the downloaded index";
        return false;
    }

    byKey_.clear();

    std::string line;
    while (std::getline(in, line)) {
        size_t pos = 0;
        while ((pos = line.find("href=\"", pos)) != std::string::npos) {
            pos += 6;
            const size_t end = line.find('"', pos);
            if (end == std::string::npos) break;

            const std::string href = line.substr(pos, end - pos);
            pos = end;

            if (href.size() < 5 || href.compare(href.size() - 4, 4, ".png") != 0) continue;

            const std::string name = decodeHref(href.substr(0, href.size() - 4));
            const std::string key = normalise(name);
            if (key.empty()) continue;

            auto existing = byKey_.find(key);
            if (existing == byKey_.end()) byKey_[key] = name;
            else if (regionRank(name) < regionRank(existing->second)) existing->second = name;
        }
    }

    return !byKey_.empty();
}

bool LibretroIndex::open(const std::string &platform, Downloader &downloader,
                         const std::string &cacheDir) {
    error_.clear();
    platform_ = platform;
    byKey_.clear();

    if (platform.empty()) {
        error_ = "no thumbnail set for this system";
        return false;
    }

    mkdir(cacheDir.c_str(), 0777);

    // The cache is a file per platform, kept on the card rather than the game drive. Re-using
    // it is what makes a second scrape run cost nothing but the images that are missing.
    std::string cacheName;
    for (char c : platform) cacheName.push_back((c == '/' || c == ' ') ? '_' : c);
    const std::string cached = cacheDir + "/" + cacheName + ".html";

    if (!nonEmptyFile(cached)) {
        const std::string url = std::string(kHost) + "/" +
                                Downloader::encodeComponent(platform) + "/Named_Boxarts/";
        // Generous: this is 2.5 MB of directory listing for a big platform, once.
        if (!downloader.fetch(url, cached, 120)) {
            error_ = "could not fetch the index: " + downloader.lastError();
            return false;
        }
    }

    if (!parse(cached)) {
        error_ = "the index could not be read";
        return false;
    }

    std::printf("libretro: %s, %zu titles indexed\n", platform.c_str(), byKey_.size());
    return true;
}

std::string LibretroIndex::match(const std::string &title) const {
    const auto found = byKey_.find(normalise(title));
    return found == byKey_.end() ? std::string() : found->second;
}
