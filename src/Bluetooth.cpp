#include "Bluetooth.h"

#include <cstdio>
#include <csignal>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

// Not `hci_get_route()` (BlueZ) — this project's own GUI does not link libbluetooth, only
// Main_MiSTer's patched binary does. The sysfs node an adapter registers is the cheaper
// equivalent; confirmed present on the reference device's own dmesg
// (CONTROLLER.md, "Bluetooth pairing"), not yet proven bit-for-bit equivalent to what
// hci_get_route checks.
bool hciNodePresent() {
    for (int i = 0; i < 4; ++i) {
        char path[64];
        std::snprintf(path, sizeof(path), "/sys/class/bluetooth/hci%d", i);
        struct stat st{};
        if (stat(path, &st) == 0) return true;
    }
    return false;
}

} // namespace

bool Bluetooth::adapterPresent() { return hciNodePresent(); }

bool Bluetooth::startScanning() {
    if (childPid_ > 0) return false;

    int pipeFds[2];
    if (pipe(pipeFds) != 0) return false;

    const pid_t pid = fork();
    if (pid < 0) {
        close(pipeFds[0]);
        close(pipeFds[1]);
        return false;
    }

    if (pid == 0) {
        // Child: stdout -> the pipe's write end, same as stock's own popen(..., "r") does.
        dup2(pipeFds[1], STDOUT_FILENO);
        close(pipeFds[0]);
        close(pipeFds[1]);
        execl("/usr/sbin/btpair", "btpair", (char *)nullptr);
        _exit(127); // only reached if exec itself failed
    }

    close(pipeFds[1]);
    fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);

    childPid_ = pid;
    readFd_ = pipeFds[0];
    pending_.clear();
    return true;
}

bool Bluetooth::nextLine(std::string &line) {
    if (childPid_ <= 0) return false;

    // Drain whatever the kernel is holding right now — non-blocking, so a modal's per-frame
    // update() never stalls waiting on a line that has not arrived yet.
    char buffer[512];
    ssize_t got;
    while ((got = read(readFd_, buffer, sizeof(buffer))) > 0) pending_.append(buffer, size_t(got));

    const size_t newline = pending_.find('\n');
    if (newline == std::string::npos) return false;

    line = pending_.substr(0, newline);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    pending_.erase(0, newline + 1);
    return true;
}

void Bluetooth::cancel() {
    if (readFd_ >= 0) {
        close(readFd_);
        readFd_ = -1;
    }
    if (childPid_ <= 0) return;

    kill(childPid_, SIGINT);

    // A bounded wait, not an indefinite one: this runs synchronously from whatever asked to
    // cancel (a modal's Back handler, or the destructor on the way out), and a hung child
    // process must not be able to freeze the whole GUI over it.
    for (int i = 0; i < 20; ++i) {
        int status = 0;
        if (waitpid(childPid_, &status, WNOHANG) == childPid_) {
            childPid_ = -1;
            return;
        }
        struct timespec pause{0, 25 * 1000000L};
        nanosleep(&pause, nullptr);
    }

    kill(childPid_, SIGKILL);
    int status = 0;
    waitpid(childPid_, &status, 0);
    childPid_ = -1;
}
