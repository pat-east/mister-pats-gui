// UiSounds must return at once however slow the player is, never queue without bound and leave no zombies.
// A fake "aplay" (a shell script that takes 300 ms to "play") is put first on PATH.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "UiSounds.h"

int main() {
    char dir[] = "/tmp/ui-sounds-test-XXXXXX";
    if (!mkdtemp(dir)) return 2;
    const std::string log = std::string(dir) + "/played.log";
    {
        std::ofstream f(std::string(dir) + "/aplay");
        f << "#!/bin/sh\ncat > /dev/null\necho played >> " << log << "\nsleep 0.3\n";
    }
    chmod((std::string(dir) + "/aplay").c_str(), 0755);
    setenv("PATH", (std::string(dir) + ":" + getenv("PATH")).c_str(), 1);

    const auto begin = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; ++i) UiSounds::playNavigation();   // key repeat
    UiSounds::playTabChange();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    const UiSounds::Stats stats = UiSounds::takeStats();
    std::printf("201 calls in %.2f ms (worst single call %.3f ms, %d dropped)\n", ms, stats.maxMs, stats.dropped);
    int failures = 0;
    if (ms > 100.0) { std::printf("FAIL: the calls blocked (%.1f ms)\n", ms); ++failures; }
    if (stats.dropped < 150) { std::printf("FAIL: key repeat was not coalesced (dropped %d)\n", stats.dropped); ++failures; }

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    int plays = 0;
    { std::ifstream f(log); std::string line; while (std::getline(f, line)) ++plays; }
    std::printf("player started %d times\n", plays);
    if (plays < 1 || plays > 4) { std::printf("FAIL: expected 1..4 plays\n"); ++failures; }
    // No child of ours may be left (a zombie would be reaped by this wait).
    int status = 0;
    if (waitpid(-1, &status, WNOHANG) > 0) { std::printf("FAIL: unreaped child\n"); ++failures; }
    std::system((std::string("rm -rf ") + dir).c_str());
    std::puts(failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
