#include "Console.h"

#include <cstdio>
#include <fcntl.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

// Tried in order: the process's own terminal first, then the active console.
const char *kCandidates[] = {"/dev/tty", "/dev/tty0", "/dev/console"};

} // namespace

ConsoleGuard::~ConsoleGuard() { release(); }

void ConsoleGuard::acquire() {
    if (changed_) return;

    for (const char *path : kCandidates) {
        // O_NOCTTY matters: without a controlling terminal (after setsid) this open
        // would adopt the console as ours, and the kernel then stops us with SIGTTOU.
        fd_ = ::open(path, O_RDWR | O_NOCTTY);
        if (fd_ < 0) continue;

        // Remember what we found instead of assuming text mode: whoever ran before us may
        // already have been in graphics mode, and forcing text back leaves a blinking cursor.
        long previous = KD_TEXT;
        if (ioctl(fd_, KDGETMODE, &previous) != 0) previous = KD_TEXT;

        if (ioctl(fd_, KDSETMODE, KD_GRAPHICS) == 0) {
            previousMode_ = int(previous);
            changed_ = true;
            std::printf("console: %s in graphics mode (was %s)\n", path,
                        previous == KD_GRAPHICS ? "graphics" : "text");
            return;
        }

        ::close(fd_);
        fd_ = -1;
    }

    std::printf("console: no terminal could be switched, text may bleed through\n");
}

void ConsoleGuard::release() {
    if (fd_ < 0) return;

    if (changed_) ioctl(fd_, KDSETMODE, previousMode_);
    ::close(fd_);
    fd_ = -1;
    changed_ = false;
}
