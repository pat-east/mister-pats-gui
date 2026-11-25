#include "app/settings/SettingsPage.h"

namespace patsgui {

// ------------------------------------------------------------------------------------------------ PageStack

void PageStack::push(std::unique_ptr<SettingsPage> page) {
    if (!page) return;
    if (!inCall_) {
        pages_.push_back(std::move(page));
        return;
    }
    pending_.push_back({OpKind::Push, std::move(page)});
}

void PageStack::pop() {
    if (!inCall_) {
        closeTop();
        return;
    }
    pending_.push_back({OpKind::Pop, nullptr});
}

void PageStack::replace(std::unique_ptr<SettingsPage> page) {
    pop();
    push(std::move(page));
}

void PageStack::clear() {
    pending_.clear();
    while (!pages_.empty()) closeTop();
}

void PageStack::closeTop() {
    if (pages_.empty()) return;
    std::unique_ptr<SettingsPage> page = std::move(pages_.back());
    pages_.pop_back();
    page->onClose();
}

void PageStack::apply() {
    // A page that closes itself or opens another does so while it runs; the operations run now, in order.
    std::vector<Op> ops;
    ops.swap(pending_);
    for (Op &op : ops) {
        if (op.kind == OpKind::Pop) closeTop();
        else if (op.page) pages_.push_back(std::move(op.page));
    }
}

bool PageStack::locked() const {
    return !pages_.empty() && pages_.back()->locked();
}

std::vector<ui::Hint> PageStack::hints() const {
    return pages_.empty() ? std::vector<ui::Hint>() : pages_.back()->hints();
}

bool PageStack::update(float deltaSeconds) {
    bool redraw = false;
    if (pages_.empty()) return redraw;
    inCall_ = true;
    if (pages_.back()->update(deltaSeconds, *this)) redraw = true;
    inCall_ = false;
    if (!pending_.empty()) {
        apply();
        redraw = true;
    }
    return redraw;
}

bool PageStack::handle(Event event) {
    if (pages_.empty()) return false;
    SettingsPage *page = pages_.back().get();
    const bool lock = page->locked();
    inCall_ = true;
    const bool used = page->handle(event, *this);
    inCall_ = false;
    bool popQueued = false;
    for (const Op &op : pending_)
        if (op.kind == OpKind::Pop) popQueued = true;
    if (!used && event == Event::Back && !popQueued) pending_.push_back({OpKind::Pop, nullptr});   // Back always leaves
    apply();
    return used || lock || event == Event::Back;
}

bool PageStack::coversPanes() const {
    for (const auto &page : pages_)
        if (page->presentation() == PagePresentation::Content) return true;
    return false;
}

void PageStack::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) {
    size_t first = 0;
    for (size_t i = pages_.size(); i > 0; --i)
        if (pages_[i - 1]->presentation() == PagePresentation::Content) { first = i - 1; break; }
    for (size_t i = first; i < pages_.size(); ++i) pages_[i]->draw(p, t, content);
}

bool PageStack::dialogOpen() const {
    return !pages_.empty() && pages_.back()->presentation() == PagePresentation::Modal;
}

}  // namespace patsgui
