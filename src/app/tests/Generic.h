#pragma once

// The invariants every page of every flow must keep, and the list of flows (with the sample states that change their
// paths) that the generic tests walk. Specific tests (Scan.cpp, Wifi.cpp, ...) use the same helpers.

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "app/tests/Rig.h"

namespace tt {

// ---- Hint vocabulary (docs/components/Hints.md, "Vocabulary (closed set)"). Returns "" when the word is allowed, else why not.
bool hintWordAllowed(ui::HintKey key, const std::string &word);
// Words that break the closed vocabulary today (found by these tests, reported, not yet fixed). A word listed here is a
// KNOWN_BUG; a word that is neither allowed nor listed fails the run.
const std::map<std::string, std::string> &knownVocabularyDeviations();

// ---- The generic page check: run after every step of every walk. `where` names the situation for the failure text.
// Checks (all on the top page of the stack): hints are non-empty (a Busy page that cannot be cancelled may list none) and
// from the closed vocabulary; a Busy page that can be cancelled lists B Cancel; a Result page has a headline, says what
// changed and (Error) what to do next, and A is "Try again" exactly when it has a retry page, else "Done"; a Step page that
// cannot continue lists neither A nor Start as a way on; draw() does not crash.
void checkPage(Rig &r, bool draw = true);

// B leaves: presses B (with a short wait after each press) until the stack is empty. A page that does not leave on B must say
// so: a Busy page that cannot be cancelled has no B hint and ends by itself within `patience` seconds; a Capture page lists a
// way out. Returns true when the stack is empty. `maxPresses` is the N of the brief.
bool escapeWithB(Rig &r, int maxPresses = 14, float patience = 90.0f);

// ---- The flows
struct Twiddle {
    std::string key;
    std::vector<std::string> values;   // the monkey sets one of them now and then
};

struct FlowSpec {
    std::string name;                          // "library.scan", "system.wifi/needsCountry"
    std::function<void(Rig &)> setup;          // sample states before the flow opens (may be empty)
    std::function<void(Rig &)> open;           // opens the flow like the row does
    std::vector<Twiddle> twiddles;             // sample states the monkey may change while the flow runs
    std::vector<std::string> keys = {"a", "a", "a", "b", "b", "down", "up", "start", "left", "right", "x", "y"};   // what the monkey presses
    // Flow specific rules, called around every frame: before=true just before the press/update, before=false after.
    std::function<void(Rig &, bool before)> watch;
    // After the walk and the GUI closing: nothing may be left running.
    std::function<void(Rig &)> atEnd;
};
const std::vector<FlowSpec> &flowSpecs();
const FlowSpec &specNamed(const std::string &name);   // CHECK-fails and returns the first spec when there is none

// The scripted, deterministic random generator the walks use (no <random>: the same numbers on every machine).
struct Rng {
    unsigned long long s;
    explicit Rng(unsigned long long seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
    unsigned next() { s = s * 6364136223846793005ULL + 1442695040888963407ULL; return unsigned(s >> 33); }
    int below(int n) { return int(next() % unsigned(n)); }
};

// One step of a walk: a key, a wait, or a sample state.
struct Act {
    enum Kind { Key, Wait, Set } kind = Key;
    std::string text;   // key name, or the sample key
    std::string value;  // Set: its value
    float seconds = 0;  // Wait
};
void apply(Rig &r, const Act &a);
std::string describe(const std::vector<Act> &acts, size_t n);

}  // namespace tt
