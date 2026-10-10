// Host-side checks of the release metadata the updater trusts: version comparison, the GitHub
// release JSON, release-info.txt and SHA256SUMS. These decide what gets installed, so the
// refusals matter as much as the accepted cases.
//
// Build and run:  make -C tests

#include <cstdio>
#include <map>
#include <string>

#include "../src/ReleaseInfo.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-62s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

Version version(const char *text) {
    Version v;
    parseVersion(text, v);
    return v;
}

bool isNewer(const char *candidate, const char *current) {
    Version a, b;
    return parseVersion(candidate, a) && parseVersion(current, b) && compareVersions(a, b) > 0;
}

std::string hash(char digit) { return std::string(64, digit); }

} // namespace

int main() {
    // Versions.
    check(isNewer("0.5.0", "0.4.6"), "a later minor version is newer");
    check(!isNewer("0.4.6", "0.5.0"), "an earlier one is not");
    check(!isNewer("0.5.0", "0.5.0"), "the same version is not");
    check(isNewer("0.5.1", "0.5.0"), "a later patch version is newer");
    check(isNewer("0.10.0", "0.9.0"), "numbers compare as numbers, not text");
    check(isNewer("1.0.0", "0.9.9"), "a later major version is newer");
    Version v;
    check(!parseVersion("", v), "nothing found does not parse");
    check(!parseVersion("nightly", v), "an unreadable tag does not parse");
    check(!parseVersion("0.5", v) && !parseVersion("0.5.0.1", v), "exactly three parts");
    check(!parseVersion("0.05.0", v), "no leading zeros");
    check(!parseVersion("0.5.0-dev", v), "a development suffix is not a release");
    check(!parseVersion("0.70000.0", v), "numbers above 65535 are refused");
    check(versionText(version("0.5.0")) == "0.5.0", "version text round-trips");

    // Library names.
    check(libraryName(version("0.5.0")) == "mister-pats-gui-0.5.0.so", "library file name");
    check(parseLibraryName("mister-pats-gui-0.5.0.so", v) && versionText(v) == "0.5.0",
          "a library name yields its version");
    check(!parseLibraryName("mister-pats-gui-.so", v) && !parseLibraryName("other-0.5.0.so", v) &&
          !parseLibraryName("mister-pats-gui-0.5.0.so.debug", v), "foreign names are refused");

    // GitHub's latest-release reply.
    LatestRelease release;
    std::string error;
    const std::string good =
        "{\"tag_name\":\"v0.5.0\",\"draft\":false,\"prerelease\":false,\"body\":\"Notes \\u00e4\","
        "\"assets\":[{\"name\":\"release-info.txt\"},{\"name\":\"SHA256SUMS\"},"
        "{\"name\":\"mister-pats-gui-0.5.0.so\"}]}";
    check(parseLatestReleaseJson(good, release, error) && release.tag == "v0.5.0",
          "a stable release parses");
    check(release.notes == "Notes \xc3\xa4", "notes keep their unicode escapes");
    check(releaseHasAssets(release, version("0.5.0"), error), "all required assets are present");
    check(!releaseHasAssets(release, version("0.6.0"), error), "a missing library is refused");
    check(!parseLatestReleaseJson("{\"tag_name\":\"v0.5.0\",\"draft\":true,\"prerelease\":false,"
                                  "\"body\":\"\",\"assets\":[]}", release, error), "a draft is refused");
    check(!parseLatestReleaseJson("{\"tag_name\":\"v0.5.0\",\"draft\":false,\"prerelease\":true,"
                                  "\"body\":\"\",\"assets\":[]}", release, error),
          "a prerelease is refused");
    check(!parseLatestReleaseJson("{\"tag_name\":\"v0.5.0-rc1\",\"draft\":false,\"prerelease\":false,"
                                  "\"body\":\"\",\"assets\":[]}", release, error),
          "a non-stable tag is refused");
    check(!parseLatestReleaseJson("{\"tag_name\":\"v0.5.0\",\"tag_name\":\"v9.9.9\",\"draft\":false,"
                                  "\"prerelease\":false,\"body\":\"\",\"assets\":[]}", release, error),
          "a duplicated field is refused");
    check(!parseLatestReleaseJson("{\"tag_name\":", release, error) &&
          !parseLatestReleaseJson("", release, error), "broken JSON is refused");

    // release-info.txt.
    ReleaseInfo info;
    check(parseReleaseInfo("version=0.5.0\nloader_abi=1\ngamesdb_format=4\n", info, error) &&
          versionText(info.version) == "0.5.0" && info.gamesDbFormat == 4, "release-info.txt parses");
    check(!parseReleaseInfo("version=0.5.0\nloader_abi=2\ngamesdb_format=4\n", info, error),
          "an unknown loader ABI is refused");
    check(!parseReleaseInfo("version=0.5.0\nloader_abi=1\ngamesdb_format=4", info, error),
          "a missing final newline is refused");
    check(!parseReleaseInfo("version=0.5.0\r\nloader_abi=1\r\ngamesdb_format=4\r\n", info, error),
          "carriage returns are refused");

    // SHA256SUMS.
    const Version v5 = version("0.5.0");
    const std::string sums = hash('a') + "  mister-gui\n" + hash('b') + "  MiSTer_gui\n" +
                             hash('c') + "  mister-pats-gui-0.5.0.so\n" + hash('d') +
                             "  release-info.txt\n";
    std::map<std::string, std::string> hashes;
    check(parseSha256Sums(sums, v5, hashes, error) && hashes.size() == 4 &&
          hashes["mister-pats-gui-0.5.0.so"] == hash('c'), "SHA256SUMS parses");
    check(!parseSha256Sums(sums + hash('e') + "  extra\n", v5, hashes, error), "extra entries are refused");
    std::string upper = sums;
    upper[0] = 'A';
    check(!parseSha256Sums(upper, v5, hashes, error), "upper-case hashes are refused");
    check(!parseSha256Sums(sums, version("0.6.0"), hashes, error), "entries must match the version");

    std::printf("\n%s\n", failures ? "FAILURES" : "all good");
    return failures ? 1 : 0;
}
