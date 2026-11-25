// Runs every registered TEST in its own process (fork), collects the counters through a pipe and prints a summary.
//   app-test               run everything
//   app-test NAME...       run the cases whose name contains one of the words
//   app-test --list        list the cases
//   app-test --no-fork X   run in this process (for a debugger)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include "app/tests/Test.h"

namespace tt {

Counters &counters() { static Counters c; return c; }
std::vector<Case> &cases() { static std::vector<Case> c; return c; }
std::string &context() { static std::string s; return s; }

void fail(const char *file, int line, const std::string &what) {
    ++counters().failures;
    static std::map<std::string, int> seen;   // the same message prints three times at most per case
    if (++seen[what] > 3) return;
    const char *slash = std::strrchr(file, '/');
    std::printf("    FAIL %s:%d: %s\n         after:%s\n", slash ? slash + 1 : file, line, what.c_str(), context().c_str());
    std::fflush(stdout);
}

void known(const char *file, int line, bool nowHolds, const std::string &what) {
    const char *slash = std::strrchr(file, '/');
    static std::set<std::string> printed;   // a known bug prints once per case
    if (!nowHolds && !printed.insert(what).second) return;
    if (nowHolds) {
        ++counters().fixed;
        std::printf("    NOTE %s:%d: known bug now fixed, remove KNOWN_BUG: %s\n", slash ? slash + 1 : file, line, what.c_str());
    } else {
        ++counters().known;
        std::printf("    KNOWN BUG %s:%d: %s\n         after:%s\n", slash ? slash + 1 : file, line, what.c_str(), context().c_str());
    }
    std::fflush(stdout);
}

}  // namespace tt

int main(int argc, char **argv) {
    bool fork_ = true, list = false;
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--no-fork") fork_ = false;
        else if (a == "--list") list = true;
        else filters.push_back(a);
    }
    int ran = 0, passed = 0, failed = 0, crashed = 0;
    tt::Counters total;
    for (const tt::Case &c : tt::cases()) {
        bool wanted = filters.empty();
        for (const std::string &f : filters)
            if (c.name.find(f) != std::string::npos) wanted = true;
        if (!wanted) continue;
        if (list) { std::printf("%s\n", c.name.c_str()); continue; }
        ++ran;
        std::printf("RUN  %s\n", c.name.c_str());
        std::fflush(stdout);
        tt::counters() = tt::Counters();
        tt::context().clear();
        bool crashedHere = false;
        if (!fork_) {
            c.run();
        } else {
            int fd[2];
            if (pipe(fd) != 0) return 2;
            const pid_t pid = fork();
            if (pid == 0) {
                close(fd[0]);
                c.run();
                const tt::Counters r = tt::counters();
                if (write(fd[1], &r, sizeof r) != ssize_t(sizeof r)) _exit(3);
                std::fflush(stdout);
                _exit(0);
            }
            close(fd[1]);
            tt::Counters r;
            const ssize_t got = read(fd[0], &r, sizeof r);
            close(fd[0]);
            int status = 0;
            waitpid(pid, &status, 0);
            if (got == ssize_t(sizeof r) && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                tt::counters() = r;
            } else {
                crashedHere = true;
                std::printf("    CRASH %s (%s %d)\n", c.name.c_str(), WIFSIGNALED(status) ? "signal" : "exit", WIFSIGNALED(status) ? WTERMSIG(status) : WEXITSTATUS(status));
            }
        }
        const tt::Counters &r = tt::counters();
        total.checks += r.checks; total.failures += r.failures; total.known += r.known; total.fixed += r.fixed;
        if (crashedHere) { ++crashed; ++failed; }
        else if (r.failures > 0) ++failed;
        else ++passed;
        std::printf("%s %s  (%d checks%s%s)\n", crashedHere ? "CRASH" : r.failures ? "FAIL" : "PASS", c.name.c_str(), r.checks,
                    r.known ? (", " + std::to_string(r.known) + " known bug(s)").c_str() : "", r.fixed ? ", known bug FIXED" : "");
        std::fflush(stdout);
    }
    if (list) return 0;
    std::printf("\n%d cases: %d passed, %d failed (%d crashed); %d checks, %d failed checks, %d known-bug hits, %d known bugs now fixed\n", ran,
                passed, failed, crashed, total.checks, total.failures, total.known, total.fixed);
    return failed ? 1 : 0;
}
