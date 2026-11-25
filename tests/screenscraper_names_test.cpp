// Host-side checks for the ScreenScraper title rules and the small JSON reader.
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/legacy/MiniJson.h"
#include "../src/legacy/ScreenScraperNames.h"

namespace {
int failures = 0;

void expect(bool ok, const char *what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}

void expectEq(const std::string &got, const std::string &want, const char *what) {
    if (got != want) {
        std::printf("FAIL: %s\n   got  [%s]\n   want [%s]\n", what, got.c_str(), want.c_str());
        ++failures;
    }
}
}  // namespace

int main() {
    using namespace ScreenScraperNames;

    expectEq(searchTitle("Final Fantasy III (USA).zip"), "Final Fantasy III", "tags and extension");
    expectEq(searchTitle("Legend of Zelda, The - A Link to the Past (USA)"),
             "Legend of Zelda, The - A Link to the Past", "an article inside is left alone");
    expectEq(searchTitle("Legend of Zelda, The (USA)"), "The Legend of Zelda", "trailing article moves");
    expectEq(searchTitle("Mario Vs. Donkey Kong (U)(Rising Sun).zip"), "Mario Vs. Donkey Kong",
             "a dot inside a title is not an extension");
    expectEq(searchTitle("Super Mario Bros. 3 [!]"), "Super Mario Bros. 3", "square brackets");
    expectEq(searchTitle("JSSDemoII V1.0b"), "JSSDemoII V1.0b", "unknown short suffix stays");
    expectEq(searchTitle("2Xtreme"), "2Xtreme", "folder style name");
    expectEq(searchTitle("Sonic   Spinball  (Europe)"), "Sonic Spinball", "spaces collapse");

    expectEq(titleKey("Sonic & Knuckles"), titleKey("Sonic and Knuckles"), "ampersand");
    expectEq(titleKey("Pokémon Red"), "pokemonred", "accent folded");
    expectEq(titleKey("Tempo Jr."), "tempojr", "punctuation dropped");
    expectEq(titleKey("The Legend of Zelda"), titleKey("Legend of Zelda, The (USA)"), "article");
    expect(titleKey("Mario Vs. Donkey Kong") != titleKey("Mario Vs. Donkey Kong 2"), "different titles differ");
    expectEq(titleKey("Ünïcode Ñame"), "unicodename", "more accents");
    expectEq(titleKey("caf\xC3"), "caf", "truncated UTF-8 does not crash");

    expect(systemId("snes") == 4 && systemId("SNES") == 4, "snes id, any case");
    expect(systemId("GBA") == 12, "gba id");
    expect(systemId("Arcade") == 0 && systemId("C128") == 0, "arcade and computers are not asked");
    expectEq(fileSafe("Atari 2600/x"), "Atari_2600_x", "file safe");

    JsonValue v;
    std::string error;
    expect(parseJson("{\"a\":[1,\"2\",{\"b\":true}],\"s\":\"x\\u00e9\\n\",\"n\":null}", v, error), "parse ok");
    expect(v["a"].at(0).asInteger() == 1 && v["a"].at(1).asInteger() == 2, "numbers and numeric strings");
    expect(v["a"].at(2)["b"].boolean, "nested bool");
    expectEq(v["s"].asString(), "x\xC3\xA9\n", "unicode escape");
    expect(v["missing"]["deeper"].type == JsonValue::Type::Null, "missing keys give null");
    expect(!parseJson("{\"a\":[1,2", v, error), "truncated input is rejected");
    expect(!parseJson("{\"a\":1} trailing", v, error), "trailing text is rejected");
    expect(!parseJson(std::string(200, '['), v, error), "nesting is bounded");

    if (failures == 0) std::printf("screenscraper_names_test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
