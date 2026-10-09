#include "CrashScreen.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unistd.h>

#include "Canvas.h"
#include "Console.h"
#include "Framebuffer.h"
#include "Theme.h"

namespace CrashScreen {
namespace {

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
