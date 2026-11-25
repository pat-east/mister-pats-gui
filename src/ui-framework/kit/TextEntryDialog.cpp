#include "ui-framework/kit/TextEntryDialog.h"

#include <algorithm>

namespace ui {

namespace {

constexpr int kFieldH = 56;

std::string trimmed(const std::string &s) {
    const char *ws = " \t\r\n";
    const size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(ws) - a + 1);
}

}  // namespace

TextEntryDialog::TextEntryDialog(const TextEntrySpec &spec) : spec_(spec) {
    keyboard_.setMaxLength(spec_.maxLength > 0 ? spec_.maxLength : std::string::npos);
    keyboard_.setMasked(spec_.masked);
    keyboard_.setValue(spec_.value);
}

TextEntryResult TextEntryDialog::handle(TextEntryEvent event) {
    KeyResult r = KeyResult::None;
    switch (event) {
        case TextEntryEvent::Up:     keyboard_.move(0, -1); return TextEntryResult::None;
        case TextEntryEvent::Down:   keyboard_.move(0, 1); return TextEntryResult::None;
        case TextEntryEvent::Left:   keyboard_.move(-1, 0); return TextEntryResult::None;
        case TextEntryEvent::Right:  keyboard_.move(1, 0); return TextEntryResult::None;
        case TextEntryEvent::Press:  r = keyboard_.press(); break;
        case TextEntryEvent::Delete: r = keyboard_.backspace(); break;
        case TextEntryEvent::Space:  r = keyboard_.space(); break;
        case TextEntryEvent::Done:   r = KeyResult::Submitted; break;
        case TextEntryEvent::Cancel:
            keyboard_.setValue(spec_.value);
            return TextEntryResult::Cancelled;
    }
    if (r == KeyResult::Submitted) {
        if (spec_.trim) keyboard_.setValue(trimmed(keyboard_.value()));
        return TextEntryResult::Submitted;
    }
    if (r == KeyResult::Changed && spec_.live) return TextEntryResult::Changed;
    return TextEntryResult::None;
}

std::vector<Hint> TextEntryDialog::hints() const {
    std::vector<Hint> h;
    h.push_back(hint(HintKey::Dpad, "hint.chooseKey", false));
    Hint a = hint(HintKey::A, "hint.type", true);
    if (!spec_.pressHint.empty()) a.action = spec_.pressHint;
    h.push_back(a);
    h.push_back(hint(HintKey::X, "hint.delete", false));
    h.push_back(hint(HintKey::Y, "hint.space", false));
    Hint start = hint(HintKey::Start, "hint.done", true);
    if (!spec_.doneHint.empty()) start.action = spec_.doneHint;
    h.push_back(start);
    Hint b = hint(HintKey::B, "hint.back", true);
    if (!spec_.cancelHint.empty()) b.action = spec_.cancelHint;
    h.push_back(b);
    return h;
}

Rect TextEntryDialog::panelRect(const Theme &t, const Rect &canvas) const {
    const int pad = t.px(Space::pad);
    const int w = std::min(t.px(1200), canvas.w - 2 * t.px(Space::margin));
    const int titleH = t.px(60);           // fixed: does not follow the text size
    const int fieldH = t.px(kFieldH);
    const int h = 2 * pad + titleH + fieldH + t.px(Space::titleGap) + keyboard_.height(t);
    return {canvas.x + (canvas.w - w) / 2, canvas.y + (canvas.h - h) / 2, w, h};
}

int TextEntryDialog::fieldHeight(const Theme &t) const { return t.px(kFieldH); }

void TextEntryDialog::drawField(Painter &p, const Theme &t, const Rect &r) const {
    TextFieldSpec f;
    f.placeholder = spec_.placeholder;
    f.value = keyboard_.value();
    f.masked = keyboard_.masked();
    f.editing = true;
    drawTextField(p, t, r, f);
}

void TextEntryDialog::draw(Painter &p, const Theme &t, const Rect &canvas) const {
    p.fillRect(canvas, Color{0, 0, 0, uint8_t(Alpha::scrimDialog)});
    const Rect panel = panelRect(t, canvas);
    const int pad = t.px(Space::pad);
    drawSurface(p, t, panel, SurfaceRole::Raised, t.px(Radius::panel));
    if (t.borders()) p.strokeRoundedRect(panel, t.px(Radius::panel), std::max(1, t.px(Stroke::focus)), Color::rgb(0xFFFFFF));

    const int titleH = t.px(60);
    int y = panel.y + pad;
    drawLabel(p, t, {panel.x + pad, y, panel.w - 2 * pad, titleH - t.px(8)}, spec_.title.str(), TextRole::Heading,
              TextTier::Primary, Align::Left, true);
    y += titleH;
    TextFieldSpec f;
    f.placeholder = spec_.placeholder;
    f.value = keyboard_.value();
    f.masked = keyboard_.masked();   // the Show / Hide key can reveal it
    f.editing = true;
    const int fieldH = t.px(kFieldH);
    drawTextField(p, t, {panel.x + pad, y, panel.w - 2 * pad, fieldH}, f);
    y += fieldH + t.px(Space::titleGap);
    keyboard_.draw(p, t, {panel.x + pad, y, panel.w - 2 * pad, keyboard_.height(t)});
}

}  // namespace ui
