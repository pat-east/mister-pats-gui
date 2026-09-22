// Host-side checks for letter navigation.
//
// The rules are easy to state and easy to get subtly wrong: forwards lands on the first
// entry of the next letter, backwards lands on the first entry of the current letter and
// only steps further back once already there. Both must terminate at the ends of the list.
//
// Build and run:  make -f tests/Makefile

#include <cstdio>
#include <string>
#include <vector>

#include "../src/Alphabet.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

std::vector<std::string> names = {
    "007 Racing",          // 0  #
    "1080 Snowboarding",   // 1  #
    "Aladdin",             // 2  A
    "Ape Escape",          // 3  A
    "Bushido Blade",       // 4  B
    "Crash Bandicoot",     // 5  C
    "Doom",                // 6  D
    "Zelda",               // 7  Z
};

int jump(int cursor, int direction) {
    return Alphabet::jump(cursor, int(names.size()), direction,
                          [](int i) -> const std::string & { return names[size_t(i)]; });
}

} // namespace

int main() {
    check(Alphabet::initial("Aladdin") == 'A', "initial letter of a title");
    check(Alphabet::initial("aladdin") == 'A', "lower case counts the same");
    check(Alphabet::initial("007 Racing") == '#', "digits collect under #");
    check(Alphabet::initial("[BIOS] Doom") == '#', "leading punctuation counts as #");
    check(Alphabet::initial("") == '#', "empty title falls back to #");

    check(jump(0, 1) == 2, "forwards: from # to the first A");
    check(jump(2, 1) == 4, "forwards: from the first A to B");
    check(jump(3, 1) == 4, "forwards: from the middle of A to B");
    check(jump(6, 1) == 7, "forwards: to the last letter");
    check(jump(7, 1) == 7, "forwards at the end of the list stays put");

    check(jump(3, -1) == 2, "backwards: from the middle to the start of the letter");
    check(jump(2, -1) == 0, "backwards: from the start of A to the start of #");
    check(jump(1, -1) == 0, "backwards: from the middle of # to its start");
    check(jump(0, -1) == 0, "backwards at the start of the list stays put");

    // A cursor sitting alone in its group must still move, not stall on itself.
    check(jump(5, -1) == 4, "backwards out of a single-element group");

    check(jump(3, 0) == 3, "direction 0 changes nothing");

    std::vector<std::string> single = {"Solo"};
    check(Alphabet::jump(0, 1, 1,
                         [&](int i) -> const std::string & { return single[size_t(i)]; }) == 0,
          "single-element list stays put");

    std::printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
    return failures ? 1 : 0;
}
