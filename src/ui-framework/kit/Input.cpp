#include "ui-framework/kit/Input.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ui {

namespace {

float channel(uint8_t v) {
    const float s = v / 255.0f;
    return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
}
float luminance(const Color &c) { return 0.2126f * channel(c.r) + 0.7152f * channel(c.g) + 0.0722f * channel(c.b); }
float contrast(const Color &a, const Color &b) {
    const float la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
}

}  // namespace

Color contrastText(const Color &background) {
    const Color dark = Color::rgb(0x111318), light = Color::rgb(0xFFFFFF);
    return contrast(background, dark) >= contrast(background, light) ? dark : light;
}

ButtonColors readyButtonColors(const Palette &palette) {
    const Color white = Color::rgb(0xFFFFFF), black = Color::rgb(0x000000);
    // High contrast with a colored highlight: the highlight itself with a dark label (at least 7 : 1), so the
    // button keeps the highlight color instead of a muddy darkened one.
    if (palette.borders && contrast(palette.accent, Color::rgb(0x111318)) >= 7.0f)
        return {palette.accent, Color::rgb(0x111318)};
    // White on a darkened accent, as long as the fill still reads as the accent (at most 45 % toward black).
    for (int i = 0; i <= 9; ++i) {
        const Color fill = Color::lerp(palette.accent, black, i * 0.05f);
        if (contrast(fill, white) >= 4.5f) return {fill, white};
    }
    const Color dark = Color::rgb(0x111318);
    return {palette.accent, dark};
}

// ---------------------------------------------------------------- ActionButton

void drawActionButton(Painter &p, const Theme &t, const Rect &r, const std::string &label, ButtonState state, bool focused) {
    const int radius = t.px(Radius::panel);
    const int pad = t.px(22);
    if (state == ButtonState::Ready && focused) {
        // Ready and focused: a solid accent fill, a thin inset border in the label color, never growing.
        const ButtonColors colors = readyButtonColors(t.palette);
        const Color text = colors.text;
        p.fillRoundedRect(r, radius, colors.fill);
        const int inset = std::max(1, t.px(4));
        p.strokeRoundedRect(r.inset(inset), std::max(0, radius - inset), 1, text.withAlpha(200));
        drawLabelColor(p, t, {r.x + pad, r.y, r.w - 2 * pad, r.h}, label, TextRole::Body, text, Align::Center, true);
        return;
    }
    p.fillRoundedRect(r, radius, t.palette.surfaceHi);
    if (state == ButtonState::Unavailable && focused) {
        // Not ready but focused: the focus outline and a marker, so the focus is still found.
        drawFocusMarker(p, t, r);
        drawFocusFrame(p, t, r, radius);
    }
    const TextTier tier = state == ButtonState::Unavailable ? TextTier::Muted : TextTier::Primary;
    drawLabel(p, t, {r.x + pad, r.y, r.w - 2 * pad, r.h}, label, TextRole::Body, tier, Align::Center, true);
}

// ---------------------------------------------------------------- TextField

void drawTextField(Painter &p, const Theme &t, const Rect &r, const TextFieldSpec &spec) {
    const int radius = t.px(Radius::panel);
    p.fillRoundedRect(r, radius, t.palette.surfaceHi);
    if (t.borders()) p.strokeRoundedRect(r, radius, std::max(1, t.px(2)), Color::rgb(0xFFFFFF));
    if (spec.focused || spec.editing) drawFocusFrame(p, t, r, radius);
    const int pad = t.px(16);
    const Rect inner{r.x + pad, r.y, r.w - 2 * pad, r.h};
    const int size = t.size(TextRole::Body);
    if (spec.value.empty()) {
        drawLabel(p, t, inner, spec.placeholder.str(), TextRole::Body, TextTier::Faint);
        if (spec.editing) {
            const int h = p.lineHeight(FontRole::Regular, size);
            p.fillRect({inner.x, r.y + (r.h - h) / 2, std::max(2, t.px(2)), h}, t.palette.accent);
        }
        return;
    }
    const std::string shown = spec.masked ? std::string(spec.value.size(), '*') : spec.value;
    const std::string fitted = p.elide(FontRole::Regular, size, shown, inner.w - t.px(8));
    drawLabel(p, t, inner, fitted, TextRole::Body, TextTier::Primary);
    if (spec.editing) {
        const int w = p.measure(FontRole::Regular, size, fitted), h = p.lineHeight(FontRole::Regular, size);
        p.fillRect({inner.x + w + t.px(2), r.y + (r.h - h) / 2, std::max(2, t.px(2)), h}, t.palette.accent);
    }
}

// ---------------------------------------------------------------- Keyboard
// A port of Classic's OnScreenKeyboard (keys(), moveCursor(), handle(), render()).

namespace {

constexpr int kUnits = 16;     // Classic kKeyboardUnits
constexpr int kPadColumn = 13; // Classic kPadColumn: the number pad starts here (rows 0-3)

void wipe(std::string &s) {
    std::fill(s.begin(), s.end(), '\0');
    s.clear();
}

}  // namespace

Keyboard::~Keyboard() { wipe(value_); }

std::vector<Keyboard::Key> Keyboard::keys() const {
    std::vector<Key> out;
    auto character = [&](int row, int column, char c) { out.push_back({Kind::Character, c, nullptr, row, column, 1}); };
    const char *row0 = symbols_ ? "~!@#$%^&*()_" : "!@#$%^&*()-=";
    const char *row1 = symbols_ ? "[]{}|\\<>?:;=" : "qwertyuiop[]";
    const char *row2 = symbols_ ? "`\"'.,/?\\~+-_" : "asdfghjkl;'\\";
    for (int i = 0; row0[i]; ++i) character(0, i, row0[i]);
    for (int i = 0; row1[i]; ++i) character(1, i, row1[i]);
    for (int i = 0; row2[i]; ++i) character(2, i, row2[i]);

    out.push_back({Kind::Shift, 0, "keyboard.shift", 3, 0, 1});
    const char *row3 = symbols_ ? "zxcvbnm,./" : "`zxcvbnm,./";
    for (int i = 0; row3[i]; ++i) character(3, 1 + i, row3[i]);

    out.push_back({Kind::Symbols, 0, symbols_ ? "keyboard.letters" : "keyboard.symbols", 4, 0, 2});
    out.push_back({Kind::Space, 0, "keyboard.space", 4, 2, 3});
    out.push_back({Kind::Backspace, 0, "keyboard.delete", 4, 5, 2});
    out.push_back({Kind::Clear, 0, "keyboard.clear", 4, 7, 2});
    if (revealEnabled_) {
        out.push_back({Kind::Reveal, 0, masked_ ? "keyboard.show" : "keyboard.hide", 4, 9, 2});
        out.push_back({Kind::Done, 0, "keyboard.done", 4, 11, 2});
    } else {
        out.push_back({Kind::Done, 0, "keyboard.done", 4, 9, 3});
    }

    const char *pad[] = {"789", "456", "123", ".0-"};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col) character(row, kPadColumn + col, pad[row][col]);
    return out;
}

void Keyboard::move(int dx, int dy) {
    const std::vector<Key> all = keys();
    if (all.empty()) return;
    cursor_ = std::min(std::max(cursor_, 0), int(all.size()) - 1);
    const Key &current = all[size_t(cursor_)];
    const float center = float(current.column) + float(current.units) / 2.0f;
    const bool inNumberPad = current.column >= kPadColumn;
    const int lastRow = inNumberPad ? 3 : 4;
    int targetRow = current.row + dy;
    if (dy && targetRow < 0) targetRow = lastRow;
    else if (dy && targetRow > lastRow) targetRow = 0;
    int best = -1;
    float bestDistance = 10000.0f;
    for (size_t i = 0; i < all.size(); ++i) {
        const Key &candidate = all[i];
        if ((dy && candidate.row != targetRow) || (dx && candidate.row != current.row)) continue;
        // The number pad is a separate vertical block: Up/Down never cross between it and the keyboard.
        if (dy && (candidate.column >= kPadColumn) != inNumberPad) continue;
        const float candidateCenter = float(candidate.column) + float(candidate.units) / 2.0f;
        if (dx && (dx < 0 ? candidateCenter >= center : candidateCenter <= center)) continue;
        const float distance = std::abs(candidateCenter - center);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = int(i);
        }
    }
    // Left/Right stop at the ends of the row: they no longer jump to the opposite end (owner decision; Classic wrapped).
    if (best >= 0) cursor_ = best;
}

KeyResult Keyboard::erase() {
    if (value_.empty()) return KeyResult::None;
    if (masked_) value_.back() = '\0';     // a masked text is wiped as it shrinks
    value_.pop_back();
    return KeyResult::Changed;
}

KeyResult Keyboard::backspace() { return erase(); }

KeyResult Keyboard::space() {
    if (value_.size() >= maxLength_) return KeyResult::None;
    value_.push_back(' ');
    return KeyResult::Changed;
}

KeyResult Keyboard::press() {
    const std::vector<Key> all = keys();
    if (all.empty()) return KeyResult::None;
    cursor_ = std::min(std::max(cursor_, 0), int(all.size()) - 1);
    const Key &key = all[size_t(cursor_)];
    switch (key.kind) {
        case Kind::Character:
            if (value_.size() >= maxLength_) return KeyResult::None;
            value_.push_back(uppercase_ && std::isalpha(static_cast<unsigned char>(key.character))
                                 ? char(std::toupper(static_cast<unsigned char>(key.character)))
                                 : key.character);
            return KeyResult::Changed;
        case Kind::Shift: uppercase_ = !uppercase_; return KeyResult::None;
        case Kind::Symbols: symbols_ = !symbols_; cursor_ = 0; return KeyResult::None;
        case Kind::Space: return space();
        case Kind::Backspace: return erase();
        case Kind::Clear:
            if (value_.empty()) return KeyResult::None;
            clear();
            return KeyResult::Changed;
        case Kind::Reveal: masked_ = !masked_; return KeyResult::None;
        case Kind::Done: return KeyResult::Submitted;
    }
    return KeyResult::None;
}

void Keyboard::clear() { wipe(value_); }

namespace {
struct Metrics { int pad, gap, keyH, unit, width, x; };
Metrics metrics(const Theme &t, const Rect &area) {
    Metrics m;
    m.pad = t.px(12);
    m.gap = std::max(1, t.px(4));
    m.keyH = t.px(46);
    m.unit = std::max(1, std::min(t.px(70), (area.w - 2 * m.pad - m.gap * (kUnits - 1)) / kUnits));
    m.width = m.unit * kUnits + m.gap * (kUnits - 1);
    m.x = area.x + m.pad + std::max(0, (area.w - 2 * m.pad - m.width) / 2);
    return m;
}
}  // namespace

int Keyboard::height(const Theme &t) const {
    const int gap = std::max(1, t.px(4));
    return 2 * t.px(12) + 5 * t.px(46) + 4 * gap;
}

void Keyboard::draw(Painter &p, const Theme &t, const Rect &area) const {
    const std::vector<Key> all = keys();
    const Metrics m = metrics(t, area);
    const Rect panel{m.x - m.pad, area.y, m.width + 2 * m.pad, height(t)};
    p.fillRoundedRect(panel, t.px(Radius::panel), t.palette.surface);
    if (t.borders()) p.strokeRoundedRect(panel, t.px(Radius::panel), std::max(1, t.px(2)), Color::rgb(0xFFFFFF));
    const int radius = t.px(Radius::chip);
    // Focus last, so no neighbor paints over its outline.
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < all.size(); ++i) {
            const Key &key = all[i];
            const bool focused = int(i) == cursor_;
            if (focused != (pass == 1)) continue;
            const Rect frame{m.x + key.column * (m.unit + m.gap), panel.y + m.pad + key.row * (m.keyH + m.gap),
                             m.unit * key.units + m.gap * (key.units - 1), m.keyH};
            const bool active = (key.kind == Kind::Shift && uppercase_) || (key.kind == Kind::Symbols && symbols_) ||
                                (key.kind == Kind::Reveal && !masked_);
            if (focused) p.fillRoundedRect(frame, radius, t.palette.accent.withAlpha(100));
            else if (active) p.fillRoundedRect(frame, radius, t.palette.accent.withAlpha(55));
            else p.fillRoundedRect(frame, radius, t.palette.surfaceHi);
            if (focused) drawFocusFrame(p, t, frame, radius);   // a fill alone is not enough of a cue
            std::string label;
            if (key.kind == Kind::Character) {
                char c = key.character;
                if (uppercase_ && std::isalpha(static_cast<unsigned char>(c))) c = char(std::toupper(static_cast<unsigned char>(c)));
                label.assign(1, c);
            } else {
                label = Text::key(key.label).str();
            }
            drawLabel(p, t, frame, label, key.kind == Kind::Character ? TextRole::Body : TextRole::Small,
                      (focused || active) ? TextTier::Primary : TextTier::Muted, Align::Center, true);
        }
    }
}

}  // namespace ui
