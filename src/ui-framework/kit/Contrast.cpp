#include "ui-framework/kit/Contrast.h"

#include <algorithm>
#include <cmath>

#include "ui-framework/kit/Input.h"

namespace ui {

std::vector<ContrastCheck> checkContrast(const Theme &t) {
    const Palette &p = t.palette;
    std::vector<ContrastCheck> out;
    auto add = [&](const char *name, const Color &fg, const Color &bg, float minimum) {
        out.push_back({name, contrastRatio(fg, bg), minimum});
    };
    struct Surface { const char *name; Color color; };
    const Surface surfaces[] = {{"background", p.background}, {"surface", p.surface}, {"surfaceHi", p.surfaceHi}};
    struct Tier { const char *name; TextTier tier; float minimum; };
    const Tier tiers[] = {{"text primary", TextTier::Primary, 4.5f}, {"text muted", TextTier::Muted, 4.5f},
                          {"text faint", TextTier::Faint, 4.5f}, {"text disabled", TextTier::Disabled, 3.0f}};
    for (const Tier &tier : tiers)
        for (const Surface &s : surfaces) add((std::string(tier.name) + " on " + s.name).c_str(), t.text(tier.tier), s.color, tier.minimum);

    add("accent text on surfaceHi", p.accentText, p.surfaceHi, 4.5f);   // values, status words
    add("accent text on background", p.accentText, p.background, 4.5f);
    add("accent on surface (graphics)", p.accent, p.surface, 3.0f);     // markers, progress fill
    add("focus on surfaceHi", p.focus, p.surfaceHi, 3.0f);              // the focus outline
    add("focus on background", p.focus, p.background, 3.0f);
    add("warning text on surfaceHi", p.warning, p.surfaceHi, 4.5f);
    add("error text on surfaceHi", p.error, p.surfaceHi, 4.5f);
    add("checkbox outline on surfaceHi", p.textMuted, p.surfaceHi, 3.0f);
    add("top bar text on top bar", p.topBarText, p.topBarFill, 4.5f);
    add("top bar muted on top bar", p.topBarMuted, p.topBarFill, 4.5f);
    add("top bar underline on top bar", p.topBarFocus, p.topBarFill, 3.0f);
    {
        const ButtonColors b = readyButtonColors(p);
        add("button label on its fill", b.text, b.fill, 4.5f);
    }
    return out;
}

}  // namespace ui
