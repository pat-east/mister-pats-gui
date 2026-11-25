#include "MraFile.h"

#include "Archive.h"

#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace {

bool directoryExists(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool fileExists(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string readWholeFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::string();
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// A real .mra routinely has a comment right before the block it documents (see docs/ARCADE.md's
// own Killer Instinct example) — stripped first so a naive tag scan cannot be confused by one.
void stripComments(std::string &xml) {
    size_t pos = 0;
    while ((pos = xml.find("<!--", pos)) != std::string::npos) {
        const size_t end = xml.find("-->", pos + 4);
        if (end == std::string::npos) { xml.erase(pos); break; }
        xml.erase(pos, end + 3 - pos);
    }
}

std::string trim(const std::string &s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string decodeEntities(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&') {
            if (s.compare(i, 5, "&amp;") == 0) { out += '&'; i += 5; continue; }
            if (s.compare(i, 4, "&lt;") == 0) { out += '<'; i += 4; continue; }
            if (s.compare(i, 4, "&gt;") == 0) { out += '>'; i += 4; continue; }
            if (s.compare(i, 6, "&quot;") == 0) { out += '"'; i += 6; continue; }
            if (s.compare(i, 6, "&apos;") == 0) { out += '\''; i += 6; continue; }
        }
        out += s[i];
        ++i;
    }
    return out;
}

bool isNameBoundary(char c) {
    return c == '>' || c == '/' || c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Content of the first <tag>...</tag> found, or empty. Skips a tag that merely starts with the
// same letters (a search for "rom" must not match "<roms") by requiring the character right
// after the name to be a real tag-name boundary, and skips a self-closing "<tag/>" — none of
// the fields this parser reads this way are ever written self-closed in practice.
std::string extractTag(const std::string &xml, const char *tag) {
    const std::string open = std::string("<") + tag;
    size_t pos = 0;
    while ((pos = xml.find(open, pos)) != std::string::npos) {
        const size_t afterName = pos + open.size();
        if (afterName >= xml.size() || !isNameBoundary(xml[afterName])) { ++pos; continue; }

        const size_t gt = xml.find('>', afterName);
        if (gt == std::string::npos) return std::string();
        if (gt > 0 && xml[gt - 1] == '/') { pos = gt + 1; continue; }

        const std::string close = std::string("</") + tag + ">";
        const size_t end = xml.find(close, gt + 1);
        if (end == std::string::npos) return std::string();
        return decodeEntities(trim(xml.substr(gt + 1, end - gt - 1)));
    }
    return std::string();
}

// Attribute values from a tag's opening "<tag ...>" text. Every real .mra this has been
// checked against uses double quotes; single quotes are accepted too, since nothing about the
// format actually forbids them.
std::string attrValue(const std::string &tagText, const char *attr) {
    const std::string needle = std::string(attr) + "=";
    size_t pos = tagText.find(needle);
    while (pos != std::string::npos) {
        const bool boundedLeft = pos == 0 || isNameBoundary(tagText[pos - 1]);
        if (boundedLeft) {
            const size_t valuePos = pos + needle.size();
            if (valuePos < tagText.size() &&
                (tagText[valuePos] == '"' || tagText[valuePos] == '\'')) {
                const char quote = tagText[valuePos];
                const size_t end = tagText.find(quote, valuePos + 1);
                if (end != std::string::npos)
                    return decodeEntities(tagText.substr(valuePos + 1, end - valuePos - 1));
            }
        }
        pos = tagText.find(needle, pos + 1);
    }
    return std::string();
}

uint32_t parseHexCrc(const std::string &s, bool &ok) {
    ok = !s.empty();
    uint32_t value = 0;
    for (char c : s) {
        value <<= 4;
        if (c >= '0' && c <= '9') value |= uint32_t(c - '0');
        else if (c >= 'a' && c <= 'f') value |= uint32_t(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= uint32_t(c - 'A' + 10);
        else { ok = false; break; }
    }
    return ok ? value : 0;
}

const char *kAllMountCandidates[] = {
    "/media/fat",  "/media/usb0", "/media/usb1", "/media/usb2", "/media/usb3",
    "/media/usb4", "/media/usb5", "/media/usb6", "/media/usb7",
};

// Every alternate a "|"-separated zip attribute names, in order.
std::vector<std::string> splitZipAlternates(const std::string &value) {
    std::vector<std::string> out;
    std::istringstream stream(value);
    std::string piece;
    while (std::getline(stream, piece, '|'))
        if (!piece.empty()) out.push_back(piece);
    return out;
}

// First alternate that actually exists under `mameDir`, or empty.
std::string resolveZipFile(const std::string &mameDir, const std::string &zipAttr) {
    for (const std::string &name : splitZipAlternates(zipAttr)) {
        const std::string full = mameDir + "/" + name;
        if (fileExists(full)) return full;
    }
    return std::string();
}

const ArchiveEntry *findByName(const std::vector<ArchiveEntry> &entries, const std::string &name) {
    for (const ArchiveEntry &entry : entries)
        if (entry.name == name) return &entry;
    for (const ArchiveEntry &entry : entries)
        if (strcasecmp(entry.name.c_str(), name.c_str()) == 0) return &entry;
    return nullptr;
}

} // namespace

bool MraFile::parse(const std::string &path, ArcadeEntry &entry) {
    std::string xml = readWholeFile(path);
    if (xml.empty()) return false;
    stripComments(xml);

    entry = ArcadeEntry();
    entry.mraPath = path;

    // Same derivation as MiSTer's own set_arcade_root (support/arcade/mra_loader.cpp): the
    // volume is everything before the first "/_" segment (e.g. "/_Arcade"), not just "drop the
    // filename" — a library organised into manufacturer subfolders under _Arcade still resolves
    // to the right volume this way.
    const size_t underscoreSlash = path.find("/_");
    if (underscoreSlash != std::string::npos) {
        entry.root = path.substr(0, underscoreSlash);
    } else {
        const size_t slash = path.find_last_of('/');
        if (slash != std::string::npos) entry.root = path.substr(0, slash);
    }

    // Confirmed on real hardware, against a real ~6,400-file arcade library: three different
    // tags are all in real, current use for a game's title, apparently depending on which era
    // or toolchain generated the .mra. <name> and <description> were both found (see MraFile.h);
    // a third, <n> — a bare, single-letter abbreviation, e.g. <n>Vanguard</n> or
    // <n>Elevator Action (BA3, 4 PCB version, 1.1)</n> — turned up in a full sweep of one
    // library's top-level _Arcade folder once the first two still left a real (not
    // hypothetical) share of games unnamed. Only ever one of the three in any file checked, so
    // a plain, ordered fallback is enough. extractTag's own tag-boundary check is what keeps
    // this safe: searching for "n" cannot accidentally match "<name" or "<nvram", since the
    // character right after "<n" is neither '>' nor whitespace nor '/' for either of those.
    entry.name = extractTag(xml, "name");
    if (entry.name.empty()) entry.name = extractTag(xml, "description");
    if (entry.name.empty()) entry.name = extractTag(xml, "n");
    entry.year = extractTag(xml, "year");
    entry.manufacturer = extractTag(xml, "manufacturer");
    entry.category = extractTag(xml, "category");
    entry.rbf = extractTag(xml, "rbf");

    size_t pos = 0;
    while ((pos = xml.find("<rom", pos)) != std::string::npos) {
        const size_t afterName = pos + 4;
        if (afterName >= xml.size() || !isNameBoundary(xml[afterName])) { ++pos; continue; }

        const size_t gt = xml.find('>', pos);
        if (gt == std::string::npos) break;
        const std::string openTag = xml.substr(pos, gt - pos + 1);
        const bool selfClosed = gt > 0 && xml[gt - 1] == '/';

        std::string body;
        size_t nextPos = gt + 1;
        if (!selfClosed) {
            const size_t close = xml.find("</rom>", gt + 1);
            if (close == std::string::npos) break;
            body = xml.substr(gt + 1, close - gt - 1);
            nextPos = close + 6;
        }
        pos = nextPos;

        const std::string zip = attrValue(openTag, "zip");
        if (zip.empty()) continue;  // e.g. the index="0" game selector: no external file at all

        ArcadeRomBlock block;
        block.zip = zip;

        const std::string indexStr = attrValue(openTag, "index");
        bool indexOk = !indexStr.empty();
        for (char c : indexStr)
            if (c < '0' || c > '9') { indexOk = false; break; }
        block.index = indexOk ? std::atoi(indexStr.c_str()) : 0;

        size_t partPos = 0;
        while ((partPos = body.find("<part", partPos)) != std::string::npos) {
            const size_t partAfter = partPos + 5;
            if (partAfter < body.size() && !isNameBoundary(body[partAfter])) {
                ++partPos;
                continue;
            }
            const size_t partGt = body.find('>', partPos);
            if (partGt == std::string::npos) break;
            const std::string partTag = body.substr(partPos, partGt - partPos + 1);
            partPos = partGt + 1;

            const std::string partName = attrValue(partTag, "name");
            if (partName.empty()) continue;  // a padding/filler part carries no name to check

            ArcadeRomPart part;
            part.name = partName;
            part.zip = attrValue(partTag, "zip");
            part.crc = parseHexCrc(attrValue(partTag, "crc"), part.hasCrc);
            block.parts.push_back(std::move(part));
        }

        // Kept even with zero named parts: a real block seen on hardware (a filler/pattern
        // region, e.g. Decathlete's "decathlt.zip") has nothing to check a CRC against but is
        // still a real file this game needs — dropping it here would mean never verifying that
        // zip exists at all, silently reporting a missing one as present.
        entry.romBlocks.push_back(std::move(block));
    }

    return true;
}

void MraFile::resolveCore(ArcadeEntry &entry) {
    entry.coreFound = false;
    entry.coreFile.clear();
    if (entry.root.empty() || entry.rbf.empty()) return;

    const std::string coresDir = entry.root + "/_Arcade/cores";
    DIR *dir = opendir(coresDir.c_str());
    if (!dir) return;

    // get_rbf tries two spellings and keeps the best of both together: "Arcade-<rbf>" and the
    // bare "<rbf>". Most cores in the wild use the second ("Alpha68k_20221223.rbf"); only a few
    // carry the "Arcade-" prefix. Looking for the prefixed form alone finds almost nothing.
    const std::string prefixes[] = {"Arcade-" + entry.rbf, entry.rbf};
    std::string best;

    while (dirent *de = readdir(dir)) {
        const std::string name = de->d_name;
        if (name.size() < 5) continue;
        if (strncasecmp(name.c_str() + name.size() - 4, ".rbf", 4) != 0) continue;

        for (const std::string &prefix : prefixes) {
            if (name.size() < prefix.size() + 4) continue;  // no room for a boundary char + ".rbf"
            if (strncasecmp(name.c_str(), prefix.c_str(), prefix.size()) != 0) continue;

            // The character after the name must end it, so "KillerInstinct" does not claim
            // "KillerInstinct2_....rbf".
            const char boundary = name[prefix.size()];
            if (boundary != '.' && boundary != '_') continue;

            // Matches MiSTer's own get_rbf: an ordinary string compare, so a dated release
            // ("..._20260924.rbf") reliably sorts after an undated copy of the same core.
            if (best.empty() || name > best) best = name;
        }
    }
    closedir(dir);

    if (!best.empty()) {
        entry.coreFound = true;
        entry.coreFile = best;
    }
}

void MraFile::resolveRom(ArcadeEntry &entry, const std::vector<std::string> &gameSearchRoots) {
    entry.romFound = false;
    entry.romCrcOk = false;
    entry.romName.clear();

    // Descriptive regardless of whether anything is actually found — the "what does this game
    // want" column stays informative even for a completely missing set.
    {
        std::vector<std::string> zipNames;
        for (const ArcadeRomBlock &block : entry.romBlocks) {
            bool already = false;
            for (const std::string &z : zipNames)
                if (z == block.zip) { already = true; break; }
            if (!already) zipNames.push_back(block.zip);
        }
        for (size_t i = 0; i < zipNames.size(); ++i) {
            if (i) entry.romName += ", ";
            entry.romName += zipNames[i];
        }
    }

    // No zip-backed rom block at all (rare — an NVRAM-only or pure-selector .mra) means there
    // is nothing to check, so neither a hard "found" nor a hard "missing" claim is honest.
    if (entry.romBlocks.empty()) return;

    std::string mameDir;
    for (const std::string &root : gameSearchRoots) {
        const std::string candidate = root + "/games/mame";
        if (directoryExists(candidate)) { mameDir = candidate; break; }
    }
    if (mameDir.empty() && !entry.root.empty()) {
        // MiSTer's own fallback when no "games/mame" exists anywhere: the legacy, bare "mame"
        // folder directly on the .mra's own volume. Simplified from the real search (which also
        // tries a bare "<root>/mame" on every volume before falling back this far) — that
        // legacy layout is rare enough on a modern install that the extra per-root check was
        // judged not worth it; see docs/ARCADE.md.
        const std::string fallback = entry.root + "/mame";
        if (directoryExists(fallback)) mameDir = fallback;
    }
    if (mameDir.empty()) return;  // nowhere to even look; found/crcOk stay false

    bool allFound = true;
    bool allCrcOk = true;

    for (const ArcadeRomBlock &block : entry.romBlocks) {
        const std::string resolvedZip = resolveZipFile(mameDir, block.zip);
        if (resolvedZip.empty()) {
            allFound = false;
            allCrcOk = false;
            continue;
        }

        const std::vector<ArchiveEntry> blockEntries = Archive::list(resolvedZip);

        for (const ArcadeRomPart &part : block.parts) {
            const std::vector<ArchiveEntry> *entries = &blockEntries;
            std::vector<ArchiveEntry> overrideEntries;

            if (!part.zip.empty() && part.zip != block.zip) {
                const std::string overrideZip = resolveZipFile(mameDir, part.zip);
                if (overrideZip.empty()) {
                    allFound = false;
                    allCrcOk = false;
                    continue;
                }
                overrideEntries = Archive::list(overrideZip);
                entries = &overrideEntries;
            }

            const ArchiveEntry *match = findByName(*entries, part.name);
            if (!match) { allCrcOk = false; continue; }
            if (part.hasCrc && match->crc32 != part.crc) allCrcOk = false;
        }
    }

    entry.romFound = allFound;
    entry.romCrcOk = allFound && allCrcOk;
}

std::vector<std::string> MraFile::misterGameSearchOrder(
    const std::vector<std::string> &mountedRoots) {
    std::vector<std::string> out;

    // MiSTer's own search (file_io.cpp, findPrefixDir) checks exactly six USB slots, usb0
    // through usb5, in ascending order, before ever looking at the SD card.
    for (int i = 0; i <= 5; ++i) {
        const std::string candidate = "/media/usb" + std::to_string(i);
        for (const std::string &root : mountedRoots)
            if (root == candidate) { out.push_back(root); break; }
    }
    for (const std::string &root : mountedRoots)
        if (root == "/media/fat") { out.push_back(root); break; }

    return out;
}

std::vector<std::string> MraFile::mountedRoots() {
    std::vector<std::string> out;
    for (const char *candidate : kAllMountCandidates)
        if (directoryExists(candidate)) out.push_back(candidate);
    return out;
}
