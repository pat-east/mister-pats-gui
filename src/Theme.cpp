#include "Theme.h"

#include <cstdio>

#include "Paths.h"

namespace {

// Prefer the bundled-by-setup Space Grotesk files, then Akrobat if present. Font falls back
// to the built-in system-style bitmap face if neither TrueType family can be loaded.
const std::vector<std::string> kBoldCandidates = {
    MISTER_PAT_ROOT "/fonts/SpaceGrotesk-Bold.ttf",
    "/media/fat/ConsoleMode/themeconfig/resources/Akrobat-Bold.ttf",
    MISTER_PAT_ROOT "/fonts/Akrobat-Bold.ttf",
};

const std::vector<std::string> kRegularCandidates = {
    MISTER_PAT_ROOT "/fonts/SpaceGrotesk-Medium.ttf",
    "/media/fat/ConsoleMode/themeconfig/resources/Akrobat-SemiBold.ttf",
    MISTER_PAT_ROOT "/fonts/Akrobat-SemiBold.ttf",
};

} // namespace

Theme::Theme(int screenWidth, int screenHeight) : width_(screenWidth), height_(screenHeight) {
    bold_.load(kBoldCandidates);
    regular_.load(kRegularCandidates);
    if (bold_.path().find("SpaceGrotesk-") != std::string::npos)
        bold_.setScalePercent(90);
    if (regular_.path().find("SpaceGrotesk-") != std::string::npos)
        regular_.setScalePercent(90);
}

int Theme::px(int designPx) const {
    const int scaled = int(long(designPx) * height_ / 1080);
    return (designPx > 0 && scaled < 1) ? 1 : scaled;
}
