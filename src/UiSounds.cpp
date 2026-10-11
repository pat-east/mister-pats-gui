#include "UiSounds.h"

#include <cerrno>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "StartSoundData.h"
#include "TabSoundData.h"
#include "NavigationSoundData.h"
#include "LaunchSoundData.h"

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

void playInWorker(const uint8_t *wav, size_t wavSize) {
    int pipeFds[2];
    if (pipe(pipeFds) != 0) _exit(0);
    const pid_t player = fork();
    if (player < 0) {
        close(pipeFds[0]); close(pipeFds[1]);
        _exit(0);
    }
    if (player == 0) {
        close(pipeFds[1]);
        if (dup2(pipeFds[0], STDIN_FILENO) < 0) _exit(127);
        close(pipeFds[0]);
        execlp("aplay", "aplay", "-q", "-t", "wav", "-", static_cast<char *>(nullptr));
        _exit(127);
    }

    close(pipeFds[0]);
    writeAll(pipeFds[1], wav, wavSize);
    close(pipeFds[1]);
    int status = 0;
    while (waitpid(player, &status, 0) < 0 && errno == EINTR) {}
    _exit(0);
}

void playWavInBackground(const uint8_t *wav, size_t wavSize) {
    // Double-fork so playback never stalls the frame loop and the short-lived worker is
    // reaped by init instead of becoming a zombie in the long-running GUI process.
    const pid_t intermediate = fork();
    if (intermediate < 0) return;
    if (intermediate == 0) {
        const pid_t worker = fork();
        if (worker == 0) playInWorker(wav, wavSize);
        _exit(0);
    }
    int status = 0;
    while (waitpid(intermediate, &status, 0) < 0 && errno == EINTR) {}
}

} // namespace

void UiSounds::play() {
    playWavInBackground(start_sound::kWav, start_sound::kWavSize);
}

void UiSounds::playTabChange() {
    playWavInBackground(tab_sound::kWav, tab_sound::kWavSize);
}

void UiSounds::playNavigation() {
    playWavInBackground(navigation_sound::kWav, navigation_sound::kWavSize);
}

void UiSounds::playGameLaunch() {
    playWavInBackground(launch_sound::kWav, launch_sound::kWavSize);
}
