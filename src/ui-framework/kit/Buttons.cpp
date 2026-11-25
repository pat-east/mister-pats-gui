#include "ui-framework/kit/Buttons.h"

#include <cctype>
#include <cstring>

namespace ui {

namespace {

struct Names {
    const char *a, *b, *x, *y, *shoulderLeft, *shoulderRight, *triggerLeft, *triggerRight;
};

// Position-based: a, b, x, y are the bottom, right, left and top buttons. The marks are the glyphs of the
// PlayStation pad (cross U+2715, circle U+25CB, square U+25A1, triangle U+25B3).
const Names kXbox = {"A", "B", "X", "Y", "LB", "RB", "LT", "RT"};
const Names kPlayStation = {"\xE2\x9C\x95", "\xE2\x97\x8B", "\xE2\x96\xA1", "\xE2\x96\xB3", "L1", "R1", "L2", "R2"};
const Names kNintendo = {"B", "A", "Y", "X", "L", "R", "ZL", "ZR"};

const Names &namesFor(ButtonLayout layout) {
    switch (layout) {
        case ButtonLayout::PlayStation: return kPlayStation;
        case ButtonLayout::Nintendo: return kNintendo;
        case ButtonLayout::Xbox: break;
    }
    return kXbox;
}

struct Token {
    const char *name;
    PadButton button;
};

const Token kTokens[] = {
    {"A", PadButton::A},           {"B", PadButton::B},           {"X", PadButton::X},
    {"Y", PadButton::Y},           {"START", PadButton::Start},   {"SELECT", PadButton::Select},
    {"LB", PadButton::ShoulderLeft},  {"L1", PadButton::ShoulderLeft},
    {"RB", PadButton::ShoulderRight}, {"R1", PadButton::ShoulderRight},
    {"LT", PadButton::TriggerLeft},   {"L2", PadButton::TriggerLeft},
    {"RT", PadButton::TriggerRight},  {"R2", PadButton::TriggerRight},
};

}  // namespace

std::string buttonName(ButtonLayout layout, PadButton button) {
    const Names &n = namesFor(layout);
    switch (button) {
        case PadButton::A: return n.a;
        case PadButton::B: return n.b;
        case PadButton::X: return n.x;
        case PadButton::Y: return n.y;
        case PadButton::Start: return "START";
        case PadButton::Select: return "SELECT";
        case PadButton::ShoulderLeft: return n.shoulderLeft;
        case PadButton::ShoulderRight: return n.shoulderRight;
        case PadButton::TriggerLeft: return n.triggerLeft;
        case PadButton::TriggerRight: return n.triggerRight;
    }
    return std::string();
}

ButtonLayout buttonLayoutFromName(const std::string &name) {
    std::string lower;
    for (char c : name) lower += char(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "playstation") return ButtonLayout::PlayStation;
    if (lower == "nintendo") return ButtonLayout::Nintendo;
    return ButtonLayout::Xbox;
}

const char *buttonLayoutName(ButtonLayout layout) {
    switch (layout) {
        case ButtonLayout::PlayStation: return "playstation";
        case ButtonLayout::Nintendo: return "nintendo";
        case ButtonLayout::Xbox: break;
    }
    return "xbox";
}

std::string formatButtons(ButtonLayout layout, const std::string &text) {
    if (text.find('{') == std::string::npos) return text;
    std::string out;
    out.reserve(text.size() + 8);
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '{') {
            const size_t close = text.find('}', i + 1);
            if (close != std::string::npos && close - i <= 8) {
                const std::string name = text.substr(i + 1, close - i - 1);
                bool known = false;
                for (const Token &t : kTokens) {
                    if (name == t.name) { out += buttonName(layout, t.button); known = true; break; }
                }
                if (known) { i = close + 1; continue; }
            }
        }
        out += text[i++];
    }
    return out;
}

}  // namespace ui
