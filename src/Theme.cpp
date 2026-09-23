#include "Theme.h"

#include <cstdio>

#include "Paths.h"

namespace {

// Console Mode's own copy first, so an existing install needs nothing further; then a copy
// placed by hand (see INSTALL.md). Nothing beyond that is worth listing — MiSTer's own OSD
// draws from a bitmap font compiled into its binary, not a loose TTF on disk, so there is no
// third system location to reliably find one at. If neither candidate opens, Font::load()
// reports it and every caller falls back to the built-in bitmap face, which needs no file at
// all and is the one fallback actually guaranteed to be there.
const std::vector<std::string> kBoldCandidates = {
    "/media/fat/ConsoleMode/themeconfig/resources/Akrobat-Bold.ttf",
    MISTER_PAT_ROOT "/fonts/Akrobat-Bold.ttf",
};

const std::vector<std::string> kRegularCandidates = {
    "/media/fat/ConsoleMode/themeconfig/resources/Akrobat-SemiBold.ttf",
    MISTER_PAT_ROOT "/fonts/Akrobat-SemiBold.ttf",
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
