#include "ui-framework/kit/Modal.h"

#include <algorithm>
#include <cstdio>

namespace ui {

// ---------------------------------------------------------------- factories

Modal Modal::info(const Text &title, const Text &body) {
    Modal m;
    m.kind_ = ModalKind::Info;
    m.title_ = title;
    m.paragraphs_.push_back(body);
    return m;
}

Modal Modal::confirm(const Text &title, const Text &body) {
    Modal m;
    m.kind_ = ModalKind::Confirm;
    m.tone_ = Tone::Action;
    m.title_ = title;
    m.paragraphs_.push_back(body);
    return m;
}

Modal Modal::error(const Text &title, const Text &whatHappened, const Text &whatToDo, const Text &details, bool retry) {
    Modal m;
    m.kind_ = ModalKind::Error;
    m.tone_ = Tone::Error;
    m.title_ = title;
    m.paragraphs_.push_back(whatHappened);
    m.paragraphs_.push_back(whatToDo);
    m.details_ = details;
    m.retry_ = retry;
    return m;
}

Modal Modal::result(Tone tone, const Text &title, const Text &status, const Text &body) {
    Modal m;
    m.kind_ = ModalKind::Result;
    m.tone_ = tone;
    m.title_ = title;
    m.status_ = status;
    m.paragraphs_.push_back(body);
    return m;
}

Modal Modal::progress(const Text &title, const Text &status, float fraction) {
    Modal m;
    m.kind_ = ModalKind::Progress;
    m.title_ = title;
    m.status_ = status;
    m.fraction_ = fraction;
    return m;
}

Modal Modal::choose(const Text &title, std::vector<ModalChoice> choices) {
    Modal m;
    m.kind_ = ModalKind::Choose;
    m.size_ = ModalSize::Medium;
    m.title_ = title;
    m.choices_ = std::move(choices);
    return m;
}

Modal Modal::viewer(std::vector<ImagePtr> images) {
    Modal m;
    m.kind_ = ModalKind::Viewer;
    m.images_ = std::move(images);
    return m;
}

// ---------------------------------------------------------------- behavior

ModalResult Modal::handle(ModalEvent event) {
    switch (kind_) {
        case ModalKind::Info:
        case ModalKind::Result:
            return (event == ModalEvent::Confirm || event == ModalEvent::Back) ? ModalResult::Closed : ModalResult::Stay;
        case ModalKind::Confirm:
            if (event == ModalEvent::Confirm) return ModalResult::Confirmed;
            if (event == ModalEvent::Back) return ModalResult::Cancelled;
            return ModalResult::Stay;
        case ModalKind::Error:
            if (event == ModalEvent::Confirm) return retry_ ? ModalResult::Confirmed : ModalResult::Closed;
            if (event == ModalEvent::Back) return ModalResult::Closed;
            return ModalResult::Stay;
        case ModalKind::Progress:
            return event == ModalEvent::Back ? ModalResult::Cancelled : ModalResult::Stay;
        case ModalKind::Choose:
            if (event == ModalEvent::Up) cursor_ = ListLayout::move(cursor_, int(choices_.size()), -1);
            else if (event == ModalEvent::Down) cursor_ = ListLayout::move(cursor_, int(choices_.size()), 1);
            else if (event == ModalEvent::Confirm) return ModalResult::Confirmed;
            else if (event == ModalEvent::Back) return ModalResult::Cancelled;
            return ModalResult::Stay;
        case ModalKind::Viewer:
            if (event == ModalEvent::Left) cursor_ = ListLayout::move(cursor_, int(images_.size()), -1);
            else if (event == ModalEvent::Right) cursor_ = ListLayout::move(cursor_, int(images_.size()), 1);
            else if (event == ModalEvent::Confirm || event == ModalEvent::Back) return ModalResult::Closed;
            return ModalResult::Stay;
    }
    return ModalResult::Stay;
}

std::vector<Hint> Modal::hints() const {
    std::vector<Hint> h;
    switch (kind_) {
        case ModalKind::Info:     h.push_back(hint(HintKey::AB, "hint.close", true)); break;
        case ModalKind::Confirm:  h.push_back(hint(HintKey::A, "hint.confirm", true)); h.push_back(hint(HintKey::B, "hint.cancel", true)); break;
        case ModalKind::Error:
            if (retry_) { h.push_back(hint(HintKey::A, "hint.tryAgain", true)); h.push_back(hint(HintKey::B, "hint.close", true)); }
            else h.push_back(hint(HintKey::AB, "hint.close", true));
            break;
        case ModalKind::Result:   h.push_back(hint(HintKey::AB, "hint.done", true)); break;
        case ModalKind::Progress: h.push_back(hint(HintKey::B, "hint.cancel", true)); break;
        case ModalKind::Choose:
            h.push_back(hint(HintKey::UpDown, "hint.move", false));
            h.push_back(hint(HintKey::A, "hint.select", true));
            h.push_back(hint(HintKey::B, "hint.cancel", true));
            break;
        case ModalKind::Viewer:
            h.push_back(hint(HintKey::LeftRight, "hint.browse", true));
            h.push_back(hint(HintKey::AB, "hint.close", true));
            break;
    }
    return h;
}

// ---------------------------------------------------------------- drawing

int Modal::paragraphsHeight(Painter &p, const Theme &t, int width) const {
    const int step = p.lineHeight(FontRole::Regular, t.size(TextRole::Body)) + t.px(Space::lineExtra);
    int total = 0;
    for (size_t i = 0; i < paragraphs_.size(); ++i)
        total += int(wrapText(p, t, paragraphs_[i].str(), TextRole::Body, false, width).size()) * step +
                 (i + 1 < paragraphs_.size() ? t.px(10) : 0);
    return total;
}

Rect Modal::panelRect(Painter &p, const Theme &t, const Rect &canvas) const {
    const int margin = t.px(Space::margin);
    int w = t.px(760), h = 0;
    if (size_ == ModalSize::Medium) { w = t.px(1120); h = t.px(760); }
    if (size_ == ModalSize::Large) { w = t.px(1812); h = t.px(860); }
    w = std::min(w, canvas.w - 2 * margin);
    if (size_ == ModalSize::Small) {
        // Small panels fit their content.
        const int pad = t.px(Space::pad);
        const int inner = w - 2 * pad;
        h = 2 * pad + p.lineHeight(FontRole::Bold, t.size(TextRole::Heading)) + t.px(Space::titleGap);
        if (kind_ == ModalKind::Progress)
            h += t.px(12) + t.px(18) + p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
        else
            h += paragraphsHeight(p, t, inner);
        if (kind_ == ModalKind::Result) h += p.lineHeight(FontRole::Bold, t.size(TextRole::Body)) + t.px(12);
        if (!details_.empty()) h += t.px(14) + p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
    }
    h = std::min(h, canvas.h - 2 * margin);
    return {canvas.x + (canvas.w - w) / 2, canvas.y + (canvas.h - h) / 2, w, h};
}

void Modal::draw(Painter &p, const Theme &t, const Rect &canvas) const {
    if (kind_ == ModalKind::Viewer) {
        // The picture at its own size, never scaled, with its position: no panel.
        if (images_.empty()) return;
        const ImagePtr &img = images_[size_t(cursor_)];
        const int x = canvas.x + (canvas.w - img->width()) / 2, y = canvas.y + (canvas.h - img->height()) / 2 - t.px(20);
        p.drawImage(*img, {x, y, img->width(), img->height()}, 255, t.px(Radius::chip));
        char count[32];
        std::snprintf(count, sizeof(count), "%d / %d", cursor_ + 1, int(images_.size()));
        drawLabel(p, t, {canvas.x, y + img->height() + t.px(16), canvas.w, t.px(34)}, count, TextRole::Body,
                  TextTier::Primary, Align::Center, true);
        return;
    }

    const Rect panel = panelRect(p, t, canvas);
    const int pad = t.px(Space::pad);
    p.fillRoundedRect(panel, t.px(Radius::panel), t.palette.surfaceHi);
    if (tone_ != Tone::Neutral)
        p.strokeRoundedRect(panel, t.px(Radius::panel), std::max(1, t.px(Stroke::focus)), t.tone(tone_));

    const int titleH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    drawLabel(p, t, {panel.x + pad, panel.y + pad, panel.w - 2 * pad, titleH}, title_.str(), TextRole::Heading,
              TextTier::Primary, Align::Left, true);
    Rect inner{panel.x + pad, panel.y + pad + titleH + t.px(Space::titleGap), panel.w - 2 * pad, 0};
    inner.h = panel.bottom() - pad - inner.y;
    const int step = p.lineHeight(FontRole::Regular, t.size(TextRole::Body)) + t.px(Space::lineExtra);

    int y = inner.y;
    if (kind_ == ModalKind::Result && !status_.empty()) {
        drawStatus(p, t, inner.x, y, tone_, status_.str(), TextRole::Body);
        y += p.lineHeight(FontRole::Bold, t.size(TextRole::Body)) + t.px(12);
    }
    for (size_t i = 0; i < paragraphs_.size(); ++i) {
        const int used = drawTextBlock(p, t, {inner.x, y, inner.w, inner.bottom() - y}, paragraphs_[i].str(),
                                       TextRole::Body, i == 0 ? TextTier::Primary : TextTier::Muted);
        y += used + t.px(10);
    }
    (void)step;
    if (!details_.empty()) {
        // The technical reason is a small, secondary line, never the headline.
        const std::string line = Text::key("modal.details").str() + ": " + details_.str();
        drawLabel(p, t, {inner.x, panel.bottom() - pad - p.lineHeight(FontRole::Regular, t.size(TextRole::Small)), inner.w,
                         p.lineHeight(FontRole::Regular, t.size(TextRole::Small))}, line, TextRole::Small, TextTier::Faint);
    }
    if (kind_ == ModalKind::Progress) {
        drawProgressBar(p, t, {inner.x, y, inner.w, t.px(12)}, fraction_, true);
        y += t.px(12) + t.px(18);
        drawLabel(p, t, {inner.x, y, inner.w, p.lineHeight(FontRole::Regular, t.size(TextRole::Body))}, status_.str(),
                  TextRole::Body, TextTier::Muted);
    }
    if (kind_ == ModalKind::Choose) {
        const int frame = listItemFrameHeight(t, true), pitch = listItemPitch(t, true);
        list_.configure(inner, frame, pitch);
        list_.ensureVisible(cursor_);
        const int last = std::min(int(choices_.size()), list_.scroll() + list_.visibleRows());
        for (int i = list_.scroll(); i < last; ++i) {
            ListItemSpec spec;
            spec.kind = ListItemKind::Choice;
            spec.title = choices_[size_t(i)].title;
            spec.detail = choices_[size_t(i)].detail;
            spec.checked = choices_[size_t(i)].checked;
            spec.status = Text::data("");
            ListItemState st;
            st.cursor = i == cursor_;
            drawListItem(p, t, list_.row(i), spec, st);
        }
    }
}

// ---------------------------------------------------------------- the stack

ModalResult ModalStack::handle(ModalEvent event) {
    if (stack_.empty()) return ModalResult::Stay;
    const ModalResult result = stack_.back().handle(event);
    if (result != ModalResult::Stay) stack_.pop_back();
    return result;
}

std::vector<Hint> ModalStack::hints() const {
    return stack_.empty() ? std::vector<Hint>() : stack_.back().hints();
}

void ModalStack::draw(Painter &p, const Theme &t, const Rect &canvas) const {
    for (size_t i = 0; i < stack_.size(); ++i) {
        const bool viewer = stack_[i].kind() == ModalKind::Viewer;
        const int alpha = i > 0 ? Alpha::scrimNested : (viewer ? Alpha::scrimViewer : Alpha::scrimDialog);
        p.fillRect(canvas, Color{0, 0, 0, uint8_t(alpha)});
        stack_[i].draw(p, t, canvas);
    }
}

}  // namespace ui
