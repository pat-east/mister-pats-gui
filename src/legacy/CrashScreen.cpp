#include "CrashScreen.h"

#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "Canvas.h"
#include "Console.h"
#include "Framebuffer.h"
#include "Theme.h"

namespace CrashScreen {
namespace {

std::string lastCrashSummary() {
    const char *path = "/media/fat/mister-pat/logs/crash.log";
    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return "Crash details are unavailable.";
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        return "Crash details are unavailable.";
    }
    FILE *input = fdopen(fd, "r");
    if (!input) {
        close(fd);
        return "Crash details are unavailable.";
    }
    constexpr long kTailBytes = 8192;
    if (info.st_size > kTailBytes) {
        if (fseek(input, -kTailBytes, SEEK_END) != 0) {
            fclose(input);
            return "Crash details are unavailable.";
        }
        char discard[1024];
        if (!fgets(discard, sizeof(discard), input)) {
            fclose(input);
            return "Crash details are unavailable.";
        }
    }
    std::string line, last;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), input)) {
        line += buffer;
        if (line.size() > 4096) line.resize(4096);
        if (line.find('\n') != std::string::npos) {
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            if (!line.empty()) last = line;
            line.clear();
        }
    }
    if (!line.empty()) last = line;
    fclose(input);
    if (last.empty()) return "Crash details are unavailable.";
    const size_t pid = last.find(" pid=");
    if (pid != std::string::npos) last.resize(pid);
    const size_t relaunch = last.find(" auto_relaunch=");
    if (relaunch != std::string::npos) last.resize(relaunch);
    if (last.compare(0, 25, "std::terminate backtrace:") == 0) {
        const size_t exception = last.find(" exception=");
        if (exception != std::string::npos) last = "C++ terminate:" + last.substr(exception + 11);
    } else if (last.compare(0, 18, "uncaught exception") == 0) {
        const size_t backtrace = last.find(" backtrace:");
        if (backtrace != std::string::npos) last.resize(backtrace);
    }
    return last;
}

void draw(Framebuffer &framebuffer, Theme &theme, int exitStatus) {
    Canvas canvas(framebuffer.width(), framebuffer.height());
    canvas.verticalGradient(canvas.bounds(), theme.background, theme.backgroundLo);

    const int cardWidth = std::min(theme.px(980), canvas.width() - theme.px(48));
    const int cardHeight = std::min(theme.px(590), canvas.height() - theme.px(48));
    const Rect card{(canvas.width() - cardWidth) / 2,
                    (canvas.height() - cardHeight) / 2, cardWidth, cardHeight};

    canvas.dropShadow(card, theme.px(24), theme.px(12), theme.shadow.withAlpha(90));
    canvas.fillRoundedRect(card, theme.px(18), theme.surface);
    canvas.fillRoundedRect({card.x, card.y, card.w, theme.px(6)}, theme.px(3), theme.warning);

    const int iconSize = theme.px(74);
    const Rect icon{canvas.width() / 2 - iconSize / 2, card.y + theme.px(48), iconSize,
                    iconSize};
    canvas.strokeRoundedRect(icon, theme.px(18), theme.px(3), theme.warning);
    theme.bold().drawCentered(canvas, icon, "!", theme.px(48), theme.warning);

    const int innerWidth = card.w - theme.px(96);
    const int innerX = card.x + (card.w - innerWidth) / 2;
    theme.bold().drawCentered(canvas,
                              {innerX, icon.bottom() + theme.px(28), innerWidth, theme.px(58)},
                              "GUI STOPPED AFTER AN ERROR", theme.px(36), theme.textPrimary);
    theme.regular().drawCentered(canvas,
                                 {innerX, icon.bottom() + theme.px(102), innerWidth,
                                  theme.px(36)},
                                 "It will not restart automatically.", theme.px(22),
                                 theme.textMuted);
    theme.bold().drawCentered(canvas,
                              {innerX, icon.bottom() + theme.px(148), innerWidth,
                               theme.px(36)},
                              "Please restart your MiSTer manually.", theme.px(22),
                              theme.textPrimary);

    char status[64];
    std::snprintf(status, sizeof(status), "EXIT STATUS: %d", exitStatus);
    const int pillWidth = theme.px(250);
    const Rect pill{canvas.width() / 2 - pillWidth / 2,
                    icon.bottom() + theme.px(210), pillWidth, theme.px(42)};
    canvas.fillRoundedRect(pill, theme.px(12), theme.surfaceHi);
    theme.regular().drawCentered(canvas, pill, status, theme.px(17), theme.textMuted);

    theme.regular().drawCentered(
        canvas, {innerX, pill.bottom() + theme.px(22), innerWidth, theme.px(30)},
        "Crash log (if available): /media/fat/mister-pat/logs/crash.log", theme.px(16),
        theme.textMuted);
    const int detailSize = theme.px(16);
    const std::string summary = theme.regular().elide(lastCrashSummary(), detailSize, innerWidth);
    theme.regular().drawCentered(
        canvas, {innerX, pill.bottom() + theme.px(60), innerWidth, theme.px(34)},
        summary, detailSize, theme.textPrimary);

    framebuffer.present(canvas);
}

} // namespace

int show(int exitStatus) {
    ConsoleGuard console;
    console.acquire();

    Framebuffer framebuffer;
    if (!framebuffer.open()) {
        std::fprintf(stderr, "GUI stopped after an error (exit status %d). Restart MiSTer manually.\n",
                     exitStatus);
        for (;;) pause();
    }

    Theme theme(framebuffer.width(), framebuffer.height());
    try {
        draw(framebuffer, theme, exitStatus);
    } catch (...) {
        // Keep the framebuffer usable even if drawing the polished panel runs out of memory.
        Canvas fallback(framebuffer.width(), framebuffer.height());
        fallback.clear(theme.background);
        theme.bold().drawCentered(fallback,
                                  {0, framebuffer.height() / 3, framebuffer.width(),
                                   theme.px(64)},
                                  "GUI STOPPED AFTER AN ERROR", theme.px(30),
                                  theme.textPrimary);
        theme.regular().drawCentered(fallback,
                                     {0, framebuffer.height() / 2, framebuffer.width(),
                                      theme.px(48)},
                                     "Restart your MiSTer manually.", theme.px(20),
                                     theme.textMuted);
        framebuffer.present(fallback);
    }

    // The parent launcher has set a RAM-only pause marker. Leave this frame on screen and
    // wait without polling or writing to storage until the user reboots the MiSTer.
    for (;;) pause();
}

} // namespace CrashScreen
