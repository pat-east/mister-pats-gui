#include "UiSounds.h"

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <spawn.h>
#include <thread>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "StartSoundData.h"
#include "TabSoundData.h"
#include "NavigationSoundData.h"
#include "LaunchSoundData.h"

extern "C" char **environ;

namespace {

bool writeAll(int fd, const uint8_t *data, size_t size) {
    while (size) {
        const ssize_t written = write(fd, data, size);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        data += written;
        size -= size_t(written);
    }
    return true;
}

// Plays one WAV: aplay is started with posix_spawn (vfork semantics: the big GUI process is not copied), fed through a
// pipe and reaped here, so there are no zombies and nothing runs on the caller's thread.
void playNow(const uint8_t *wav, size_t wavSize) {
    int pipeFds[2];
    if (pipe2(pipeFds, O_CLOEXEC) != 0) return;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipeFds[0], STDIN_FILENO);
    char *const argv[] = {const_cast<char *>("aplay"), const_cast<char *>("-q"), const_cast<char *>("-t"),
                          const_cast<char *>("wav"), const_cast<char *>("-"), nullptr};
    pid_t player = 0;
    const int rc = posix_spawnp(&player, "aplay", &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipeFds[0]);
    if (rc != 0) { close(pipeFds[1]); return; }
    writeAll(pipeFds[1], wav, wavSize);   // SIGPIPE is blocked on this thread: a dead aplay is just a failed write
    close(pipeFds[1]);
    int status = 0;
    while (waitpid(player, &status, 0) < 0 && errno == EINTR) {}
}

struct Sound { const uint8_t *wav = nullptr; size_t size = 0; bool tick = false; };

std::mutex gMutex;
std::condition_variable gWake;
Sound gPending;            // the one waiting sound (wav == nullptr: none)
bool gBusy = false;        // the worker is playing one
bool gStarted = false;
int gCalls = 0, gDropped = 0;
double gTotalMs = 0, gMaxMs = 0;

void worker() {
    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &block, nullptr);
    for (;;) {
        Sound next;
        {
            std::unique_lock<std::mutex> lock(gMutex);
            gWake.wait(lock, [] { return gPending.wav != nullptr; });
            next = gPending;
            gPending = Sound();
            gBusy = true;
        }
        playNow(next.wav, next.size);
        sigset_t pending;   // swallow a SIGPIPE a failed write may have left pending on this thread
        sigemptyset(&pending);
        sigpending(&pending);
        int got = 0;
        if (sigismember(&pending, SIGPIPE)) sigwait(&block, &got);
        std::lock_guard<std::mutex> lock(gMutex);
        gBusy = false;
    }
}

void enqueue(const uint8_t *wav, size_t size, bool tick) {
    const auto begin = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(gMutex);
        ++gCalls;
        if (!gStarted) {
            gStarted = true;
            try { std::thread(worker).detach(); } catch (...) { gStarted = false; }
        }
        if (gPending.wav && tick) ++gDropped;                         // a tick never waits behind another sound
        else if (gBusy && gPending.wav == nullptr && tick) { gPending = {wav, size, tick}; gWake.notify_one(); }
        else { if (gPending.wav) ++gDropped; gPending = {wav, size, tick}; gWake.notify_one(); }
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    std::lock_guard<std::mutex> lock(gMutex);
    gTotalMs += ms;
    if (ms > gMaxMs) gMaxMs = ms;
}

} // namespace

UiSounds::Stats UiSounds::takeStats() {
    std::lock_guard<std::mutex> lock(gMutex);
    Stats s;
    s.calls = gCalls; s.dropped = gDropped; s.totalMs = gTotalMs; s.maxMs = gMaxMs;
    gCalls = gDropped = 0; gTotalMs = gMaxMs = 0;
    return s;
}

void UiSounds::play() {
    enqueue(start_sound::kWav, start_sound::kWavSize, false);
}

void UiSounds::playTabChange() {
    enqueue(tab_sound::kWav, tab_sound::kWavSize, false);
}

void UiSounds::playNavigation() {
    enqueue(navigation_sound::kWav, navigation_sound::kWavSize, true);
}

void UiSounds::playGameLaunch() {
    enqueue(launch_sound::kWav, launch_sound::kWavSize, false);
}
