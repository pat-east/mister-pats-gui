#include "ui-framework/kit/WizardParts.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace ui {

// ---------------------------------------------------------------- text

Rect wizardTextColumn(const Theme &t, const Rect &body) {
    return {body.x, body.y, std::min(body.w, readingWidth(t)), body.h};
}

static int paragraphStep(Painter &p, const Theme &t) {
    return p.lineHeight(FontRole::Regular, t.size(TextRole::Body)) + t.px(Space::lineExtra);
}

int drawWizardParagraphs(Painter &p, const Theme &t, const Rect &body, const std::vector<WizardParagraph> &paragraphs) {
    const Rect col = wizardTextColumn(t, body);
    int y = col.y;
    for (size_t i = 0; i < paragraphs.size(); ++i) {
        if (y >= col.bottom()) break;
        const int used = drawTextBlock(p, t, {col.x, y, col.w, col.bottom() - y}, paragraphs[i].text.str(), TextRole::Body,
                                       paragraphs[i].tier, paragraphs[i].bold);
        y += used + (i + 1 < paragraphs.size() ? t.px(10) : 0);
    }
    return y - col.y;
}

bool WizardScrollText::handle(WizardEvent event) {
    const int page = std::max(1, visible_ - 1);
    const int last = std::max(0, total_ - visible_);
    switch (event) {
        case WizardEvent::Up:          scroll_ = std::max(0, scroll_ - 1); return true;
        case WizardEvent::Down:        scroll_ = std::min(last, scroll_ + 1); return true;
        case WizardEvent::Left:
        case WizardEvent::TriggerPrev: scroll_ = std::max(0, scroll_ - page); return true;
        case WizardEvent::Right:
        case WizardEvent::TriggerNext: scroll_ = std::min(last, scroll_ + page); return true;
        default: return false;
    }
}

void WizardScrollText::draw(Painter &p, const Theme &t, const Rect &area) {
    const int gutter = scrollbarGutter(t);
    const Rect col = wizardTextColumn(t, {area.x, area.y, std::max(0, area.w - gutter), area.h});
    lines_.clear();
    for (size_t i = 0; i < source_.size(); ++i) {
        const std::vector<std::string> wrapped =
            wrapText(p, t, source_[i].text.str(), TextRole::Body, source_[i].bold, col.w);
        for (const std::string &line : wrapped) lines_.push_back({line, source_[i].tier, source_[i].bold});
        if (i + 1 < source_.size()) lines_.push_back({std::string(), TextTier::Primary, false});   // the paragraph gap
    }
    const int step = paragraphStep(p, t);
    total_ = int(lines_.size());
    visible_ = std::max(1, area.h / step);
    scroll_ = std::max(0, std::min(scroll_, total_ - visible_));
    const int lineH = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    for (int i = 0; i < visible_ && scroll_ + i < total_; ++i) {
        const Line &l = lines_[size_t(scroll_ + i)];
        if (l.text.empty()) continue;
        drawLabel(p, t, {col.x, area.y + i * step, col.w, lineH}, l.text, TextRole::Body, l.tier, Align::Left, l.bold);
    }
    drawScrollbar(p, t, area, total_, visible_, scroll_);
}

// ---------------------------------------------------------------- centred page

namespace {
std::vector<std::string> wrapped(Painter &p, const Theme &t, const Text &text, TextRole role, bool bold, int width, int maxLines) {
    std::vector<std::string> lines = wrapText(p, t, text.str(), role, bold, width);
    if (maxLines > 0 && int(lines.size()) > maxLines) {
        lines.resize(size_t(maxLines));
        lines.back() = p.elide(bold ? FontRole::Bold : FontRole::Regular, t.size(role), lines.back() + " \xE2\x80\xA6", width);
    }
    return lines;
}
int stepOf(Painter &p, const Theme &t, TextRole role, bool bold) {
    return p.lineHeight(bold ? FontRole::Bold : FontRole::Regular, t.size(role)) + t.px(Space::lineExtra);
}
}  // namespace

int drawWizardCentered(Painter &p, const Theme &t, const Rect &body, const WizardCentered &spec) {
    const Rect col = readingColumn(t, body);
    const int headStep = stepOf(p, t, TextRole::Heading, true), lineStep = stepOf(p, t, TextRole::Body, false);
    const std::vector<std::string> head = wrapped(p, t, spec.headline, TextRole::Heading, true, col.w, 2);
    std::vector<std::vector<std::string>> lines;
    for (const Text &l : spec.lines) lines.push_back(wrapped(p, t, l, TextRole::Body, false, col.w, 0));
    const int icon = t.px(112), gap = t.px(28), lineGap = t.px(10);
    auto heightOf = [&](bool withIcon) {
        int h = int(head.size()) * headStep;
        if (withIcon && spec.icon != IconId::None) h += icon + gap;
        for (size_t i = 0; i < lines.size(); ++i) h += (i == 0 ? t.px(16) : lineGap) + int(lines[i].size()) * lineStep;
        return h;
    };
    const bool withIcon = spec.icon != IconId::None && heightOf(true) <= body.h;
    const int total = heightOf(withIcon);
    int y = body.y + std::max(0, (body.h - total) / 2);
    if (withIcon) {
        const Color c = spec.tone == Tone::Neutral || spec.tone == Tone::Action ? t.palette.accent : t.tone(spec.tone);
        drawIcon(p, spec.icon, {col.x + (col.w - icon) / 2, y, icon, icon}, c, std::max(3, t.px(4)));
        y += icon + gap;
    }
    const int headH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    for (const std::string &l : head) {
        drawLabel(p, t, {col.x, y, col.w, headH}, l, TextRole::Heading, TextTier::Primary, Align::Center, true);
        y += headStep;
    }
    const int lineH = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    for (size_t i = 0; i < lines.size(); ++i) {
        y += i == 0 ? t.px(16) : lineGap;
        for (const std::string &l : lines[i]) {
            if (y + lineH > body.bottom()) return total;
            drawLabel(p, t, {col.x, y, col.w, lineH}, l, TextRole::Body, i == 0 && spec.firstPrimary ? TextTier::Primary : TextTier::Muted,
                      Align::Center);
            y += lineStep;
        }
    }
    return total;
}

// ---------------------------------------------------------------- status slot

int wizardStatusHeight(Painter &p, const Theme &t) {
    return p.lineHeight(FontRole::Bold, t.size(TextRole::Body)) + t.px(8);
}

void drawWizardStatus(Painter &p, const Theme &t, const Rect &area, Tone tone, const Text &word, const Text &line) {
    const int h = p.lineHeight(FontRole::Bold, t.size(TextRole::Body));
    const int y = area.y + (area.h - h) / 2;
    int x = area.x;
    if (!word.empty()) {
        const int w = std::min(statusWidth(p, t, word.str(), TextRole::Body), area.w / 2);
        p.pushClip({area.x, area.y, w, area.h});
        drawStatus(p, t, x, y, tone, word.str(), TextRole::Body);
        p.popClip();
        x += w + t.px(16);
    }
    if (!line.empty() && x < area.right())
        drawLabel(p, t, {x, y, area.right() - x, h}, line.str(), TextRole::Body, TextTier::Muted);
}

// ---------------------------------------------------------------- choose list

bool ChooseList::twoLine() const {
    for (const ChooseItem &i : items_)
        if (!i.detail.empty()) return true;
    return false;
}

int ChooseList::contentHeight(const Theme &t) const {
    if (items_.empty()) return 0;
    const bool two = twoLine();
    return (int(items_.size()) - 1) * listItemPitch(t, two, Density::Dense) + listItemFrameHeight(t, two, Density::Dense);
}

void ChooseList::setItems(std::vector<ChooseItem> items) {
    items_ = std::move(items);
    cursor_ = items_.empty() ? 0 : std::max(0, std::min(cursor_, int(items_.size()) - 1));
}

void ChooseList::setCursor(int index) {
    cursor_ = items_.empty() ? 0 : std::max(0, std::min(index, int(items_.size()) - 1));
}

void ChooseList::setCurrent(int index) {
    for (size_t i = 0; i < items_.size(); ++i) items_[i].checked = int(i) == index;
    setCursor(index);
}

int ChooseList::checkedCount() const {
    int n = 0;
    for (const ChooseItem &i : items_) n += i.checked ? 1 : 0;
    return n;
}

std::vector<int> ChooseList::checked() const {
    std::vector<int> out;
    for (size_t i = 0; i < items_.size(); ++i)
        if (items_[i].checked) out.push_back(int(i));
    return out;
}

static int firstLetter(const ChooseItem &item) {
    const std::string s = item.title.str();
    return s.empty() ? 0 : std::toupper(static_cast<unsigned char>(s[0]));
}

// The row a letter jump from the cursor lands on: the first row of the next letter (direction 1), or the first row of the
// cursor's letter, else of the previous letter (direction -1). Stays put at the ends.
int ChooseList::letterJump(int direction) const {
    const int count = int(items_.size());
    if (direction > 0) {
        const int letter = firstLetter(items_[size_t(cursor_)]);
        for (int i = cursor_ + 1; i < count; ++i)
            if (firstLetter(items_[size_t(i)]) != letter) return i;
        return cursor_;
    }
    int i = cursor_;
    const int letter = firstLetter(items_[size_t(i)]);
    while (i > 0 && firstLetter(items_[size_t(i - 1)]) == letter) --i;
    if (i < cursor_) return i;
    if (i == 0) return 0;
    const int before = firstLetter(items_[size_t(i - 1)]);
    while (i > 0 && firstLetter(items_[size_t(i - 1)]) == before) --i;
    return i;
}

ChooseOutcome ChooseList::handle(WizardEvent event) {
    const int count = int(items_.size());
    if (count == 0) return ChooseOutcome::None;
    const int page = layout_.pageStep();
    int next = cursor_;
    switch (event) {
        case WizardEvent::Up:          next = ListLayout::move(cursor_, count, -1); break;
        case WizardEvent::Down:        next = ListLayout::move(cursor_, count, 1); break;
        case WizardEvent::TriggerPrev: next = jumpByLetter_ ? letterJump(-1) : ListLayout::move(cursor_, count, -page); break;
        case WizardEvent::TriggerNext: next = jumpByLetter_ ? letterJump(1) : ListLayout::move(cursor_, count, page); break;
        case WizardEvent::Confirm: break;
        default: return ChooseOutcome::None;
    }
    if (event != WizardEvent::Confirm) {
        const bool moved = next != cursor_;
        cursor_ = next;
        return moved ? ChooseOutcome::Moved : ChooseOutcome::None;
    }
    ChooseItem &row = items_[size_t(cursor_)];
    if (row.disabled) return ChooseOutcome::None;
    if (mode_ == ChooseMode::Single) return ChooseOutcome::Chosen;
    if (exclusiveFirst_ && !items_.empty()) {
        if (cursor_ == 0) {
            if (row.checked) return ChooseOutcome::None;       // the only way to clear it is to select another row
            for (ChooseItem &i : items_) i.checked = false;
            row.checked = true;
            return ChooseOutcome::Toggled;
        }
        row.checked = !row.checked;
        if (row.checked) items_[0].checked = false;
        else if (checkedCount() == 0) items_[0].checked = true;   // nothing left: back to the first row
        return ChooseOutcome::Toggled;
    }
    row.checked = !row.checked;
    return ChooseOutcome::Toggled;
}

void ChooseList::draw(Painter &p, const Theme &t, const Rect &area) {
    const bool two = twoLine();
    const int frame = listItemFrameHeight(t, two, Density::Dense), pitch = listItemPitch(t, two, Density::Dense);
    const Rect rows{area.x, area.y, std::max(0, area.w - scrollbarGutter(t)), area.h};
    layout_.configure(rows, frame, pitch);
    layout_.ensureVisible(cursor_);
    const int last = std::min(int(items_.size()), layout_.scroll() + layout_.visibleRows());
    for (int i = layout_.scroll(); i < last; ++i) {
        const ChooseItem &it = items_[size_t(i)];
        ListItemSpec spec;
        spec.kind = ListItemKind::Choice;
        spec.title = it.title;
        spec.detail = it.detail.empty() ? Text::data(std::string()) : it.detail;
        spec.status = it.status.empty() ? Text::data(std::string()) : it.status;
        spec.statusTone = it.statusTone;
        spec.checked = it.checked;
        spec.disabled = it.disabled;
        ListItemState st;
        st.cursor = i == cursor_;
        drawListItem(p, t, layout_.row(i), spec, st);
    }
    drawScrollbar(p, t, area, int(items_.size()), layout_.visibleRows(), layout_.scroll());
}

// ---------------------------------------------------------------- result

static const char *statusKey(Tone tone) {
    switch (tone) {
        case Tone::Done: return "result.status.done";
        case Tone::Warning: return "result.status.warning";
        case Tone::Error: return "result.status.error";
        default: return "result.status.info";
    }
}

int drawWizardResult(Painter &p, const Theme &t, const Rect &body, const WizardResult &r, int &firstItem) {
    const Rect col = wizardTextColumn(t, body);
    const int boldH = p.lineHeight(FontRole::Bold, t.size(TextRole::Body));
    const int smallH = p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
    const Tone tone = r.tone == Tone::Action ? Tone::Neutral : r.tone;
    int y = body.y;

    // 1. The status: icon and word, never colour alone.
    const std::string word = r.status.empty() ? Text::key(statusKey(tone)).str() : r.status.str();
    drawStatus(p, t, col.x, y, tone, word, TextRole::Body);
    y += boldH + t.px(12);

    // 2. The headline, in plain words (at most two lines).
    if (!r.headline.empty()) {
        const int used = drawTextBlock(p, t, {col.x, y, col.w, body.bottom() - y}, r.headline.str(), TextRole::Heading,
                                       TextTier::Primary, true, 2);
        y += used + t.px(12);
    }
    // 3. What changed.
    if (!r.changed.empty()) {
        const int used = drawTextBlock(p, t, {col.x, y, col.w, body.bottom() - y}, r.changed.str(), TextRole::Body, TextTier::Primary);
        y += used + t.px(12);
    }

    // What follows the items (next step, details) is measured first so the items never push it out of the frame.
    int tail = 0, nextH = 0, detailsH = 0;
    std::string detailLine;
    if (!r.next.empty()) {
        nextH = int(wrapText(p, t, r.next.str(), TextRole::Body, false, col.w).size()) * paragraphStep(p, t);
        tail += nextH + t.px(12);
    }
    if (!r.details.empty()) {
        detailLine = Text::key("modal.details").str() + ": " + r.details.str();
        const int lines = std::min<int>(2, int(wrapText(p, t, detailLine, TextRole::Small, false, col.w).size()));
        detailsH = lines * (smallH + t.px(2));
        tail += detailsH;
    }

    int shown = 0;
    if (!r.items.empty()) {
        const int gutter = scrollbarGutter(t);
        const Rect area{body.x, y, body.w, std::max(0, body.bottom() - tail - y)};
        const int count = int(r.items.size());
        firstItem = std::max(0, std::min(firstItem, count - 1));
        int yy = area.y, i = firstItem;
        for (; i < count; ++i) {
            const bool second = !r.items[size_t(i)].details.empty() &&
                                (r.items[size_t(i)].tone == Tone::Error || r.items[size_t(i)].tone == Tone::Warning);
            // ResultItem's height does not follow the text size: make room for both lines at Largest.
            const int h = std::max(resultItemHeight(t, r.items[size_t(i)]),
                                   p.lineHeight(FontRole::Regular, t.size(TextRole::Body)) +
                                       (second ? p.lineHeight(FontRole::Regular, t.size(TextRole::Small)) : 0) + t.px(10));
            if (yy + h > area.bottom()) break;
            drawResultItem(p, t, {area.x, yy, area.w - gutter, h}, r.items[size_t(i)]);
            yy += h + t.px(4);
        }
        shown = i - firstItem;
        if (shown == 0 && i < count) shown = 0;
        // The scrollbar counts items: a thumb for "where am I".
        drawScrollbar(p, t, area, count, std::max(1, shown), firstItem);
        y = std::min(body.bottom() - tail, yy + t.px(8));
        // Items that fit leave no gap to fill; the tail follows them.
    } else {
        firstItem = 0;
    }

    // 5. The next step, and 6. the raw text, small and muted.
    if (nextH > 0) {
        drawTextBlock(p, t, {col.x, y, col.w, nextH}, r.next.str(), TextRole::Body, TextTier::Muted);
        y += nextH + t.px(12);
    }
    if (detailsH > 0) drawTextBlock(p, t, {col.x, y, col.w, detailsH}, detailLine, TextRole::Small, TextTier::Muted, false, 2);
    return shown;
}

void ResultPage::handle(WizardEvent event, Wizard &w) {
    const int count = int(result_.items.size());
    if (event == WizardEvent::Up) { first_ = std::max(0, first_ - 1); return; }
    if (event == WizardEvent::Down) { if (first_ + shown_ < count) ++first_; return; }
    if (event != WizardEvent::Confirm) return;
    if (result_.retry != kNoPage) w.go(result_.retry);
    else w.close();
}

std::vector<Hint> ResultPage::hints() const {
    std::vector<Hint> h;
    if (shown_ < int(result_.items.size())) h.push_back(hint(HintKey::UpDown, "hint.scroll", false));
    if (result_.retry != kNoPage) {
        h.push_back(result_.primary.empty() ? hint(HintKey::A, "hint.tryAgain", true) : hint(HintKey::A, result_.primary, true));
        h.push_back(hint(HintKey::B, "hint.close", true));
    } else if (!result_.primary.empty()) {
        h.push_back(hint(HintKey::AB, result_.primary, true));
    } else {
        h.push_back(hint(HintKey::AB, "hint.done", true));
    }
    return h;
}

// ---------------------------------------------------------------- busy

std::string formatElapsed(int seconds) {
    if (seconds < 0) seconds = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d:%02d", seconds / 60, seconds % 60);
    return buf;
}

std::string BusyPage::elapsedText() const { return format("wizard.busy.elapsed", {formatElapsed(elapsedSeconds())}); }

void BusyPage::enter(Wizard &w) {
    elapsed_ = quiet_ = 0.0;
    sinceDetail_ = 1.0;
    hasPending_ = false;
    lastFraction_ = fraction;
    lastStatus_ = status.empty() ? std::string() : status.str();
    lastDetail_ = detail.empty() ? std::string() : detail.str();
    lastFacts_ = facts.empty() ? std::string() : facts.str();
    if (onEnter) onEnter(w);
}

void BusyPage::setDetail(const Text &line, const Text &factsLine) {
    const bool same = line.str() == detail.str() && factsLine.str() == facts.str();
    if (same) { hasPending_ = false; return; }
    if (sinceDetail_ >= 1.0 - 1e-6 || line.empty() || detail.empty()) {
        detail = line;
        facts = factsLine;
        sinceDetail_ = 0.0;
        hasPending_ = false;
    } else {
        pendingDetail_ = line;
        pendingFacts_ = factsLine;
        hasPending_ = true;
    }
}

bool BusyPage::update(float dt, Wizard &w) {
    const int secondsBefore = elapsedSeconds();
    bool changed = onUpdate ? onUpdate(dt, w) : false;
    if (!w.active() || w.current() != this) return true;
    sinceDetail_ += dt;
    if (hasPending_ && sinceDetail_ >= 1.0 - 1e-6) {
        detail = pendingDetail_;
        facts = pendingFacts_;
        hasPending_ = false;
        sinceDetail_ = 0.0;
    }
    elapsed_ += dt;
    const std::string nowStatus = status.empty() ? std::string() : status.str();
    const std::string nowDetail = detail.empty() ? std::string() : detail.str();
    const std::string nowFacts = facts.empty() ? std::string() : facts.str();
    const bool visibleChange = nowStatus != lastStatus_ || nowDetail != lastDetail_ || nowFacts != lastFacts_ || fraction != lastFraction_;
    const bool noteBefore = waitNoteVisible();
    if (visibleChange) quiet_ = 0.0; else quiet_ += dt;
    lastStatus_ = nowStatus;
    lastDetail_ = nowDetail;
    lastFacts_ = nowFacts;
    lastFraction_ = fraction;
    return changed || visibleChange || waitNoteVisible() != noteBefore || (showElapsed && elapsedSeconds() != secondsBefore);
}

void BusyPage::draw(Painter &p, const Theme &t, const Rect &body) {
    // Every line keeps its place whether or not it has text (nothing jumps when one appears): the phrase in a two-line slot
    // bottom-aligned to the bar, the detail line, the elapsed line, two lines for the wait note.
    const bool wide = centered;
    const Rect col = wide ? readingColumn(t, body) : wizardTextColumn(t, body);
    const Align align = wide ? Align::Center : Align::Left;
    const Text note = waitNoteVisible() ? (waitNote.empty() ? Text::key("wizard.longWait") : waitNote) : Text();
    std::string meta;
    if (showElapsed) meta = elapsedText();
    if (!facts.empty()) meta += (meta.empty() ? "" : "  \xC2\xB7  ") + facts.str();
    const int headStep = stepOf(p, t, TextRole::Heading, true), smallStep = stepOf(p, t, TextRole::Small, false);
    const int headH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    const int bodyH = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    const int smallH = p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
    const int bar = t.px(12), gap = t.px(32);
    const int total = 2 * headStep + gap + bar + t.px(20) + bodyH + t.px(12) + smallH + t.px(16) + 2 * smallStep;
    int y = body.y + (wide ? std::max(0, (body.h - total) / 2) : 0);
    const std::vector<std::string> head = wrapped(p, t, status.empty() ? Text::data(std::string()) : status, TextRole::Heading, true, col.w, 2);
    y += (2 - int(head.size())) * headStep;
    for (const std::string &l : head) {
        drawLabel(p, t, {col.x, y, col.w, headH}, l, TextRole::Heading, TextTier::Primary, align, true);
        y += headStep;
    }
    y += gap;
    drawProgressBar(p, t, {col.x, y, col.w, bar}, fraction < 0.0f ? 0.05f : std::min(1.0f, fraction), false);
    y += bar + t.px(20);
    // One line is reserved. A longer text (a translation, Largest text) wraps to a second line and pushes the lines below
    // down into the room the block keeps free at its end; it is never cut in the middle of the page.
    const int bodyStep = stepOf(p, t, TextRole::Body, false);
    int detailLines = 1;
    if (!detail.empty()) {
        const std::vector<std::string> lines = wrapped(p, t, detail, TextRole::Body, false, col.w, 2);
        detailLines = std::max<int>(1, int(lines.size()));
        for (size_t i = 0; i < lines.size(); ++i)
            drawLabel(p, t, {col.x, y + int(i) * bodyStep, col.w, bodyH}, lines[i], TextRole::Body, TextTier::Primary, align);
    }
    y += bodyH + (detailLines - 1) * bodyStep + t.px(12);
    if (!meta.empty()) drawLabel(p, t, {col.x, y, col.w, smallH}, meta, TextRole::Small, TextTier::Muted, align);
    y += smallH + t.px(16);
    if (!note.empty()) {
        const std::vector<std::string> lines = wrapped(p, t, note, TextRole::Small, false, col.w, 2);
        for (const std::string &l : lines) {
            drawLabel(p, t, {col.x, y, col.w, smallH}, l, TextRole::Small, TextTier::Muted, align);
            y += smallStep;
        }
    }
}

// ---------------------------------------------------------------- entry

void EntryPage::enter(Wizard &w) {
    dialog_ = TextEntryDialog(spec_);
    invalid_ = Text();
    if (onEnter) onEnter(w);
}

void EntryPage::draw(Painter &p, const Theme &t, const Rect &body) {
    const int fieldH = dialog_.fieldHeight(t);
    const int statusH = wizardStatusHeight(p, t);
    int y = body.y;
    dialog_.drawField(p, t, {body.x, y, body.w, fieldH});
    y += fieldH + t.px(8);
    if (!invalid_.empty()) drawWizardStatus(p, t, {body.x, y, body.w, statusH}, Tone::Warning, Text::key("wizard.entry.check"), invalid_);
    else drawWizardStatus(p, t, {body.x, y, body.w, statusH}, statusTone, statusWord, statusLine);
    y += statusH + t.px(8);
    dialog_.drawKeyboard(p, t, {body.x, y, body.w, dialog_.keyboardHeight(t)});
}

void EntryPage::handle(WizardEvent event, Wizard &w) {
    TextEntryEvent te;
    switch (event) {
        case WizardEvent::Up:      te = TextEntryEvent::Up; break;
        case WizardEvent::Down:    te = TextEntryEvent::Down; break;
        case WizardEvent::Left:    te = TextEntryEvent::Left; break;
        case WizardEvent::Right:   te = TextEntryEvent::Right; break;
        case WizardEvent::Confirm: te = TextEntryEvent::Press; break;
        case WizardEvent::X:       te = TextEntryEvent::Delete; break;
        case WizardEvent::Y:       te = TextEntryEvent::Space; break;
        case WizardEvent::Start:   te = TextEntryEvent::Done; break;
        case WizardEvent::Back:    te = TextEntryEvent::Cancel; break;
        default: return;
    }
    const TextEntryResult r = dialog_.handle(te);
    if (te == TextEntryEvent::Press || te == TextEntryEvent::Delete || te == TextEntryEvent::Space) invalid_ = Text();
    if (r == TextEntryResult::Cancelled) { w.cancelTo(backTo); return; }
    if (r != TextEntryResult::Submitted) return;
    if (validate) {
        const Text message = validate(dialog_.value());
        if (!message.empty()) { invalid_ = message; return; }
    }
    if (onSubmit) onSubmit(dialog_.value(), w);
}

}  // namespace ui
