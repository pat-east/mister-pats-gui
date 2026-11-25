#pragma once

// A tiny test framework: TEST(name) registers a case, CHECK()/CHECK_EQ() assert, KNOWN_BUG() records a bug that is
// reported but does not fail the run (it turns into a note "now fixed" once the condition holds). Every case runs in
// its own forked process, so a crash or an abort() in the code under test fails that one case and the rest still run.
// No pixels are compared anywhere: the tests drive the flows and assert behaviour (src/app/tests/README.md).

#include <cstdio>
#include <functional>
#include <sstream>
#include <type_traits>
#include <string>
#include <vector>

namespace tt {

struct Counters {
    int checks = 0, failures = 0, known = 0, fixed = 0;
};

struct Case {
    std::string name;
    std::function<void()> run;
};

Counters &counters();
std::vector<Case> &cases();
std::string &context();   // free text printed with every failure of the running case (the key sequence so far)
void fail(const char *file, int line, const std::string &what);
void known(const char *file, int line, bool nowHolds, const std::string &what);

struct Registrar {
    Registrar(const char *name, std::function<void()> fn) { cases().push_back({name, std::move(fn)}); }
};

// Enums print as numbers, everything else through operator<<.
template <class T>
inline typename std::enable_if<std::is_enum<T>::value, long>::type printable(const T &v) { return long(v); }
template <class T>
inline typename std::enable_if<!std::is_enum<T>::value, const T &>::type printable(const T &v) { return v; }

template <class A, class B>
inline std::string show(const A &a, const B &b) {
    std::ostringstream o;
    o << " (" << printable(a) << " vs " << printable(b) << ")";
    return o.str();
}

}  // namespace tt

#define TEST(name) \
    static void test_##name(); \
    static ::tt::Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(cond) do { ++::tt::counters().checks; if (!(cond)) ::tt::fail(__FILE__, __LINE__, #cond); } while (0)
#define CHECK_MSG(cond, msg) do { ++::tt::counters().checks; if (!(cond)) ::tt::fail(__FILE__, __LINE__, std::string(#cond) + ": " + (msg)); } while (0)
#define CHECK_EQ(a, b) do { ++::tt::counters().checks; if (!((a) == (b))) ::tt::fail(__FILE__, __LINE__, std::string(#a " == " #b) + ::tt::show((a), (b))); } while (0)
// A bug that is known and reported (see the list in the final report / README): not a failure while `cond` is false.
#define KNOWN_BUG(cond, what) do { ++::tt::counters().checks; ::tt::known(__FILE__, __LINE__, (cond), what); } while (0)
