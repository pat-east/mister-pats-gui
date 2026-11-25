#include "ui-framework/kit/Toast.h"

#include <algorithm>

namespace ui {

void Toast::show(const Text &text, Tone tone, float minSeconds) {
    text_ = text;
    tone_ = tone == Tone::Done ? Tone::Done : Tone::Neutral;
    left_ = std::max(duration(text.str()), minSeconds);   // a newer toast replaces the showing one; its time starts over
}

float Toast::duration(const std::string &text) {
    // Count characters, not bytes: UTF-8 continuation bytes (10xxxxxx) do not start one.
    long chars = 0;
    for (unsigned char ch : text) if ((ch & 0xC0) != 0x80) ++chars;
    return 3.5f + float(std::max(0L, chars - 40) / 40);
}

bool Toast::update(float deltaSeconds, bool suppressed) {
    if (left_ <= 0.0f || suppressed) return false;
    left_ -= deltaSeconds;
    if (left_ > 0.0f) return false;
    left_ = 0.0f;
    return true;
}

Rect Toast::rect(Painter &p, const Theme &t, const Rect &canvas) const {
    if (!visible()) return Rect{};
    const TextRole role = TextRole::Body;
    const int h = p.lineHeight(FontRole::Regular, t.size(role)) + t.px(16);
    const int icon = t.px(16), pad = t.px(16), gap = t.px(8);
    const int maxW = canvas.w * 6 / 10;                              // elided at 60 % of the screen
    const int textW = std::min(labelWidth(p, t, text_.str(), role), maxW - 2 * pad - icon - gap);
    const int w = 2 * pad + icon + gap + std::max(0, textW);
    const int right = canvas.right() - t.marginX();
    const int bottom = canvas.bottom() - t.bottomBarHeight() - t.px(12);   // never over the hints
    return Rect{right - w, bottom - h, w, h};
}

void Toast::draw(Painter &p, const Theme &t, const Rect &canvas) const {
    if (!visible()) return;
    const Rect r = rect(p, t, canvas);
    drawSurface(p, t, r, SurfaceRole::Raised, r.h / 2);
    if (t.borders()) p.strokeRoundedRect(r, r.h / 2, std::max(1, t.px(2)), Color::rgb(0xFFFFFF));
    const int icon = t.px(16), pad = t.px(16), gap = t.px(8);
    drawIcon(p, iconForTone(tone_), {r.x + pad, r.y + (r.h - icon) / 2, icon, icon}, t.tone(tone_), std::max(2, t.px(2)));
    const int textX = r.x + pad + icon + gap;
    drawLabel(p, t, {textX, r.y, r.right() - pad - textX, r.h}, text_.str(), TextRole::Body, TextTier::Primary);
}

}  // namespace ui
