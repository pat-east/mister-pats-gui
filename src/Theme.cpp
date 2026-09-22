#include "Theme.h"

#include <cstdio>

#include "Paths.h"

namespace {

// ConsoleMode ships these; the rest are common locations so the app also runs on a plain box.
const std::vector<std::string> kBoldCandidates = {
    "/media/fat/ConsoleMode/themeconfig/resources/Akrobat-Bold.ttf",
    MISTER_PAT_ROOT "/fonts/Akrobat-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
};

const std::vector<std::string> kRegularCandidates = {
    "/media/fat/ConsoleMode/themeconfig/resources/Akrobat-SemiBold.ttf",
    MISTER_PAT_ROOT "/fonts/Akrobat-SemiBold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
};

} // namespace

Theme::Theme(int screenWidth, int screenHeight) : width_(screenWidth), height_(screenHeight) {
    if (!bold_.load(kBoldCandidates)) std::printf("theme: no bold font, using built-in face\n");
    else std::printf("theme: bold font %s\n", bold_.path().c_str());

    if (!regular_.load(kRegularCandidates)) std::printf("theme: no regular font, using built-in face\n");
    else std::printf("theme: regular font %s\n", regular_.path().c_str());
}

int Theme::px(int designPx) const {
    const int scaled = int(long(designPx) * height_ / 1080);
    return (designPx > 0 && scaled < 1) ? 1 : scaled;
}
