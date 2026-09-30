// Host-side checks for LibretroIndex's arcade lookup: a cover is chosen by the whole title,
// brackets included, because in MAME's set every ROM revision has its own — see ARCADE.md,
// "Scraper and box art", for the real wrong-cover bug the loose match alone would have shipped.
//
// Build and run:  make -C tests

#include <cstdio>
#include <fstream>
#include <string>

#include "../src/LibretroIndex.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

} // namespace

int main() {
    // The server's real listing for these, percent-encoded, in its real (alphabetical) order.
    const std::string listing = "/tmp/mister-gui-libretro-test.html";
    {
        std::ofstream out(listing);
        out << "<a href=\"Killer%20Instinct%20(proto%20v4.7).png\">x</a>\n"
               "<a href=\"Killer%20Instinct%20(v1.0).png\">x</a>\n"
               "<a href=\"Killer%20Instinct%20(v1.5d).png\">x</a>\n"
               "<a href=\"Killer%20Instinct.png\">x</a>\n"
               "<a href=\"3%20Count%20Bout%20_%20Fire%20Suplex.png\">x</a>\n"
               "<a href=\"10%20Yard%20Fight%20(Vs.%20version%20World,%2011_05_84).png\">x</a>\n"
               "<a href=\"Ghosts%27n%20Goblins%20(World%2C%20rev%20A).png\">x</a>\n";
    }

    LibretroIndex index;
    check(index.parse(listing), "a listing parses");

    // The bug: the loose match drops every bracket, so all four are one title and the first
    // one listed — a prototype — wins.
    check(index.match("Killer Instinct (v1.5d)") == "Killer Instinct (proto v4.7)",
          "the loose match really does pick the wrong revision");

    check(index.matchExact("Killer Instinct (v1.5d)") == "Killer Instinct (v1.5d)",
          "the exact match picks the revision asked for");
    check(index.matchExact("Killer Instinct (v1.0)") == "Killer Instinct (v1.0)",
          "and another one");
    check(index.matchExact("Killer Instinct") == "Killer Instinct",
          "a title with no revision finds the plain one");
    check(index.matchExact("killer  instinct (V1.5D) ") == "Killer Instinct (v1.5d)",
          "case and stray spaces do not matter");
    check(index.matchExact("Killer Instinct (v9.9)").empty(),
          "a revision the server lacks finds nothing, to fall back on the loose match");

    check(index.matchExact("3 Count Bout : Fire Suplex") == "3 Count Bout _ Fire Suplex",
          "a character the server replaced still lines up");
    check(index.matchExact("Ghosts'n Goblins (World, rev A)") == "Ghosts'n Goblins (World, rev A)",
          "an apostrophe and comma survive percent-decoding");
    check(index.matchExact("10 Yard Fight (Vs. version World, 11/05/84)") ==
              "10 Yard Fight (Vs. version World, 11_05_84)",
          "slashes in a date line up with the server's underscores");

    check(LibretroIndex::platformFor("Arcade") == "MAME", "Arcade maps to the MAME platform");

    std::remove(listing.c_str());
    std::printf("\n%s\n", failures ? "FAILURES" : "all good");
    return failures ? 1 : 0;
}
