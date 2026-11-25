#include <algorithm>
#include <utility>

#include "ui-framework/kit/Components.h"

namespace ui {

namespace {

// The token for a key in the user's layout (Theme::buttonLayout): the bottom face button is HintKey::A in every
// layout and prints the layout's name for it (Xbox A, PlayStation cross, Nintendo B); the shoulders and triggers
// follow too. Movement keys are drawn as arrows or as the D-pad icon.
std::string keyToken(ButtonLayout layout, HintKey key) {
    auto name = [&](PadButton b) { return buttonName(layout, b); };
    switch (key) {
        case HintKey::Dpad:      return "";            // drawn as the cross icon
        case HintKey::UpDown:    return "\xE2\x86\x91/\xE2\x86\x93";   // up/down arrows
        case HintKey::LeftRight: return "\xE2\x86\x90/\xE2\x86\x92";   // left/right arrows
        case HintKey::Shoulders: return name(PadButton::ShoulderLeft) + "/" + name(PadButton::ShoulderRight);
        case HintKey::Triggers:  return name(PadButton::TriggerLeft) + "/" + name(PadButton::TriggerRight);
        case HintKey::X:         return name(PadButton::X);
        case HintKey::Y:         return name(PadButton::Y);
        case HintKey::A:         return name(PadButton::A);
        case HintKey::AB:        return name(PadButton::A) + "/" + name(PadButton::B);
        case HintKey::Start:     return "Start";
        case HintKey::B:         return name(PadButton::B);
    }
    return "";
}

// Draws the D-pad cross: five rounded parts, as the bottom bar does today.
void drawDpad(Painter &p, const Theme &t, int x, int y, int size, Color c) {
    const int arm = std::max(5, size / 3);
    const int r = std::max(1, t.px(1));
    const int mid = (size - arm) / 2;
    p.fillRoundedRect({x + mid, y, arm, arm}, r, c);
    p.fillRoundedRect({x + mid, y + size - arm, arm, arm}, r, c);
    p.fillRoundedRect({x, y + mid, arm, arm}, r, c);
    p.fillRoundedRect({x + size - arm, y + mid, arm, arm}, r, c);
    p.fillRoundedRect({x + mid, y + mid, arm, arm}, r, c);
}

struct Laid {
    const Hint *hint;
    int holdWidth;   // the word "Hold" and its gap, 0 for a plain hint
    int keyWidth;
    int width;
};

}  // namespace

// One line of hints. `frame` draws the high-contrast frame around it (not around the hold hint, which has its bar).
// Fitting, as the Classic bottom bar does it (App.cpp drawBottomBar): a line that is too long first steps its text
// down one pixel at a time, never below the Small size; only then are the hints that are not essential dropped from
// the right. In high contrast the line is bold, Primary, and sits in a frame (surfaceHi, 2 px focus outline, radius 6,
// padding 10 x 4) that reaches 10 px beyond the left margin.
static int drawHintLine(Painter &p, const Theme &t, const Rect &area, std::vector<Hint> hints, bool frame) {
    int size = t.size(TextRole::Hint);
    const int smallest = std::min(size, t.size(TextRole::Small));
    int icon = std::max(size, t.px(18));
    const int gap = t.px(Space::gap);
    const int inner = t.px(8);   // between the key token and its action
    const bool contrast = t.highContrast();
    const FontRole actionFont = contrast ? FontRole::Bold : FontRole::Regular;
    const Color actionColor = contrast ? t.palette.textPrimary : t.text(TextTier::Muted);

    std::stable_sort(hints.begin(), hints.end(),
                     [](const Hint &a, const Hint &b) { return int(a.key) < int(b.key); });

    auto layout = [&](const std::vector<Hint> &list, std::vector<Laid> &out) {
        out.clear();
        int total = 0;
        for (const Hint &h : list) {
            Laid l;
            l.hint = &h;
            l.holdWidth = h.hold ? p.measure(actionFont, size, Catalog::get().tr("hint.hold")) + inner : 0;
            l.keyWidth = h.key == HintKey::Dpad ? icon : p.measure(FontRole::Bold, size, keyToken(t.buttonLayout(), h.key));
            l.width = l.holdWidth + l.keyWidth + inner + p.measure(actionFont, size, h.action.str());
            total += l.width + (out.empty() ? 0 : gap);
            out.push_back(l);
        }
        return total;
    };

    std::vector<Laid> laid;
    int total = layout(hints, laid);
    // Too long: step the text down, never below the Small size (the icon follows the text).
    while (total > area.w && size > smallest) {
        --size;
        icon = std::max(size, t.px(18));
        total = layout(hints, laid);
    }
    // Still too long: drop the least important hints from the right until the line fits.
    while (total > area.w) {
        int victim = -1;
        for (int i = int(hints.size()) - 1; i >= 0; --i)
            if (!hints[i].essential) { victim = i; break; }
        if (victim < 0) break;
        hints.erase(hints.begin() + victim);
        total = layout(hints, laid);
    }

    const int line = p.lineHeight(actionFont, size);
    const int y = area.y + (area.h - line) / 2;
    if (contrast && frame && !laid.empty()) {
        const Rect box{area.x - t.px(10), y - t.px(4), std::min(total, area.w) + 2 * t.px(10), line + 2 * t.px(4)};
        p.fillRoundedRect(box, t.px(6), t.palette.surfaceHi);
        p.strokeRoundedRect(box, t.px(6), std::max(1, t.px(Stroke::focus)), t.palette.focus);
    }
    int x = area.x;
    for (const Laid &l : laid) {
        if (l.holdWidth) p.drawText(actionFont, size, x, y, Catalog::get().tr("hint.hold"), actionColor);
        const int kx = x + l.holdWidth;
        if (l.hint->key == HintKey::Dpad)
            drawDpad(p, t, kx, area.y + (area.h - icon) / 2, icon, t.palette.textPrimary);
        else
            p.drawText(FontRole::Bold, size, kx, y, keyToken(t.buttonLayout(), l.hint->key), t.palette.textPrimary);
        p.drawText(actionFont, size, kx + l.keyWidth + inner, y, l.hint->action.str(), actionColor);
        x += l.width + gap;
    }
    return std::min(total, area.w);
}

int drawHints(Painter &p, const Theme &t, const Rect &area, std::vector<Hint> hints) {
    return drawHintLine(p, t, area, std::move(hints), true);
}

int drawHoldProgress(Painter &p, const Theme &t, const Rect &area, const Hint &hint, float progress) {
    Hint h = hint;
    h.hold = true;
    h.essential = true;
    // The line sits a little above the bar so the pair is centered in the bar's height.
    const int barH = t.px(6), barGap = t.px(5);
    const int line = p.lineHeight(FontRole::Regular, t.size(TextRole::Hint));
    const int pair = line + barGap + barH;
    const int top = area.y + (area.h - pair) / 2;
    const int used = drawHintLine(p, t, {area.x, top, area.w, line}, {h}, false);
    const int barW = std::min(area.w, std::max(used, t.px(240)));
    drawProgressBar(p, t, {area.x, top + line + barGap, barW, barH}, progress, false);
    return barW;
}

}  // namespace ui
