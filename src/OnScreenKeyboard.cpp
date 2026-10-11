#include "OnScreenKeyboard.h"

#include <algorithm>
#include <cmath>
#include <cctype>

#include "Canvas.h"
#include "Theme.h"

namespace {

constexpr int kKeyboardUnits = 16;
constexpr int kPadColumn = 13;

} // namespace

std::vector<OnScreenKeyboard::Key> OnScreenKeyboard::keys() const {
    std::vector<Key> out;
    auto character = [&](int row, int column, char c) {
        out.push_back({Kind::Character, c, nullptr, row, column, 1});
    };
    const char *row0 = symbols_ ? "~!@#$%^&*()_" : "!@#$%^&*()-=";
    const char *row1 = symbols_ ? "[]{}|\\<>?:;=" : "qwertyuiop[]";
    const char *row2 = symbols_ ? "`\"'.,/?\\~+-_" : "asdfghjkl;'\\";
    for (int i = 0; row0[i]; ++i) character(0, i, row0[i]);
    for (int i = 0; row1[i]; ++i) character(1, i, row1[i]);
    for (int i = 0; row2[i]; ++i) character(2, i, row2[i]);

    out.push_back({Kind::Shift, 0, "Shift", 3, 0, 1});
    const char *row3 = symbols_ ? "zxcvbnm,./" : "`zxcvbnm,./";
    const int row3Start = 1;
    for (int i = 0; row3[i]; ++i) character(3, row3Start + i, row3[i]);

    out.push_back({Kind::Symbols, 0, symbols_ ? "ABC" : "Symbols", 4, 0, 2});
    out.push_back({Kind::Space, 0, "Space", 4, 2, 3});
    out.push_back({Kind::Backspace, 0, "Delete", 4, 5, 2});
    out.push_back({Kind::Clear, 0, "Clear", 4, 7, 2});
    if (revealEnabled_) {
        out.push_back({Kind::Reveal, 0, masked_ ? "Show" : "Hide", 4, 9, 2});
        out.push_back({Kind::Done, 0, "Done", 4, 11, 2});
    } else {
        out.push_back({Kind::Done, 0, "Done", 4, 9, 3});
    }

    const char *pad[] = {"789", "456", "123", ".0-"};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col)
            character(row, kPadColumn + col, pad[row][col]);
    return out;
}

void OnScreenKeyboard::moveCursor(int dx, int dy) {
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
        // The keypad is a separate vertical block. Its last row must not jump into the
        // keyboard's action row (and the top row must not jump across either).
        if (dy && (candidate.column >= kPadColumn) != inNumberPad) continue;
        const float candidateCenter = float(candidate.column) + float(candidate.units) / 2.0f;
        if (dx && (dx < 0 ? candidateCenter >= center : candidateCenter <= center)) continue;
        const float distance = std::abs(candidateCenter - center);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = int(i);
        }
    }
    // Horizontal movement wraps to the opposite end of this visual row when there is no
    // key further in the requested direction.
    if (dx && best < 0) {
        float edge = dx < 0 ? -1.0f : 10000.0f;
        for (size_t i = 0; i < all.size(); ++i) {
            const Key &candidate = all[i];
            if (candidate.row != current.row || int(i) == cursor_) continue;
            if ((candidate.column >= kPadColumn) != inNumberPad) continue;
            const float candidateCenter = float(candidate.column) + float(candidate.units) / 2.0f;
            if ((dx < 0 && candidateCenter > edge) || (dx > 0 && candidateCenter < edge)) {
                edge = candidateCenter;
                best = int(i);
            }
        }
    }
    if (best >= 0) cursor_ = best;
}

OnScreenKeyboard::Result OnScreenKeyboard::handle(Action action) {
    if (action == Action::Up) moveCursor(0, -1);
    else if (action == Action::Down) moveCursor(0, 1);
    else if (action == Action::Left) moveCursor(-1, 0);
    else if (action == Action::Right) moveCursor(1, 0);
    else if (action == Action::FaceXPress) {
        if (value_.empty()) return Result::None;
        if (masked_) value_.back() = '\0';
        value_.pop_back();
        return Result::Changed;
    }
    else if (action == Action::CycleView) {
        if (value_.size() >= maxLength_) return Result::None;
        value_.push_back(' ');
        return Result::Changed;
    }
    else if (action == Action::Confirm) {
        const std::vector<Key> all = keys();
        if (all.empty()) return Result::None;
        cursor_ = std::min(std::max(cursor_, 0), int(all.size()) - 1);
        const Key &key = all[size_t(cursor_)];
        switch (key.kind) {
        case Kind::Character:
            if (value_.size() >= maxLength_) return Result::None;
            value_.push_back(uppercase_ && std::isalpha(static_cast<unsigned char>(key.character))
                                 ? char(std::toupper(static_cast<unsigned char>(key.character)))
                                 : key.character);
            return Result::Changed;
        case Kind::Shift: uppercase_ = !uppercase_; return Result::None;
        case Kind::Symbols: symbols_ = !symbols_; cursor_ = 0; return Result::None;
        case Kind::Space:
            if (value_.size() >= maxLength_) return Result::None;
            value_.push_back(' ');
            return Result::Changed;
        case Kind::Backspace:
            if (value_.empty()) return Result::None;
            if (masked_) value_.back() = '\0';
            value_.pop_back();
            return Result::Changed;
        case Kind::Clear:
            if (value_.empty()) return Result::None;
            clear();
            return Result::Changed;
        case Kind::Reveal: masked_ = !masked_; return Result::None;
        case Kind::Done: return Result::Submitted;
        }
    }
    return Result::None;
}

void OnScreenKeyboard::clear() {
    std::fill(value_.begin(), value_.end(), '\0');
    value_.clear();
}

OnScreenKeyboard::~OnScreenKeyboard() { clear(); }

Rect OnScreenKeyboard::bounds(Theme &theme, const Rect &available) const {
    if (available.empty()) return {};
    const int gap = std::max(1, theme.px(4));
    const int unit = std::max(1, std::min(theme.px(70),
        (available.w - gap * (kKeyboardUnits - 1)) / kKeyboardUnits));
    const int width = unit * kKeyboardUnits + gap * (kKeyboardUnits - 1);
    const int keyHeight = std::max(1, std::min(theme.px(46),
        (available.h - gap * 4) / 5));
    const int height = keyHeight * 5 + gap * 4;
    return {available.x + std::max(0, (available.w - width) / 2),
            available.bottom() - height, width, height};
}

void OnScreenKeyboard::render(Canvas &canvas, Theme &theme, const Rect &area) const {
    const std::vector<Key> all = keys();
    const Rect board = bounds(theme, area);
    if (board.empty()) return;
    const int gap = std::max(1, theme.px(4));
    const int unit = (board.w - gap * (kKeyboardUnits - 1)) / kKeyboardUnits;
    const int keyHeight = (board.h - gap * 4) / 5;

    for (size_t i = 0; i < all.size(); ++i) {
        const Key &key = all[i];
        const int x = board.x + key.column * (unit + gap);
        const int y = board.y + key.row * (keyHeight + gap);
        const int width = unit * key.units + gap * (key.units - 1);
        const Rect rect{x, y, width, keyHeight};
        const bool focused = int(i) == cursor_;
        const bool active = (key.kind == Kind::Shift && uppercase_) ||
                            (key.kind == Kind::Symbols && symbols_) ||
                            (key.kind == Kind::Reveal && !masked_);
        canvas.fillRoundedRect(rect, theme.px(6),
                               focused || active ? theme.accent.withAlpha(focused ? 100 : 55)
                                                 : theme.surface.withAlpha(180));

        std::string label;
        if (key.kind == Kind::Character) {
            char c = key.character;
            if (uppercase_ && std::isalpha(static_cast<unsigned char>(c)))
                c = char(std::toupper(static_cast<unsigned char>(c)));
            label.assign(1, c);
        } else {
            label = key.label;
        }
        const int fontSize = key.kind == Kind::Character ? theme.sizeBody() : theme.sizeSmall();
        theme.bold().drawCentered(canvas, rect, label, fontSize,
                                  focused || active ? theme.textPrimary : theme.textMuted);
    }
}
