#include "app/SearchHeader.h"

#include "app/TextEntryInput.h"

namespace patsgui {

namespace {
int nextOwner() {
    static int next = 0;
    return ++next;   // 0 is the library's shared slot; every header gets a number of its own
}
}  // namespace

SearchHeader::SearchHeader(Library &library) : library_(library), owner_(nextOwner()) {}

void SearchHeader::open() {
    ui::TextEntrySpec spec;
    spec.title = placeholder_;
    spec.placeholder = placeholder_;
    spec.value = query_;
    spec.live = false;        // the search runs when the user is done, not on every key (D2)
    spec.maxLength = 64;      // Classic's limit
    spec.trim = true;
    spec.pressHint = ui::Text::key("hint.enter");     // Classic search keyboard: A Enter, Start Search, B Close
    spec.doneHint = ui::Text::key("hint.search");
    spec.cancelHint = ui::Text::key("hint.close");
    entry_ = ui::TextEntryDialog(spec);
    typing_ = true;
}

bool SearchHeader::handle(Event event, bool fieldFocused, Out &out) {
    out = Out::None;
    if (typing_) {
        ui::TextEntryEvent te;
        if (!mapTextEntryEvent(event, te)) return true;   // the keyboard locks the screen behind it
        const ui::TextEntryResult r = entry_.handle(te);
        if (r == ui::TextEntryResult::Submitted) {
            typing_ = false;
            query_ = entry_.value();
            out = Out::Submitted;
        } else if (r == ui::TextEntryResult::Cancelled) {
            typing_ = false;   // nothing changes
        }
        return true;
    }
    if (event == Event::Start || (event == Event::Confirm && fieldFocused)) {
        open();
        return true;
    }
    if (event == Event::Back && fieldFocused && !query_.empty()) {
        query_.clear();
        out = Out::Cleared;
        return true;
    }
    return false;
}

void SearchHeader::fill(ui::PageHeaderSpec &header, bool fieldFocused) const {
    header.hasSearch = true;
    header.search.placeholder = placeholder_;
    header.search.value = query_;
    header.search.focused = fieldFocused && !typing_;
}

void SearchHeader::drawDialog(ui::Painter &p, const ui::Theme &t, const ui::Rect &canvas) const {
    if (typing_) entry_.draw(p, t, canvas);
}

}  // namespace patsgui
