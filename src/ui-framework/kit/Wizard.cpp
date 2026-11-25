#include "ui-framework/kit/Wizard.h"

#include <algorithm>

namespace ui {

namespace {
constexpr size_t kHistoryDepth = 8;

bool isContinue(const WizardPage &page, WizardEvent e) {
    const HintKey k = page.continueKey();
    if (e == WizardEvent::Confirm) return k == HintKey::A || k == HintKey::AB;
    if (e == WizardEvent::Start) return k == HintKey::Start;
    return false;
}
}  // namespace

void WizardPage::cancel(Wizard &w) { w.cancelTo(back()); }

WizardPage *Wizard::page(PageId id) const {
    const auto it = pages_.find(id);
    return it == pages_.end() ? nullptr : it->second.get();
}

// ---------------------------------------------------------------- flow control

void Wizard::open(PageId first) {
    history_.clear();
    WizardPage *pg = page(first);
    if (!pg) { page_ = kClosed; return; }
    page_ = first;
    pg->enter(*this);
}

void Wizard::go(PageId target) { transition(target, false, true); }
void Wizard::cancelTo(PageId target) { transition(target, true, false); }

void Wizard::transition(PageId target, bool cancelled, bool remember) {
    WizardPage *cur = current();
    if (!cur) return;
    if (target == kPrevious) {
        if (history_.empty()) target = kClose;
        else { target = history_.back(); history_.pop_back(); }
        remember = false;
    }
    WizardPage *next = target >= 0 ? page(target) : nullptr;
    if (!next) {
        page_ = kClosed;
        history_.clear();
        cur->leave(cancelled);
        return;
    }
    // Only a page the user can return to is remembered: a Busy or Result page is never a Back target.
    if (remember && (cur->kind() == WizardPage::Kind::Step || cur->kind() == WizardPage::Kind::Capture)) {
        history_.push_back(page_);
        if (history_.size() > kHistoryDepth) history_.erase(history_.begin());
    }
    // Arriving at a page that is also the newest Back target (an explicit back to it, or Try again) makes it the
    // current page, not one to go back to: B there must not return to the same page.
    while (!history_.empty() && history_.back() == target) history_.pop_back();
    page_ = target;
    cur->leave(cancelled);
    next->enter(*this);
}

void Wizard::abort() {
    WizardPage *cur = current();
    page_ = kClosed;
    history_.clear();
    if (cur) cur->leave(true);
}

// ---------------------------------------------------------------- input

bool Wizard::handle(WizardEvent event) {
    WizardPage *pg = current();
    if (!pg) return false;
    switch (pg->kind()) {
        case WizardPage::Kind::Capture:
            pg->handle(event, *this);
            break;
        case WizardPage::Kind::Busy:
            if (event == WizardEvent::Back) { if (pg->cancellable()) pg->cancel(*this); }
            else pg->handle(event, *this);
            break;
        case WizardPage::Kind::Result:
            if (event == WizardEvent::Back) close();
            else pg->handle(event, *this);
            break;
        case WizardPage::Kind::Step: {
            if (event == WizardEvent::Back) { cancelTo(pg->back()); break; }
            Text reason;
            if (isContinue(*pg, event) && !pg->canContinue(reason)) break;   // swallowed; the reason line says why
            pg->handle(event, *this);
            break;
        }
    }
    return true;
}

bool Wizard::update(float deltaSeconds) {
    WizardPage *pg = current();
    if (!pg) return false;
    const PageId before = page_;
    const bool changed = pg->update(deltaSeconds, *this);
    return changed || page_ != before;
}

bool Wizard::wantsRawInput() const {
    const WizardPage *pg = current();
    return pg && pg->kind() == WizardPage::Kind::Capture && pg->rawInput();
}

std::vector<Hint> Wizard::hints() const {
    const WizardPage *pg = current();
    if (!pg) return std::vector<Hint>();
    std::vector<Hint> own = pg->hints();
    const bool stepLike = pg->kind() == WizardPage::Kind::Step || pg->kind() == WizardPage::Kind::Capture;
    Text reason;
    if (stepLike && !pg->canContinue(reason)) {
        const HintKey key = pg->continueKey();
        own.erase(std::remove_if(own.begin(), own.end(), [key](const Hint &h) { return h.key == key; }), own.end());
    }
    bool hasA = false, hasB = false;
    for (const Hint &h : own) {
        if (h.key == HintKey::A || h.key == HintKey::AB) hasA = true;
        if (h.key == HintKey::B || h.key == HintKey::AB) hasB = true;
    }
    switch (pg->kind()) {
        case WizardPage::Kind::Step: {
            if (hasB) break;
            const PageId back = pg->back();
            const bool leaves = back == kClose || (back == kPrevious && history_.empty());
            own.push_back(hint(HintKey::B, leaves ? "hint.close" : "hint.back", true));
            break;
        }
        case WizardPage::Kind::Busy:
            if (!hasB && pg->cancellable()) own.push_back(hint(HintKey::B, "hint.cancel", true));
            break;
        case WizardPage::Kind::Result:
            if (!hasA && !hasB) own.push_back(hint(HintKey::AB, "hint.done", true));
            else if (!hasB) own.push_back(hint(HintKey::B, "hint.close", true));
            break;
        case WizardPage::Kind::Capture:
            break;   // the page names everything, including its way out
    }
    return own;
}

// ---------------------------------------------------------------- drawing

Rect Wizard::panelRect(const Theme &t, const Rect &canvas, WizardFrame frame) {
    const int margin = t.px(Space::margin);
    int w = t.px(1120), h = t.px(760);
    if (frame == WizardFrame::Large) { w = t.px(1812); h = t.px(860); }
    w = std::min(w, canvas.w - 2 * margin);
    h = std::min(h, canvas.h - 2 * margin);
    return {canvas.x + (canvas.w - w) / 2, canvas.y + (canvas.h - h) / 2, w, h};
}

int Wizard::reasonStrip(Painter &p, const Theme &t) const {
    const WizardPage *pg = current();
    if (!pg || !(pg->kind() == WizardPage::Kind::Step || pg->kind() == WizardPage::Kind::Capture)) return 0;
    return p.lineHeight(FontRole::Regular, t.size(TextRole::Body)) + t.px(12);
}

Rect Wizard::bodyRect(Painter &p, const Theme &t, const Rect &canvas) const {
    const Rect panel = panelRect(t, canvas, frame_);
    const int pad = t.px(Space::pad);
    const int titleH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    Rect body{panel.x + pad, panel.y + pad + titleH + t.px(Space::titleGap), panel.w - 2 * pad, 0};
    body.h = std::max(0, panel.bottom() - pad - reasonStrip(p, t) - body.y);
    return body;
}

void Wizard::draw(Painter &p, const Theme &t, const Rect &canvas) const {
    WizardPage *pg = current();
    if (!pg) return;
    p.fillRect(canvas, Color{0, 0, 0, uint8_t(Alpha::scrimDialog)});
    const Rect panel = panelRect(t, canvas, frame_);
    const int pad = t.px(Space::pad);
    p.fillRoundedRect(panel, t.px(Radius::panel), t.palette.surfaceHi);
    if (pg->tone() != Tone::Neutral)
        p.strokeRoundedRect(panel, t.px(Radius::panel), std::max(1, t.px(Stroke::focus)), t.tone(pg->tone()));
    else if (t.borders())
        p.strokeRoundedRect(panel, t.px(Radius::panel), std::max(1, t.px(Stroke::focus)), Color::rgb(0xFFFFFF));

    // Title row: the title, and the optional "n / m" at the right (the title gives way to it).
    const int titleH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    const int inner = panel.w - 2 * pad;
    const std::string progress = pg->progress();
    int progressW = 0;
    if (!progress.empty()) {
        progressW = labelWidth(p, t, progress, TextRole::Small) + t.px(16);
        drawLabel(p, t, {panel.right() - pad - progressW + t.px(16), panel.y + pad, progressW - t.px(16), titleH}, progress,
                  TextRole::Small, TextTier::Muted, Align::Right);
    }
    drawLabel(p, t, {panel.x + pad, panel.y + pad, inner - progressW, titleH}, pg->title().str(), TextRole::Heading,
              TextTier::Primary, Align::Left, true);

    const Rect body = bodyRect(p, t, canvas);
    p.pushClip(body.inset(-t.px(4)).intersect(panel));
    pg->draw(p, t, body);
    p.popClip();

    Text reason;
    if (reasonStrip(p, t) > 0 && !pg->canContinue(reason) && !reason.empty()) {
        const int line = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
        drawLabel(p, t, {body.x, panel.bottom() - pad - line, body.w, line}, reason.str(), TextRole::Body, TextTier::Muted);
    }
}

}  // namespace ui
