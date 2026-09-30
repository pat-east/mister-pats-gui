// Host-side check of the version comparison behind the "update available" note.
//
// Build and run:  make -C tests

#include <cstdio>

#include "../src/UpdateCheck.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

} // namespace

int main() {
    check(UpdateCheck::isNewer("0.2.0", "0.1.8"), "a later minor version is newer");
    check(!UpdateCheck::isNewer("0.1.8", "0.2.0"), "an earlier one is not - the bug this pins");
    check(!UpdateCheck::isNewer("0.2.0", "0.2.0"), "the same version is not");
    check(UpdateCheck::isNewer("0.2.1", "0.2.0"), "a later patch version is newer");
    check(UpdateCheck::isNewer("0.10.0", "0.9.0"), "numbers compare as numbers, not text");
    check(UpdateCheck::isNewer("1.0.0", "0.9.9"), "a later major version is newer");
    check(!UpdateCheck::isNewer("", "0.2.0"), "nothing found is not an update");
    check(!UpdateCheck::isNewer("nightly", "0.2.0"), "an unreadable tag is not an update");
    check(!UpdateCheck::isNewer("0.3.0", "weird"), "an unreadable current version is not one either");

    std::printf("\n%s\n", failures ? "FAILURES" : "all good");
    return failures ? 1 : 0;
}
