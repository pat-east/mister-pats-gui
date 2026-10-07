#include "LibretroIndex.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
    // MiSTer's arcade set is named after MAME's, and so is this platform — "Killer Instinct
    // (v1.5d)" is the same string in a .mra filename and on the server. The other arcade
    // platform there, "FBNeo - Arcade Games", names the same games quite differently.
    {"Arcade", "MAME"},
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

std::string digitsOf(const std::string &key) {
    std::string digits;
    for (char c : key)
        if (c >= '0' && c <= '9') digits.push_back(c);
    return digits;
}

// Returns maxDistance + 1 as soon as a candidate cannot qualify. This runs only after all
// exact-key variants fail, and the caller first narrows candidates by length and prefix.
int editDistanceWithin(const std::string &a, const std::string &b, int maxDistance) {
    if (std::abs(int(a.size()) - int(b.size())) > maxDistance) return maxDistance + 1;
    std::vector<int> previous(b.size() + 1), current(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) previous[j] = int(j);

    for (size_t i = 1; i <= a.size(); ++i) {
        current[0] = int(i);
        int rowMin = current[0];
        for (size_t j = 1; j <= b.size(); ++j) {
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1,
                                   previous[j - 1] + (a[i - 1] != b[j - 1])});
            rowMin = std::min(rowMin, current[j]);
        }
        if (rowMin > maxDistance) return maxDistance + 1;
        previous.swap(current);
    }
    return previous[b.size()];
}

bool specialEdition(const std::string &filename) {
    for (const char *tag : {"(Alternate)", "(Beta)", "(Demo)", "(Proto)", "(Hack)"})
        if (filename.find(tag) != std::string::npos) return true;
    return false;
}

bool compatibleRegion(const std::string &local, const std::string &remote) {
    const auto has = [&](const char *tag) { return local.find(tag) != std::string::npos; };
    if (has("(J)") || has("(Japan)")) return remote.find("(Japan") != std::string::npos;
    if (has("(U)") || has("(USA)") || has("(NA)"))
        return remote.find("(USA") != std::string::npos ||
               remote.find("(World") != std::string::npos;
    if (has("(E)") || has("(Europe)") || has("(EU)"))
        return remote.find("(Europe") != std::string::npos ||
               remote.find("(World") != std::string::npos;
    if (has("(G)")) return remote.find("(Germany") != std::string::npos;
    if (has("(F)")) return remote.find("(France") != std::string::npos;
    return true;
}

bool beginsWithExtendedTitle(const std::string &bare, const std::string &remote) {
    const size_t length = bare.find_last_not_of(" \t") + 1;
    if (length < 8 || remote.size() <= length ||
        strncasecmp(remote.c_str(), bare.c_str(), length) != 0) return false;
    const std::string suffix = remote.substr(length);
    return suffix.compare(0, 3, " - ") == 0 || suffix.compare(0, 5, " for ") == 0 ||
           suffix.compare(0, 2, ". ") == 0;
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

std::string LibretroIndex::exactKey(const std::string &title) {
    std::string out;
    bool space = false;

    for (char c : title) {
        if (c == ' ' || c == '\t') { space = !out.empty(); continue; }

        // The server's own substitution for what a filename cannot hold, applied to the
        // local title too so both sides land on the same key: "Bubble Bobble : Part 2"
        // is listed as "Bubble Bobble _ Part 2".
        if (std::strchr("&*/:`<>?\\|", c)) c = '_';
        if (c >= 'A' && c <= 'Z') c = char(c + 32);

        if (space) out.push_back(' ');
        space = false;
        out.push_back(c);
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
    byExact_.clear();

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
            byExact_.emplace(exactKey(name), name);

            const std::string key = normalise(name);
            if (key.empty()) continue;

            auto existing = byKey_.find(key);
            if (existing == byKey_.end()) byKey_[key] = name;
            else if (regionRank(name) < regionRank(existing->second) ||
                     (regionRank(name) == regionRank(existing->second) &&
                      specialEdition(existing->second) && !specialEdition(name)))
                existing->second = name;
        }
    }

    return !byKey_.empty();
}

bool LibretroIndex::open(const std::string &platform, Downloader &downloader,
                         const std::string &cacheDir) {
    error_.clear();
    platform_ = platform;
    byKey_.clear();
    byExact_.clear();

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

    return true;
}

std::string LibretroIndex::match(const std::string &title) const {
    const std::string key = normalise(title);
    const auto find = [&](const std::string &candidate) -> std::string {
        const auto hit = byKey_.find(candidate);
        return hit == byKey_.end() ? std::string() : hit->second;
    };

    // Preserve the strongest match. Most names need no special handling: brackets already
    // disappear in normalise(), and punctuation is ignored on both sides.
    std::string hit = find(key);
    if (!hit.empty()) return hit;

    // Some ROM sets store regional aliases in one filename, separated by " ~ ". Each side
    // is a complete title, so look it up on its own rather than concatenating the names.
    if (title.find(" ~ ") != std::string::npos) {
        size_t start = 0;
        std::string aliasHit;
        while (start < title.size()) {
            const size_t end = title.find(" ~ ", start);
            hit = find(normalise(title.substr(start, end - start)));
            if (!hit.empty()) aliasHit = hit;
            if (end == std::string::npos) break;
            start = end + 3;
        }
        if (!aliasHit.empty()) return aliasHit;
    }

    if (platform_ == "MAME") {
        // A few MRA descriptions use a different established MAME title. Preserve the
        // revision when the server has one; a generic lookup can pick a bootleg image for
        // an official set merely because both share the same normalised base name.
        if (key == "280zzzap") {
            hit = find("datsun280zzzap");
            if (!hit.empty()) return hit;
        }
        if (key == "battlezonerev2") {
            hit = matchExact("Battle Zone (rev 2)");
            if (!hit.empty()) return hit;
        }
        if (key == "cyberpoliceeswat") {
            const size_t suffix = title.find('(');
            if (suffix != std::string::npos) {
                hit = matchExact("E-Swat - Cyber Police " + title.substr(suffix));
                if (!hit.empty()) return hit;
            }
        }
    }

    // Many ROM sets omit the publisher's "Version" or "Edition" suffix. The libretro
    // catalog uses it for titles such as Pokemon - Emerald Version and Feuerrote Edition.
    hit = find(key + "version");
    if (!hit.empty()) return hit;
    hit = find(key + "edition");
    if (!hit.empty()) return hit;

    // No-Intro often writes a leading article at the front while the thumbnail catalog
    // files it at the end: "A Sound of Thunder" -> "Sound of Thunder, A".
    const size_t metadata = title.find_first_of("([");
    const std::string bare = title.substr(0, metadata);
    for (const char *article : {"A ", "An ", "The "}) {
        const size_t length = std::strlen(article);
        if (bare.size() > length && strncasecmp(bare.c_str(), article, length) == 0) {
            hit = find(normalise(bare.substr(length) + " " + bare.substr(0, length - 1)));
            if (!hit.empty()) return hit;
        }
    }

    // "and" and "&" are interchangeable in a few ROM-set labels. The latter vanishes
    // under normalise(), so remove only a whole "and" word as another exact-key probe.
    std::string withoutAnd = bare;
    for (size_t i = 0; i + 5 <= withoutAnd.size(); ++i) {
        if (strncasecmp(withoutAnd.c_str() + i, " and ", 5) == 0) {
            withoutAnd.erase(i, 4);
            break;
        }
    }
    if (withoutAnd != bare) {
        hit = find(normalise(withoutAnd));
        if (!hit.empty()) return hit;
    }

    // The catalog spells numbered sequels with Roman numerals in some series.
    std::string roman = bare;
    for (size_t i = 0; i < roman.size(); ++i) {
        if (i && roman[i - 1] != ' ' && roman[i - 1] != '-') continue;
        if (i + 1 < roman.size() && roman[i + 1] != ' ' && roman[i + 1] != '-') continue;
        const char *replacement = roman[i] == '2' ? "II" :
                                  roman[i] == '3' ? "III" :
                                  roman[i] == '4' ? "IV" : nullptr;
        if (!replacement) continue;
        roman.replace(i, 1, replacement);
        i += std::strlen(replacement) - 1;
    }
    if (roman != bare) {
        hit = find(normalise(roman));
        if (!hit.empty()) return hit;
    }

    // Thumbnail catalogs often file a leading article after the first title segment:
    // "The Legend of Zelda - The Minish Cap" -> "Legend of Zelda, The - ...".
    // Some local sets omit that article altogether ("Lord of the Rings").
    std::string articleTitle = bare;
    while (!articleTitle.empty() && articleTitle.back() == ' ') articleTitle.pop_back();
    const size_t divider = articleTitle.find(" - ");
    const size_t headEnd = divider == std::string::npos ? articleTitle.size() : divider;
    const size_t leading = strncasecmp(articleTitle.c_str(), "The ", 4) == 0 ? 4 :
                           strncasecmp(articleTitle.c_str(), "An ", 3) == 0 ? 3 :
                           strncasecmp(articleTitle.c_str(), "A ", 2) == 0 ? 2 : 0;
    if (leading && headEnd > leading) {
        const std::string article = articleTitle.substr(0, leading - 1);
        articleTitle = articleTitle.substr(leading, headEnd - leading) + ", " + article +
                       articleTitle.substr(headEnd);
    } else if (headEnd >= 12 && articleTitle.compare(headEnd - 5, 5, ", The") != 0) {
        articleTitle.insert(headEnd, ", The");
    }
    if (articleTitle != bare) {
        hit = find(normalise(articleTitle));
        if (!hit.empty()) return hit;
        if (divider == std::string::npos && platform_ != "MAME") {
            std::string extended;
            for (const auto &entry : byKey_) {
                if (!beginsWithExtendedTitle(articleTitle, entry.second) ||
                    specialEdition(entry.second) ||
                    !compatibleRegion(title, entry.second)) continue;
                if (!extended.empty()) { extended.clear(); break; }
                extended = entry.second;
            }
            if (!extended.empty()) return extended;
        }
    }

    if (platform_ == "Nintendo - Game Boy Advance") {
        // This ROM set prepends the publisher to many Disney games, while the catalog
        // files the same release under its on-box title.
        if (title.compare(0, 9, "Disney's ") == 0) {
            hit = find(normalise(title.substr(9)));
            if (!hit.empty()) return hit;
        }

        // Famicom Mini volumes are numbered 1-30 in the ROM set and 01-30 in the
        // thumbnail set. The volume number identifies the release even where the
        // Japanese title is spelled differently.
        const char *series = "Famicom Mini - Vol ";
        if (title.compare(0, std::strlen(series), series) == 0) {
            int volume = 0;
            if (std::sscanf(title.c_str() + std::strlen(series), "%d", &volume) == 1 &&
                volume >= 1 && volume <= 30 &&
                title.find(" - ", std::strlen(series)) != std::string::npos) {
                char numbered[32];
                std::snprintf(numbered, sizeof(numbered), "famicommini%02d", volume);
                std::string numberedHit;
                for (const auto &entry : byKey_) {
                    if (entry.first.compare(0, std::strlen(numbered), numbered) != 0 ||
                        entry.second.find("(Japan") == std::string::npos ||
                        specialEdition(entry.second)) continue;
                    if (!numberedHit.empty()) { numberedHit.clear(); break; }
                    numberedHit = entry.second;
                }
                if (!numberedHit.empty()) return numberedHit;
            }
        }

        // Old GBA Video dumps put the medium after the title; the thumbnail set puts it
        // before the title. Try only this named series, not arbitrary word deletion.
        std::string videoTitle = title;
        std::string lowerTitle = title;
        for (char &c : lowerTitle)
            if (c >= 'A' && c <= 'Z') c = char(c + 32);
        const size_t suffix = lowerTitle.find(" - gameboy advance video");
        if (suffix != std::string::npos) videoTitle.resize(suffix);
        const std::string videoKey = normalise(videoTitle);
        if (suffix != std::string::npos || videoKey.find("volume") != std::string::npos ||
            videoKey.find("collection") != std::string::npos) {
            hit = find("gameboyadvancevideo" + videoKey);
            if (!hit.empty()) return hit;
        }

        // These video cartridges use episode names on the server, while the ROM set
        // labels them only by collection and volume. Keep each alias tied to its
        // named series; the French TMNT cartridge has a different episode title.
        std::string videoAlias = key;
        const std::string medium = "gameboyadvancevideo";
        if (videoAlias.size() > medium.size() &&
            videoAlias.compare(videoAlias.size() - medium.size(), medium.size(), medium) == 0)
            videoAlias.resize(videoAlias.size() - medium.size());
        if (videoAlias == "nicktoonscollectionvolume3")
            hit = find("gameboyadvancevideonicktoonsvolume3");
        else if (videoAlias == "superrobotmonkeyteamvolume1")
            hit = find("gameboyadvancevideosuperrobotmonkeyteamhyperforcegovolume1");
        else if (videoAlias == "theadventuresofjimmyneutronvolume1")
            hit = find("gameboyadvancevideotheadventuresofjimmyneutronboygeniusvolume1");
        else if (videoAlias == "yugiohyugivsjoeyvolume1")
            hit = matchExact(title.find("(F)") != std::string::npos ?
                "Game Boy Advance Video - Yu-Gi-Oh! - Yugi vs. Joey (France)" :
                "Game Boy Advance Video - Yu-Gi-Oh! - Yugi vs. Joey (USA, Europe)");
        else if (videoAlias == "teenagemutantninjaturtlesvolume1")
            hit = find(title.find("(F)") != std::string::npos ?
                       "gameboyadvancevideoteenagemutantninjaturtlesledemenagement" :
                       "gameboyadvancevideoteenagemutantninjaturtlesthingschange");
        if (!hit.empty()) return hit;

        // A few GBA release lines use a series label absent from the server's title.
        // Retain the individual subtitle or volume so sequels cannot cross-match.
        if (key.compare(0, 10, "goldensun2") == 0) {
            hit = find("goldensun" + key.substr(10));
            if (!hit.empty()) return hit;
        }
        if (key.compare(0, 16, "hudsoncollection") == 0) {
            hit = find("hudsonbestcollection" + key.substr(16));
            if (!hit.empty()) return hit;
        }
        std::string versionTitle = bare;
        while (!versionTitle.empty() && versionTitle.back() == ' ') versionTitle.pop_back();
        if (versionTitle.size() >= 8 &&
            versionTitle.compare(versionTitle.size() - 8, 8, " Version") == 0 &&
            key.size() > 7 && key.compare(key.size() - 7, 7, "version") == 0) {
            hit = find(key.substr(0, key.size() - 7));
            if (!hit.empty()) return hit;
        }

        // The GBA compilation line is titled "Classic NES Series" on the server.
        if (key.compare(0, 10, "classicnes") == 0) {
            hit = find("classicnesseries" + key.substr(10));
            if (!hit.empty()) return hit;
        }

        // This ROM set drops the e in the German title; the server uses "Blattgruene".
        if (key == "pokemonblattgrune") return find("pokemonblattgrueneedition");

        // Libretro puts the edition word before the title in French, Italian and Spanish.
        // Limit this fallback to Pokemon so it cannot cross-match unrelated games.
        if (key.compare(0, 7, "pokemon") == 0) {
            const std::string rest = key.substr(7);
            for (const char *prefix : {"pokemonversion", "pokemonversione", "pokemonedicion"}) {
                hit = find(std::string(prefix) + rest);
                if (!hit.empty()) return hit;
            }
        }
    }

    // Some dumps stop at the main title while the catalog appends a subtitle. Accept only
    // one possible continuation of a substantial title, with the same numbers and no
    // special-edition artwork. Several continuations mean different games or editions.
    // "Pirates of the Caribbean" is a series: the catalog's only continuation here is
    // "Dead Man's Chest", which is a different release from the unsuffixed GBA game.
    if (platform_ != "MAME" && key.size() >= 14 && key != "piratesofthecaribbean") {
        std::string continuation;
        for (const auto &entry : byKey_) {
            const std::string &candidate = entry.first;
            if (candidate.size() <= key.size() || candidate.size() - key.size() > 20 ||
                candidate.compare(0, key.size(), key) != 0 ||
                digitsOf(candidate) != digitsOf(key) || specialEdition(entry.second)) continue;
            if (!continuation.empty()) { continuation.clear(); break; }
            continuation = entry.second;
        }
        if (!continuation.empty()) return continuation;
    }

    // Long catalog subtitles can exceed the short continuation limit above. Require
    // a visible title boundary and the same region, then accept only one title.
    if (platform_ != "MAME" && key != "piratesofthecaribbean") {
        std::string extended;
        for (const auto &entry : byKey_) {
            if (!beginsWithExtendedTitle(bare, entry.second) ||
                digitsOf(entry.first) != digitsOf(key) ||
                specialEdition(entry.second) || !compatibleRegion(title, entry.second)) continue;
            if (!extended.empty()) { extended.clear(); break; }
            extended = entry.second;
        }
        if (!extended.empty()) return extended;
    }

    // A catalog subtitle may be much longer than the dump title. This last prefix
    // probe still requires one and only one title across *all* continuation lengths;
    // looking only at long candidates could hide a different short-titled sequel.
    if (platform_ != "MAME" && key.size() >= 12 &&
        key != "piratesofthecaribbean" && key != "chaojimaliou") {
        std::string extended;
        for (const auto &entry : byKey_) {
            const std::string &candidate = entry.first;
            if (candidate.size() <= key.size() || candidate.size() - key.size() > 75 ||
                candidate.compare(0, key.size(), key) != 0 ||
                digitsOf(candidate) != digitsOf(key) || specialEdition(entry.second)) continue;
            if (!extended.empty()) { extended.clear(); break; }
            extended = entry.second;
        }
        if (!extended.empty() && compatibleRegion(title, extended)) return extended;
    }

    // Some catalogs insert a series label between an unchanged main title and
    // subtitle, for example "Yu Yu Hakusho - Ghostfiles - Spirit Detective".
    // Both ends and every digit must agree, and the candidate must be unique.
    if (platform_ != "MAME") {
        const size_t split = bare.find(" - ");
        if (split != std::string::npos) {
            const std::string head = normalise(bare.substr(0, split));
            const std::string tail = normalise(bare.substr(split + 3));
            if (head.size() >= 6 && tail.size() >= 8) {
                std::string inserted;
                for (const auto &entry : byKey_) {
                    const std::string &candidate = entry.first;
                    if (candidate.size() <= head.size() + tail.size() ||
                        candidate.size() - head.size() - tail.size() > 30 ||
                        candidate.compare(0, head.size(), head) != 0 ||
                        candidate.compare(candidate.size() - tail.size(), tail.size(), tail) != 0 ||
                        digitsOf(candidate) != digitsOf(key) || specialEdition(entry.second))
                        continue;
                    if (!inserted.empty()) { inserted.clear(); break; }
                    inserted = entry.second;
                }
                if (!inserted.empty()) return inserted;
            }
        }
    }

    // A single small spelling difference is common in old dump names (e.g. "Daioh" vs
    // "Daiou"). Never change the number in a title, and require a unique nearest name;
    // otherwise a sequel or another revision could silently get the wrong cover.
    if (platform_ != "MAME" && key.size() >= 9) {
        const int limit = key.size() >= 25 ? 3 : key.size() >= 13 ? 2 : 1;
        const std::string digits = digitsOf(key);
        int best = limit + 1, second = limit + 1;
        std::string closest;
        for (const auto &entry : byKey_) {
            const std::string &candidate = entry.first;
            if (candidate.size() < 2 || candidate.compare(0, 2, key, 0, 2) != 0 ||
                std::abs(int(candidate.size()) - int(key.size())) > limit ||
                digitsOf(candidate) != digits ||
                (specialEdition(entry.second) && title.find("Demo") == std::string::npos &&
                 title.find("Beta") == std::string::npos &&
                 title.find("Proto") == std::string::npos &&
                 title.find("Hack") == std::string::npos)) continue;
            const int distance = editDistanceWithin(key, candidate, limit);
            if (distance < best) {
                second = best;
                best = distance;
                closest = entry.second;
            } else if (distance < second) {
                second = distance;
            }
        }
        if (best <= limit && second - best >= 2) return closest;
    }

    return std::string();
}

std::string LibretroIndex::matchExact(const std::string &title) const {
    const auto found = byExact_.find(exactKey(title));
    return found == byExact_.end() ? std::string() : found->second;
}
