#include "ui-framework/kit/Palettes.h"

namespace ui {

namespace {

// The lowest contrast of a text color over the three surfaces text is drawn on.
float worstContrast(const Color &fg, const Palette &p) {
    return std::min(contrastRatio(fg, p.background), std::min(contrastRatio(fg, p.surface), contrastRatio(fg, p.surfaceHi)));
}

// The faintest color between `from` and the page background that still reaches `minimum` on every surface.
// When `from` itself falls short it is returned unchanged (the palette's own text colors are then to blame).
Color fadeWhile(const Color &from, const Palette &p, float minimum) {
    if (worstContrast(from, p) < minimum) return from;
    float lo = 0.0f, hi = 1.0f;   // 0 = from, 1 = background
    for (int i = 0; i < 24; ++i) {
        const float mid = (lo + hi) / 2;
        if (worstContrast(Color::lerp(from, p.background, mid), p) >= minimum) lo = mid; else hi = mid;
    }
    return Color::lerp(from, p.background, lo);
}

// The accent moved toward white (dark surfaces) or black (light ones) until it can be read as text.
Color readableAccent(const Palette &p) {
    if (contrastRatio(p.accent, p.surfaceHi) >= 4.5f && contrastRatio(p.accent, p.background) >= 4.5f) return p.accent;
    const Color toward = p.dark ? Color::rgb(0xFFFFFF) : Color::rgb(0x000000);
    for (int i = 1; i <= 20; ++i) {
        const Color c = Color::lerp(p.accent, toward, i / 20.0f);
        if (contrastRatio(c, p.surfaceHi) >= 4.5f && contrastRatio(c, p.background) >= 4.5f) return c;
    }
    return toward;
}

// Strengthens a text color toward the primary text color until it reaches `minimum` on every surface.
Color strengthen(const Color &from, const Palette &p, float minimum) {
    if (worstContrast(from, p) >= minimum) return from;
    for (int i = 1; i <= 20; ++i) {
        const Color c = Color::lerp(from, p.textPrimary, i / 20.0f);
        if (worstContrast(c, p) >= minimum) return c;
    }
    return p.textPrimary;
}

void finishPalette(Palette &p) {
    p.textMuted = strengthen(p.textMuted, p, 4.5f);
    p.textFaint = fadeWhile(p.textMuted, p, 4.5f);
    p.textDisabled = fadeWhile(p.textMuted, p, 3.0f);
    p.accentText = readableAccent(p);
}

// The color themes, in the order an application stores them (Classic's Preferences index). The first eight are the
// framework's own set (the names say the color); the rest are further hue families. Every theme must pass
// `ui-demo --contrast` in dark and light mode (src/ui-framework/docs/THEMING.md). Three source colors were adjusted to pass:
//   Navy    surfaceHi 0x183765 -> 0x16335F   (error text on surfaceHi was 4.30 : 1, needs 4.5)
//   Maroon  brightBackground 0xD98B8B -> 0xEECBCB, brightBackgroundLo 0xC97575 -> 0xDEBABA
//           (light mode: the focus color on the page was 1.75 : 1, needs 3.0)
//   Ruby    accent 0xB51F3D -> 0xB92A47      (accent on surface was 2.80 : 1, needs 3.0)
const ColorTheme kThemes[] = {
    {"Blue",     0x4C8DFF, 0xB9D5FF, 0x0B0D14, 0x05070C, 0x161A26, 0x222839, 0xA6C6FF, 0x8AACEE},
    {"Red",      0xDD2020, 0xFFFFFF, 0x0D0C0C, 0x070707, 0x181717, 0x252323, 0xE6E6E6, 0xD9D9D9},
    {"Cobalt",   0x0070D1, 0x9ACBFF, 0x090B17, 0x050610, 0x11162A, 0x1C2542, 0x80B8E8, 0x66A2D8},
    {"Green",    0x107C10, 0xB0E9A7, 0x08110B, 0x040805, 0x101C13, 0x1B2D1E, 0x88BE88, 0x70AA70},
    {"Yellow",   0xF5C928, 0xFFF0A3, 0x090A0C, 0x030405, 0x15171A, 0x262A2F, 0xE6D991, 0xD7C975},
    {"Navy",     0xE32636, 0xFF9BA3, 0x071326, 0x030815, 0x102445, 0x16335F, 0xA6BCE8, 0x8FA7DB},
    {"Maroon",   0xFFD21F, 0xFFF1A3, 0x16080B, 0x080405, 0x241014, 0x391922, 0xEECBCB, 0xDEBABA},
    {"Emerald",  0x20D36B, 0xA0FFC2, 0x040D09, 0x020604, 0x0D1911, 0x173020, 0x91DDAE, 0x75CC96},
    {"Scarlet",  0xD92732, 0xFFD15A, 0x101113, 0x070809, 0x1C1E22, 0x2B2D33, 0xD7949A, 0xC47C84},
    {"Gold",     0xD8A928, 0xF4D77F, 0x120F08, 0x080704, 0x211B0E, 0x332A14, 0xE8D290, 0xD8BD72},
    {"Silver",   0xAEB8C5, 0xE2E7EE, 0x0C1015, 0x06090D, 0x171D23, 0x282F39, 0xC9CFD6, 0xB8C0CA},
    {"Mint",     0x46C7A1, 0xA2EAD5, 0x07120F, 0x040908, 0x10201B, 0x1A322B, 0xA3DBC9, 0x86CDB6},
    {"Pink",     0xD55694, 0xF6A8D4, 0x180A13, 0x09040A, 0x28101E, 0x3B182D, 0xE29AB9, 0xD580A4},
    {"Violet",   0x8A63E8, 0xC7B4FF, 0x100B18, 0x07050B, 0x1B1227, 0x2A1B3D, 0xB8A0E7, 0xA48AD9},
    {"Amber",    0xC98721, 0xF1C27D, 0x130D06, 0x080502, 0x21160A, 0x34220D, 0xE3B26F, 0xD29A55},
    {"Teal",     0x168F91, 0x7CD1D2, 0x061113, 0x03090A, 0x0E1D1F, 0x163033, 0x8BCBCB, 0x70B9BA},
    {"Ruby",     0xB92A47, 0xF09AA9, 0x14080D, 0x080407, 0x211018, 0x351824, 0xD77C92, 0xC7637C},
    {"Ice",      0x36BDD1, 0x9DEAF5, 0x071115, 0x03090B, 0x102027, 0x19323B, 0x89D6E1, 0x6DC7D4},
    {"Orange",   0xF07824, 0xFFC08C, 0x140A05, 0x090503, 0x241309, 0x391D0B, 0xEBA16F, 0xDD8952},
    {"Lime",     0x8DCB28, 0xC9EC86, 0x0C1206, 0x050802, 0x18200B, 0x273515, 0xB4D875, 0xA0C45B},
    {"Lavender", 0xA68AD8, 0xD9C9F4, 0x100D15, 0x07050A, 0x1D1727, 0x2E243D, 0xB9A9D5, 0xA490C5},
    {"Coral",    0xE36C65, 0xFFB8B1, 0x130A09, 0x080504, 0x221311, 0x35201E, 0xE5A09A, 0xD68B85},
};

const HighlightColor kHighlights[] = {
    {"White", 0xFFFFFF},      {"Pink", 0xFF8FD8},          {"Yellow", 0xFFE66D}, {"Cyan", 0x6FEAFF},
    {"Lime Green", 0xB4FF3C}, {"Electric Blue", 0x5B9BFF}, {"Orange", 0xFFA64D}, {"Lavender", 0xC9B3FF},
};

}  // namespace

const ColorTheme *builtinColorThemes(int &count) {
    count = int(sizeof(kThemes) / sizeof(kThemes[0]));
    return kThemes;
}

const HighlightColor *highlightColors(int &count) {
    count = int(sizeof(kHighlights) / sizeof(kHighlights[0]));
    return kHighlights;
}

Palette makePalette(const ColorTheme &theme, bool dark) {
    Palette p;
    p.dark = dark;
    if (dark) {
        p.background = Color::rgb(theme.background);
        p.backgroundLo = Color::rgb(theme.backgroundLo);
        p.surface = Color::rgb(theme.surface);
        p.surfaceHi = Color::rgb(theme.surfaceHi);
        p.accent = Color::rgb(theme.accent);
        p.textPrimary = Color::rgb(0xFFFFFF);
        p.textMuted = Color::rgb(0x9AA4C0);
        p.warning = Color::rgb(0xF0A33A);
        p.error = Color::rgb(0xEC7B6D);
        p.focus = Color::rgb(theme.focus);
        p.topBarFill = p.backgroundLo;
        p.topBarSolid = false;
        p.topBarText = p.textPrimary;
        p.topBarMuted = p.textMuted;
        p.topBarFocus = p.focus;
    } else {
        const Color white = Color::rgb(0xFFFFFF);
        const Color bright = Color::rgb(theme.brightBackground);
        p.background = bright;
        p.backgroundLo = Color::rgb(theme.brightBackgroundLo);
        p.surface = Color::lerp(white, bright, 0.35f);
        p.surfaceHi = Color::lerp(white, bright, 0.15f);
        p.accent = Color::rgb(theme.accent).scaled(0.55f);   // a darker shade of the accent on light pages
        p.textPrimary = Color::rgb(0x191B20);
        p.textMuted = Color::rgb(0x3D4551);
        p.warning = Color::rgb(0x8F5200);
        p.error = Color::rgb(0xB74738);
        p.focus = p.accent;
        // The top bar stays dark with light text and fades into the light page below it.
        p.topBarFill = Color::rgb(0x07090D);
        p.topBarSolid = true;
        p.topBarText = Color::rgb(0xFFFFFF);
        p.topBarMuted = Color::rgb(0xD5DAE4);
        p.topBarFocus = Color::rgb(theme.focus);
    }
    finishPalette(p);
    return p;
}

Palette makeHighContrastPalette(const HighlightColor &highlight) {
    // Black background; one color carries every highlight: the focus outline, progress, selected values,
    // the tab underline. With White as the highlight the plain borders are left out (they could not be told
    // from the focus outline).
    Palette p;
    p.dark = true;
    const Color h = Color::rgb(highlight.rgb);
    p.borders = highlight.rgb != 0xFFFFFF;
    p.background = Color::rgb(0x000000);
    p.backgroundLo = Color::rgb(0x000000);
    p.surface = Color::rgb(0x111111);
    p.surfaceHi = Color::rgb(0x292929);
    p.accent = h;
    p.textPrimary = Color::rgb(0xFFFFFF);
    p.textMuted = Color::rgb(0xD0D0D0);
    p.warning = Color::rgb(0xFFB866);
    p.error = Color::rgb(0xFF9A8E);
    p.focus = h;
    p.topBarFill = Color::rgb(0x000000);
    p.topBarSolid = false;
    p.topBarText = p.textPrimary;
    p.topBarMuted = p.textMuted;
    p.topBarFocus = h;
    finishPalette(p);
    return p;
}

}  // namespace ui
