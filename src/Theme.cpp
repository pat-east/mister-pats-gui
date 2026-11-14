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

namespace {

struct HighContrastColor { const char *name; uint32_t rgb; };
// Light, saturated colors that stand out on the black high-contrast background. They differ in
// hue so a viewer can pick the one that is easiest to see with their eyes.
const HighContrastColor kHighContrastPalette[Theme::kHighContrastColors] = {
    {"White", 0xFFFFFF},
    {"Pink", 0xFF8FD8},
    {"Yellow", 0xFFE66D}, // the default, see Theme::kDefaultHighContrastColor
    {"Cyan", 0x6FEAFF},
    {"Lime Green", 0xB4FF3C},
    {"Electric Blue", 0x5B9BFF},
    {"Orange", 0xFFA64D},
    {"Lavender", 0xC9B3FF},
};

struct ColorTheme {
    const char *name;
    uint32_t accent;
    uint32_t focus;
    uint32_t background;
    uint32_t backgroundLo;
    uint32_t surface;
    uint32_t surfaceHi;
    uint32_t brightBackground;
    uint32_t brightBackgroundLo;
};
const ColorTheme kColorThemes[] = {
    {"MiSTer Pat", 0x4C8DFF, 0xB9D5FF, 0x0B0D14, 0x05070C, 0x161A26, 0x222839, 0xA6C6FF, 0x8AACEE},
    {"Nintendo", 0xDD2020, 0xFFFFFF, 0x0D0C0C, 0x070707, 0x181717, 0x252323, 0xE6E6E6, 0xD9D9D9},
    {"PlayStation", 0x0070D1, 0x9ACBFF, 0x090B17, 0x050610, 0x11162A, 0x1C2542, 0x80B8E8, 0x66A2D8},
    {"XBOX", 0x107C10, 0xB0E9A7, 0x08110B, 0x040805, 0x101C13, 0x1B2D1E, 0x88BE88, 0x70AA70},
    {"Batman", 0xF5C928, 0xFFF0A3, 0x090A0C, 0x030405, 0x15171A, 0x262A2F, 0xE6D991, 0xD7C975},
    {"Superman", 0xE32636, 0xFF9BA3, 0x071326, 0x030815, 0x102445, 0x183765, 0xA6BCE8, 0x8FA7DB},
    {"Flash", 0xFFD21F, 0xFFF1A3, 0x16080B, 0x080405, 0x241014, 0x391922, 0xD98B8B, 0xC97575},
    {"Green Lantern", 0x20D36B, 0xA0FFC2, 0x040D09, 0x020604, 0x0D1911, 0x173020, 0x91DDAE, 0x75CC96},
    {"Iron Man", 0xD92732, 0xFFD15A, 0x101113, 0x070809, 0x1C1E22, 0x2B2D33, 0xD7949A, 0xC47C84},
    {"Gold", 0xD8A928, 0xF4D77F, 0x120F08, 0x080704, 0x211B0E, 0x332A14, 0xE8D290, 0xD8BD72},
    {"Silver", 0xAEB8C5, 0xE2E7EE, 0x0C1015, 0x06090D, 0x171D23, 0x282F39, 0xC9CFD6, 0xB8C0CA},
    {"Mint", 0x46C7A1, 0xA2EAD5, 0x07120F, 0x040908, 0x10201B, 0x1A322B, 0xA3DBC9, 0x86CDB6},
    {"Barby", 0xD55694, 0xF6A8D4, 0x180A13, 0x09040A, 0x28101E, 0x3B182D, 0xE29AB9, 0xD580A4},
    {"Violet", 0x8A63E8, 0xC7B4FF, 0x100B18, 0x07050B, 0x1B1227, 0x2A1B3D, 0xB8A0E7, 0xA48AD9},
    {"Amber", 0xC98721, 0xF1C27D, 0x130D06, 0x080502, 0x21160A, 0x34220D, 0xE3B26F, 0xD29A55},
    {"Teal", 0x168F91, 0x7CD1D2, 0x061113, 0x03090A, 0x0E1D1F, 0x163033, 0x8BCBCB, 0x70B9BA},
    {"Ruby", 0xB51F3D, 0xF09AA9, 0x14080D, 0x080407, 0x211018, 0x351824, 0xD77C92, 0xC7637C},
    {"Ice", 0x36BDD1, 0x9DEAF5, 0x071115, 0x03090B, 0x102027, 0x19323B, 0x89D6E1, 0x6DC7D4},
    {"Orange", 0xF07824, 0xFFC08C, 0x140A05, 0x090503, 0x241309, 0x391D0B, 0xEBA16F, 0xDD8952},
    {"Lime", 0x8DCB28, 0xC9EC86, 0x0C1206, 0x050802, 0x18200B, 0x273515, 0xB4D875, 0xA0C45B},
    {"Lavender", 0xA68AD8, 0xD9C9F4, 0x100D15, 0x07050A, 0x1D1727, 0x2E243D, 0xB9A9D5, 0xA490C5},
    {"Coral", 0xE36C65, 0xFFB8B1, 0x130A09, 0x080504, 0x221311, 0x35201E, 0xE5A09A, 0xD68B85},
};

int clampedHighContrastColor(int index) {
    return index >= 0 && index < Theme::kHighContrastColors ? index : Theme::kDefaultHighContrastColor;
}

} // namespace

const char *Theme::highContrastColorName(int index) {
    return kHighContrastPalette[clampedHighContrastColor(index)].name;
}

const char *Theme::colorThemeName(int index) {
    if (index < 0 || index >= colorThemeCount()) index = kDefaultColorTheme;
    return kColorThemes[index].name;
}

int Theme::colorThemeCount() {
    return int(sizeof(kColorThemes) / sizeof(kColorThemes[0]));
}

void Theme::setColorTheme(int index) {
    if (index < 0 || index >= colorThemeCount()) return;
    colorTheme_ = index;
    if (!highContrast_) applyPalette();
}

void Theme::setDarkMode(bool enabled) {
    darkMode_ = enabled;
    applyPalette();
}

void Theme::setHighContrast(bool enabled) {
    highContrast_ = enabled;
    applyPalette();
}

void Theme::setHighContrastColor(int index) {
    highContrastColor_ = clampedHighContrastColor(index);
    if (highContrast_) applyPalette();
}

void Theme::applyPalette() {
    if (highContrast_) {
        // One color carries every highlight: focus outline, progress, hints, selected values.
        const Color highlight = Color::rgb(kHighContrastPalette[highContrastColor_].rgb);
        background = Color::rgb(0x000000);
        backgroundLo = Color::rgb(0x000000);
        surface = Color::rgb(0x111111);
        surfaceHi = Color::rgb(0x292929);
        accent = highlight;
        textPrimary = Color::rgb(0xFFFFFF);
        textMuted = Color::rgb(0xD0D0D0);
        warning = Color::rgb(0xFF9A8E);
        success = highlight;
        favorite = highlight;
        focus = highlight;
        topBarText = textPrimary;
        topBarMuted = textMuted;
        topBarFocus = focus;
    } else {
        const ColorTheme &palette = kColorThemes[colorTheme_];
        if (darkMode_) {
            background = Color::rgb(palette.background);
            backgroundLo = Color::rgb(palette.backgroundLo);
            surface = Color::rgb(palette.surface);
            surfaceHi = Color::rgb(palette.surfaceHi);
            accent = Color::rgb(palette.accent);
            textPrimary = Color::rgb(0xFFFFFF);
            textMuted = Color::rgb(0x9AA4C0);
            warning = Color::rgb(0xE06A5A);
            success = Color::rgb(0x4C8DFF);
            favorite = accent;
            focus = Color::rgb(palette.focus);
            topBarText = textPrimary;
            topBarMuted = textMuted;
            topBarFocus = focus;
        } else {
            const Color white = Color::rgb(0xFFFFFF);
            const Color brightBackground = Color::rgb(palette.brightBackground);
            background = brightBackground;
            backgroundLo = Color::rgb(palette.brightBackgroundLo);
            surface = Color::lerp(white, brightBackground, 0.35f);
            surfaceHi = Color::lerp(white, brightBackground, 0.15f);
            const Color paletteAccent = Color::rgb(palette.accent);
            accent = paletteAccent.scaled(0.55f);
            textPrimary = Color::rgb(0x191B20);
            textMuted = Color::rgb(0x59616D);
            warning = Color::rgb(0xB74738);
            success = Color::rgb(0x2859A8);
            favorite = accent;
            focus = accent;
            topBarText = Color::rgb(0xFFFFFF);
            topBarMuted = Color::rgb(0xFFFFFF);
            topBarFocus = Color::rgb(palette.focus);
        }
    }
}

void Theme::setSpaceGroteskEnabled(bool enabled) {
    spaceGroteskEnabled_ = enabled;
    loadFonts();
}

int Theme::px(int designPx) const {
    const int scaled = int(long(designPx) * height_ / 1080);
    return (designPx > 0 && scaled < 1) ? 1 : scaled;
}
