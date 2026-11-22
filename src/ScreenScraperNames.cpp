#include "ScreenScraperNames.h"

#include <cctype>
#include <cstring>

namespace ScreenScraperNames {
namespace {

// Folds the common accented Latin letters (U+00C0..U+017F) to ASCII; anything else outside
// ASCII is dropped.
char foldLatin(unsigned cp) {
    static const char *const table =
        // U+00C0 .. U+00FF
        "AAAAAAACEEEEIIII" "DNOOOOO*OUUUUYTs" "aaaaaaaceeeeiiii" "dnooooo/ouuuuyty";
    if (cp >= 0xC0 && cp <= 0xFF) {
        const char c = table[cp - 0xC0];
        return (c == '*' || c == '/') ? 0 : c;
    }
    if (cp >= 0x100 && cp <= 0x17F) {
        static const char *const ext =
            // U+0100 .. U+017F, one entry per code point
            "AaAaAaCcCcCcCcDd" "DdEeEeEeEeEeGgGg" "GgGgHhHhIiIiIiIi" "IiJjJjKkkLlLlLlL"
            "lLlNnNnNnnNnOoOo" "OoOoRrRrRrSsSsSs" "SsTtTtTtUuUuUuUu" "UuUuWwYyYZzZzZzs";
        return ext[cp - 0x100];
    }
    return 0;
}

// Reads the text as UTF-8 and returns it folded to lower-case ASCII, keeping every character
// that survives so punctuation can be judged by the caller.
std::string foldAscii(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) { out += char(std::tolower(c)); ++i; continue; }
        unsigned cp = 0;
        size_t n = 0;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 3; }
        else { ++i; continue; }
        if (i + n >= text.size()) break;   // truncated sequence at the end
        bool ok = true;
        for (size_t k = 1; k <= n; ++k) {
            const unsigned char next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (next & 0x3F);
        }
        i += ok ? n + 1 : 1;
        if (!ok) continue;
        const char folded = foldLatin(cp);
        if (folded) out += char(std::tolower(static_cast<unsigned char>(folded)));
    }
    return out;
}

// Removes "(...)" and "[...]" groups (nested ones included).
std::string stripBrackets(const std::string &text) {
    std::string out;
    int depthRound = 0, depthSquare = 0;
    for (char c : text) {
        if (c == '(') { ++depthRound; continue; }
        if (c == ')') { if (depthRound > 0) --depthRound; continue; }
        if (c == '[') { ++depthSquare; continue; }
        if (c == ']') { if (depthSquare > 0) --depthSquare; continue; }
        if (depthRound == 0 && depthSquare == 0) out += c;
    }
    return out;
}

std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// "Name, The" -> "The Name" (also A and An), case-insensitive on the article.
std::string moveArticle(const std::string &text) {
    const size_t comma = text.rfind(',');
    if (comma == std::string::npos) return text;
    const std::string tail = trim(text.substr(comma + 1));
    std::string lower;
    for (char c : tail) lower += char(std::tolower(static_cast<unsigned char>(c)));
    if (lower != "the" && lower != "a" && lower != "an") return text;
    return tail + " " + trim(text.substr(0, comma));
}

// A real file extension is one to four letters or digits after the last dot, and the dot must
// not be the first character. "Mario Vs. Donkey Kong" has a dot but no extension.
std::string stripExtension(const std::string &name) {
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return name;
    const size_t length = name.size() - dot - 1;
    if (length < 1 || length > 4) return name;
    for (size_t i = dot + 1; i < name.size(); ++i)
        if (!std::isalnum(static_cast<unsigned char>(name[i]))) return name;
    // "Vs." style: a dot followed by a space was never an extension; here nothing follows the dot.
    // Names such as "Super Mario Bros. 3" have a space after the dot, which fails the check above.
    static const char *const known[] = {"zip", "7z", "nes", "sfc", "smc", "gb", "gbc", "gba", "gg",
                                        "sms", "md", "gen", "bin", "iso", "cue", "chd", "z64",
                                        "n64", "v64", "a26", "lnx", "j64", "pce", "sg", "mra", "rom"};
    std::string ext = name.substr(dot + 1);
    for (char &c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
    for (const char *k : known) if (ext == k) return name.substr(0, dot);
    return name;
}

}  // namespace

std::string searchTitle(const std::string &gameName) {
    std::string text = trim(stripBrackets(stripExtension(gameName)));
    text = moveArticle(text);
    // Collapse runs of spaces left behind by removed tags.
    std::string out;
    bool space = false;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) { space = true; continue; }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

std::string titleKey(const std::string &name) {
    const std::string folded = foldAscii(moveArticle(trim(stripBrackets(stripExtension(name)))));
    std::string out;
    for (size_t i = 0; i < folded.size(); ++i) {
        const char c = folded[i];
        if (c == '&') { out += "and"; continue; }
        if (std::isalnum(static_cast<unsigned char>(c))) out += c;
    }
    return out;
}

int systemId(const std::string &systemKey) {
    struct Entry { const char *key; int id; };
    // Ids from ScreenScraper's systemesListe.php (checked 2026-11-21). Compared without case.
    static const Entry table[] = {
        {"snes", 4}, {"nes", 3}, {"megadrive", 1}, {"genesis", 1}, {"sms", 2}, {"gameboy", 9},
        {"gb", 9}, {"gbc", 10}, {"gba", 12}, {"gamegear", 21}, {"n64", 14}, {"atari2600", 26},
        {"jaguar", 27}, {"atarilynx", 28}, {"astrocade", 44}, {"psx", 57}, {"saturn", 22},
        {"megacd", 20}, {"sega32x", 19}, {"s32x", 19}, {"tgfx16", 31}, {"pcengine", 31},
        {"tgfx16-cd", 114}, {"pcenginecd", 114}, {"supergrafx", 105}, {"sg1000", 109},
        {"coleco", 48}, {"colecovision", 48}, {"intellivision", 115}, {"atari5200", 40},
        {"atari7800", 41}, {"vectrex", 102}, {"neogeo", 142}, {"neogeo-cd", 70},
        {"wonderswan", 45}, {"wonderswancolor", 46}, {"ngp", 25}, {"ngpc", 82},
        {"odyssey2", 104}, {"channelf", 80}, {"gamate", 266}, {"supervision", 207},
        {"megaduck", 90}, {"pokemini", 211}, {"virtualboy", 11}, {"3do", 29}, {"cdi", 133},
        {"pico", 250},
    };
    std::string lower;
    for (char c : systemKey) lower += char(std::tolower(static_cast<unsigned char>(c)));
    for (const Entry &e : table) if (lower == e.key) return e.id;
    return 0;
}

std::string fileSafe(const std::string &text) {
    std::string out;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') out += c;
        else out += '_';
    }
    return out.empty() ? std::string("_") : out;
}

}  // namespace ScreenScraperNames
