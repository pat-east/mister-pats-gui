// Host-side audit of scrape-misses.txt against the same LibretroIndex matcher the GUI uses.
// Build: c++ -std=gnu++14 -O2 -Isrc tools/scrape_misses_audit.cpp \
//        src/LibretroIndex.cpp src/Downloader.cpp src/SystemCatalog.cpp \
//        src/CoreIndex.cpp -o build/scrape-misses-audit
// Run:   build/scrape-misses-audit scrape-misses.txt /tmp/mister-scrape-index \
//        /tmp/mister-scrape-audit.tsv

#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "Downloader.h"
#include "LibretroIndex.h"
#include "SystemCatalog.h"

namespace {

struct NameKey { const char *name; const char *key; };
const NameKey kNames[] = {
    {"Arcade", "Arcade"}, {"Atari 2600", "Atari2600"},
    {"Atari 800", "ATARI800"}, {"Atari Jaguar", "Jaguar"},
    {"Atari Lynx", "AtariLynx"},
    {"Commodore 64", "C64"}, {"Game Boy", "GAMEBOY"},
    {"Game Boy Advance", "GBA"}, {"Game Gear", "GameGear"},
    {"Genesis 32X", "S32X"}, {"Intellivision", "Intellivision"},
    {"Master System", "SMS"}, {"Mega Drive", "MegaDrive"},
    {"MSX1", "MSX1"}, {"NES", "NES"},
    {"Nintendo 64", "N64"}, {"PC (486)", "AO486"},
    {"PlayStation", "PSX"}, {"SCV", "SCV"},
    {"Sharp X68000", "X68000"}, {"SNES", "SNES"},
    {"TurboGrafx-16", "TGFX16"}, {"VTech CreatiVision", "CreatiVision"},
    {"ZX Spectrum Next", "ZXNext"},
};

std::string keyFor(const std::string &name) {
    for (const NameKey &entry : kNames)
        if (name == entry.name) return entry.key;
    return name;
}

struct Entry { std::string system, title; };
struct Count { size_t total = 0, matched = 0, excluded = 0, unsupported = 0; };

} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s misses.txt cache-dir results.tsv\n", argv[0]);
        return 2;
    }

    std::ifstream input(argv[1]);
    if (!input) { std::perror(argv[1]); return 1; }
    std::vector<Entry> entries;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        entries.push_back({line.substr(0, tab), line.substr(tab + 1)});
    }

    mkdir(argv[2], 0777);
    std::ofstream output(argv[3]);
    if (!output) { std::perror(argv[3]); return 1; }
    output << "status\tsystem\ttitle\tremote\n";

    std::map<std::string, std::vector<const Entry *>> groups;
    for (const Entry &entry : entries) groups[entry.system].push_back(&entry);

    Downloader downloader;
    Count all;
    for (const auto &group : groups) {
        const std::string &system = group.first;
        const std::string key = keyFor(system);
        const std::string platform = LibretroIndex::platformFor(key);
        LibretroIndex index;
        const bool available = !platform.empty() && index.open(platform, downloader, argv[2]);
        if (!platform.empty() && !available) {
            std::fprintf(stderr, "%s: %s\n", system.c_str(), index.lastError().c_str());
            return 1; // A failed index is missing evidence, not a missing cover.
        }

        Count count;
        for (const Entry *entry : group.second) {
            ++count.total;
            const bool excluded = SystemCatalog::isSystemFile(key, entry->title);
            std::string remote;
            if (!excluded && available) {
                if (key == "Arcade") remote = index.matchExact(entry->title);
                if (remote.empty()) remote = index.match(entry->title);
            }

            const char *status = excluded ? "excluded" : !available ? "unsupported" :
                                 remote.empty() ? "missing" : "matched";
            if (excluded) ++count.excluded;
            else if (!available) ++count.unsupported;
            else if (!remote.empty()) ++count.matched;
            output << status << '\t' << system << '\t' << entry->title << '\t' << remote << '\n';
        }
        all.total += count.total;
        all.excluded += count.excluded;
        all.unsupported += count.unsupported;
        all.matched += count.matched;
        std::printf("%-24s %4zu total  %4zu matched  %3zu excluded  %3zu unsupported\n",
                    system.c_str(), count.total, count.matched, count.excluded,
                    count.unsupported);
    }

    std::printf("TOTAL %zu entries, %zu matched, %zu excluded, %zu unsupported, %zu still missing\n",
                all.total, all.matched, all.excluded, all.unsupported,
                all.total - all.matched - all.excluded - all.unsupported);
    return 0;
}
