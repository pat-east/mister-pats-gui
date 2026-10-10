#include "Theme.h"

#include <cstdio>
#include <fstream>
#include <sys/stat.h>

#include "Paths.h"

namespace {

const char *kSpaceBold = MISTER_PAT_ROOT "/fonts/SpaceGrotesk-Bold.ttf";
const char *kSpaceMedium = MISTER_PAT_ROOT "/fonts/SpaceGrotesk-Medium.ttf";

bool regularFile(const char *path) {
    struct stat info {};
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 4) return false;
    std::ifstream in(path, std::ios::binary);
    unsigned char signature[4]{};
    return in.read(reinterpret_cast<char *>(signature), sizeof(signature)) &&
           signature[0] == 0x00 && signature[1] == 0x01 &&
           signature[2] == 0x00 && signature[3] == 0x00;
}

} // namespace

Theme::Theme(int screenWidth, int screenHeight) : width_(screenWidth), height_(screenHeight) {
    loadFonts();
}

void Theme::loadFonts() {
    std::vector<std::string> bold;
    std::vector<std::string> regular;
    if (spaceGroteskEnabled_ && spaceGroteskAvailable()) {
        bold.push_back(kSpaceBold);
        regular.push_back(kSpaceMedium);
    }
    bold_.load(bold);
    regular_.load(regular);
    if (bold_.path().find("SpaceGrotesk-") != std::string::npos)
        bold_.setScalePercent(90);
    else
        bold_.setScalePercent(100);
    if (regular_.path().find("SpaceGrotesk-") != std::string::npos)
        regular_.setScalePercent(90);
    else
        regular_.setScalePercent(100);
}

bool Theme::spaceGroteskAvailable() {
    return regularFile(kSpaceBold) && regularFile(kSpaceMedium);
}

bool Theme::spaceGroteskActive() const {
    return bold_.path() == kSpaceBold && regular_.path() == kSpaceMedium;
}

void Theme::setSpaceGroteskEnabled(bool enabled) {
    spaceGroteskEnabled_ = enabled;
    loadFonts();
}

int Theme::px(int designPx) const {
    const int scaled = int(long(designPx) * height_ / 1080);
    return (designPx > 0 && scaled < 1) ? 1 : scaled;
}
