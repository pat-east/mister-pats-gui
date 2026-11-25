#include "app/settings/StandardPages.h"

#include <algorithm>

#include "ui-framework/kit/List.h"
#include "ui-framework/kit/Primitives.h"
#include "ui-framework/kit/WizardParts.h"

namespace patsgui {
namespace {

// The modal's hints with the words of A and B replaced when the page names its own.
std::vector<ui::Hint> withWords(std::vector<ui::Hint> hints, const ui::Text &aWords, const ui::Text &bWords) {
    for (ui::Hint &h : hints) {
        if ((h.key == ui::HintKey::A || h.key == ui::HintKey::AB) && !aWords.empty()) {
            if (h.key == ui::HintKey::AB) h.key = ui::HintKey::A;
            h.action = aWords;
        } else if (h.key == ui::HintKey::B && !bWords.empty()) {
            h.action = bWords;
        }
    }
    return hints;
}

// A default ui::Text has neither a key nor data: str() would look up "" and draw the catalog's missing-key marker.
// Components that always draw a text get this instead of an empty one.
ui::Text safe(const ui::Text &t) { return t.empty() ? ui::Text::data(std::string()) : t; }

}  // namespace

void registerStandardPageStrings(ui::Catalog &c) {
    c.set("settings.page.checkEntry", "Check your entry");
}

ui::Rect drawSettingsPageFrame(ui::Painter &p, const ui::Theme &t, const ui::Rect &content, const ui::Text &title,
                               const ui::Text &info) {
    ui::PageHeaderSpec header;
    header.title = title;
    header.info = info;
    const ui::Rect body = ui::drawPageHeader(p, t, content, header);
    ui::drawSurface(p, t, body, ui::SurfaceRole::Faint, t.px(ui::Radius::panel));
    return body.inset(t.px(ui::Space::pad));
}

// ------------------------------------------------------------------------------------------------ ChoicePage

ChoicePage::ChoicePage(const ChoiceSpec &spec) : spec_(spec) {
    rebuild(std::max(0, std::min(spec.cursor >= 0 ? spec.cursor : spec.current, int(spec.items.size()) - 1)));
}

void ChoicePage::rebuild(int cursor) {
    std::vector<ui::ModalChoice> choices;
    for (size_t i = 0; i < spec_.items.size(); ++i) {
        ui::ModalChoice c;
        c.title = spec_.items[i].title;
        c.detail = safe(spec_.items[i].detail);
        c.checked = int(i) == spec_.current;
        choices.push_back(c);
    }
    modal_ = ui::ModalStack();
    modal_.push(ui::Modal::choose(spec_.title, choices));
    for (int i = 0; i < cursor; ++i) modal_.handle(ui::ModalEvent::Down);   // the cursor starts on the option in use
    cursor_ = cursor;
}

// A one-line status pill in the free band under (else above) a centered panel, for the pages whose framework
// dialog has no message slot (ChoiceSpec::message, TextInputSpec::message).
static void drawMessagePill(ui::Painter &p, const ui::Theme &t, const ui::Rect &panel, const ui::Rect &canvas,
                            const std::string &line) {
    if (line.empty()) return;
    const int lineH = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body));
    const int h = lineH + t.px(16), gap = t.px(10);
    const int maxW = std::max(t.px(160), canvas.w - 2 * t.px(ui::Space::margin));
    const int w = std::min(maxW, ui::labelWidth(p, t, line, ui::TextRole::Body) + t.px(32));
    int y = panel.bottom() + gap;
    if (y + h > canvas.bottom() - t.px(96)) y = std::max(canvas.y + t.px(8), panel.y - gap - h);   // not enough room below
    const ui::Rect pill{canvas.x + (canvas.w - w) / 2, y, w, h};
    ui::drawSurface(p, t, pill, ui::SurfaceRole::Raised, h / 2);
    ui::drawLabel(p, t, {pill.x + t.px(16), pill.y, pill.w - t.px(32), h}, line, ui::TextRole::Body, ui::TextTier::Primary,
                  ui::Align::Center);
}

void ChoicePage::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &) {
    modal_.draw(p, t, p.bounds());
    const ui::Text message = spec_.messageProvider ? spec_.messageProvider() : spec_.message;
    if (message.empty()) return;
    // The choose modal is Medium: 1120 x 760 design pixels, clamped to the canvas, centered (src/ui-framework/kit/Modal.cpp).
    const ui::Rect canvas = p.bounds();
    const int margin = t.px(ui::Space::margin);
    const int w = std::min(t.px(1120), canvas.w - 2 * margin), h = std::min(t.px(760), canvas.h - 2 * margin);
    const ui::Rect panel{canvas.x + (canvas.w - w) / 2, canvas.y + (canvas.h - h) / 2, w, h};
    drawMessagePill(p, t, panel, canvas, message.str());
}

bool ChoicePage::handle(Event event, PageStack &stack) {
    ui::ModalEvent me;
    if (!toModalEvent(event, me, true) || spec_.items.empty()) return true;   // nothing else acts while it is open
    const int at = modal_.topSelected();
    const ui::ModalResult r = modal_.handle(me);
    if (r == ui::ModalResult::Stay) { cursor_ = modal_.topSelected(); return true; }
    if (r == ui::ModalResult::Confirmed) {
        if (spec_.closeOnChoose) stack.pop(); else rebuild(at);   // the modal closed itself: put it back
        if (spec_.onChosen) spec_.onChosen(at, stack);
    } else {
        stack.pop();
        if (spec_.onCancel) spec_.onCancel(stack);
    }
    return true;
}

std::vector<ui::Hint> ChoicePage::hints() const { return withWords(modal_.hints(), spec_.chooseHint, spec_.cancelHint); }

// ------------------------------------------------------------------------------------------------ ConfirmPage

ConfirmPage::ConfirmPage(const ConfirmSpec &spec) : spec_(spec) {
    modal_.push(ui::Modal::confirm(spec.title, spec.body));
}

void ConfirmPage::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &) { modal_.draw(p, t, p.bounds()); }

bool ConfirmPage::handle(Event event, PageStack &stack) {
    ui::ModalEvent me;
    if (!toModalEvent(event, me, true)) return true;
    const ui::ModalResult r = modal_.handle(me);
    if (r == ui::ModalResult::Confirmed) {
        if (spec_.closeOnConfirm) stack.pop(); else { modal_ = ui::ModalStack(); modal_.push(ui::Modal::confirm(spec_.title, spec_.body)); }
        if (spec_.onConfirm) spec_.onConfirm(stack);
    } else if (r == ui::ModalResult::Cancelled) {
        stack.pop();
        if (spec_.onCancel) spec_.onCancel(stack);
    }
    return true;
}

std::vector<ui::Hint> ConfirmPage::hints() const { return withWords(modal_.hints(), spec_.confirmHint, spec_.cancelHint); }

// ------------------------------------------------------------------------------------------------ ResultPage

ResultPage::ResultPage(const ResultSpec &spec) : spec_(spec) {
    switch (spec.kind) {
        case ResultSpec::Info: modal_.push(ui::Modal::info(safe(spec.title), safe(spec.body))); break;
        case ResultSpec::Done: modal_.push(ui::Modal::result(ui::Tone::Done, safe(spec.title), spec.status, safe(spec.body))); break;
        case ResultSpec::Warning: modal_.push(ui::Modal::result(ui::Tone::Warning, safe(spec.title), spec.status, safe(spec.body))); break;
        case ResultSpec::Error:
            modal_.push(ui::Modal::error(safe(spec.title), safe(spec.body), safe(spec.whatToDo), spec.details, bool(spec.onRetry)));
            break;
    }
}

void ResultPage::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &) { modal_.draw(p, t, p.bounds()); }

bool ResultPage::handle(Event event, PageStack &stack) {
    ui::ModalEvent me;
    if (!toModalEvent(event, me, true)) return true;
    const ui::ModalResult r = modal_.handle(me);
    if (r == ui::ModalResult::Stay) return true;
    stack.pop();
    if (r == ui::ModalResult::Confirmed) { if (spec_.onRetry) spec_.onRetry(stack); }
    else if (spec_.onClose) spec_.onClose(stack);
    return true;
}

std::vector<ui::Hint> ResultPage::hints() const {
    if (spec_.closeHint.empty() || (spec_.kind == ResultSpec::Error && spec_.onRetry)) return modal_.hints();
    return withWords(modal_.hints(), spec_.closeHint, ui::Text());
}

// ------------------------------------------------------------------------------------------------ ProgressPage

ProgressPage::ProgressPage(const ProgressSpec &spec) : spec_(spec) {}

void ProgressPage::set(ui::Text &field, const ui::Text &v) {
    if (field.str() != v.str()) dirty_ = true;
    field = v;
}

void ProgressPage::setFraction(float v) {
    v = std::max(0.0f, std::min(1.0f, v));
    if (v != spec_.fraction) dirty_ = true;
    spec_.fraction = v;
}

bool ProgressPage::update(float deltaSeconds, PageStack &stack) {
    deltaSeconds_ = deltaSeconds;
    if (onUpdate) onUpdate(*this, stack);
    const bool redraw = dirty_;
    dirty_ = false;
    return redraw;
}

void ProgressPage::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) {
    const ui::Rect inner = drawSettingsPageFrame(p, t, content,
                                                 spec_.header.empty() ? ui::Text::key("tab.settings") : spec_.header);
    ui::ProgressPanelSpec panel;
    panel.title = safe(spec_.title);
    panel.status = safe(spec_.status);
    panel.detail = spec_.detail;
    panel.fraction = spec_.fraction;
    panel.fill = true;   // from the top of the page, in the reading column
    ui::drawProgressPanel(p, t, ui::readingColumn(t, inner), panel);
}

bool ProgressPage::handle(Event event, PageStack &stack) {
    if (event == Event::Back && spec_.cancellable) {
        stack.pop();
        if (spec_.onCancel) spec_.onCancel(stack);
    }
    return true;   // locked: nothing else acts, and a page that cannot be cancelled swallows Back
}

std::vector<ui::Hint> ProgressPage::hints() const {
    std::vector<ui::Hint> h;
    if (spec_.cancellable)
        h.push_back(ui::hint(ui::HintKey::B, spec_.cancelHint.empty() ? ui::Text::key("hint.cancel") : spec_.cancelHint, true));
    return h;
}

// ------------------------------------------------------------------------------------------------ TextInputPage

TextInputPage::TextInputPage(const TextInputSpec &spec) : spec_(spec), dialog_(spec.entry) {}

void TextInputPage::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &) {
    dialog_.draw(p, t, p.bounds());
    if (!spec_.message.empty()) drawMessagePill(p, t, dialog_.panelRect(t, p.bounds()), p.bounds(), spec_.message.str());
    notice_.draw(p, t, p.bounds());
}

bool TextInputPage::handle(Event event, PageStack &stack) {
    if (!notice_.empty()) {
        ui::ModalEvent me;
        if (toModalEvent(event, me, true)) notice_.handle(me);
        return true;
    }
    ui::TextEntryEvent te;
    if (!mapTextEntryEvent(event, te)) return true;   // the keyboard locks everything behind it
    const ui::TextEntryResult r = dialog_.handle(te);
    if (r == ui::TextEntryResult::Cancelled) {
        stack.pop();
        if (spec_.onCancel) spec_.onCancel(stack);
    } else if (r == ui::TextEntryResult::Submitted) {
        std::string value = dialog_.value();
        if (spec_.validate) {
            const ui::Text message = spec_.validate(value);
            if (!message.empty()) {
                notice_.push(ui::Modal::info(spec_.invalidTitle.empty() ? ui::Text::key("settings.page.checkEntry")
                                                                        : spec_.invalidTitle, message));
                return true;
            }
        }
        if (spec_.closeOnSubmit) stack.pop();
        if (spec_.onSubmit) spec_.onSubmit(value, stack);
        if (spec_.entry.masked) {   // a typed secret does not outlive the call (Classic overwrites the password buffers)
            volatile char *d = value.empty() ? nullptr : &value[0];
            for (size_t i = 0; i < value.size(); ++i) d[i] = '\0';
        }
    }
    return true;
}

std::vector<ui::Hint> TextInputPage::hints() const { return notice_.empty() ? dialog_.hints() : notice_.hints(); }

void TextInputPage::onClose() {
    dialog_ = ui::TextEntryDialog();
    if (spec_.onLeave) spec_.onLeave();
}   // the typed text does not outlive the page

// ------------------------------------------------------------------------------------------------ InfoPage

std::unique_ptr<InfoPage> InfoPage::facts(const InfoSpec &spec, Provider provider, float refreshSeconds) {
    std::unique_ptr<InfoPage> page(new InfoPage(spec, Kind::Facts));
    page->provider_ = std::move(provider);
    page->refreshSeconds_ = refreshSeconds;
    if (page->provider_) page->entries_ = page->provider_();
    page->entriesRead_ = true;
    return page;
}

std::unique_ptr<InfoPage> InfoPage::viewer(const InfoSpec &spec) {
    std::unique_ptr<InfoPage> page(new InfoPage(spec, Kind::Viewer));
    page->view_ = View::Empty;
    return page;
}

void InfoPage::setText(const std::string &text) {
    logical_.clear();
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        std::string raw = text.substr(start, end == std::string::npos ? end : end - start);
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        for (size_t at = raw.find("**"); at != std::string::npos; at = raw.find("**", at)) raw.erase(at, 2);
        raw.erase(std::remove(raw.begin(), raw.end(), '`'), raw.end());
        Line line;
        size_t marker = 0;
        while (marker < raw.size() && raw[marker] == '#' && marker < 6) ++marker;
        if (marker > 0 && marker < raw.size() && raw[marker] == ' ') {
            line.heading = true;
            raw.erase(0, marker + 1);
        }
        line.blank = raw.empty();
        line.text = raw;
        logical_.push_back(line);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    view_ = logical_.empty() ? View::Empty : View::Text;
    wrappedWidth_ = -1;
    scroll_ = 0;
}

void InfoPage::setLoading(const ui::Text &line) { view_ = View::Loading; message_ = line; detail_ = ui::Text(); }
void InfoPage::setError(const ui::Text &message, bool retry) { view_ = View::Error; message_ = message; detail_ = ui::Text(); retry_ = retry; }
void InfoPage::setError(const ui::Text &headline, const ui::Text &detail, bool retry) {
    view_ = View::Error;
    message_ = headline;
    detail_ = detail;
    retry_ = retry;
}
void InfoPage::setEmpty(const ui::Text &line) { view_ = View::Empty; message_ = line; detail_ = ui::Text(); }

bool InfoPage::update(float deltaSeconds, PageStack &) {
    if (kind_ != Kind::Facts || !provider_ || refreshSeconds_ <= 0.0f) return false;
    sinceRefresh_ += deltaSeconds;
    if (sinceRefresh_ < refreshSeconds_) return false;
    sinceRefresh_ = 0.0f;
    entries_ = provider_();
    return true;
}

void InfoPage::layoutText(ui::Painter &p, const ui::Theme &t, int width) {
    const int scale = t.size(ui::TextRole::Body);
    if (width == wrappedWidth_ && scale == wrappedScale_) return;
    wrappedWidth_ = width;
    wrappedScale_ = scale;
    wrapped_.clear();
    for (const Line &line : logical_) {
        if (line.blank) { wrapped_.push_back(line); continue; }
        for (const std::string &piece : ui::wrapText(p, t, line.text, ui::TextRole::Body, line.heading, width)) {
            Line l;
            l.text = piece;
            l.heading = line.heading;
            wrapped_.push_back(l);
        }
    }
}

void InfoPage::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) {
    const ui::Rect inner = drawSettingsPageFrame(p, t, content, spec_.title, spec_.subtitle);
    if (kind_ == Kind::Facts) {
        // The list, then the note directly under it (at the bottom when the list fills the page and scrolls).
        const int small = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Small));
        std::vector<std::string> lines;
        int footerH = 0;
        if (!spec_.footer.empty() || !spec_.footerAccent.empty()) {
            if (!spec_.footer.empty())
                lines = ui::wrapText(p, t, spec_.footer.str(), ui::TextRole::Small, false, std::min(inner.w, ui::readingWidth(t)));
            footerH = int(lines.size()) * small;
            if (!spec_.footerAccent.empty()) footerH += small + (lines.empty() ? 0 : t.px(6));
        }
        const int gap = footerH > 0 ? t.px(16) : 0;
        facts_.minLabelPercent = 28;   // the same value column on every facts page of the area (Info, Disc drive)
        const int used = ui::drawKeyValueList(p, t, {inner.x, inner.y, inner.w, std::max(0, inner.h - footerH - gap)}, entries_, facts_);
        int y = std::min(inner.y + used + gap, inner.bottom() - footerH);
        for (const std::string &line : lines) {
            ui::drawLabel(p, t, {inner.x, y, inner.w, small}, line, ui::TextRole::Small, ui::TextTier::Muted);
            y += small;
        }
        if (!spec_.footerAccent.empty()) {   // the project address: plain text, nothing here opens it
            ui::drawLabel(p, t, {inner.x, y + (lines.empty() ? 0 : t.px(6)), inner.w, small}, spec_.footerAccent.str(),
                          ui::TextRole::Small, ui::TextTier::Primary);
        }
        return;
    }
    const int step = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body)) + t.px(ui::Space::lineExtra);
    // Loading, the error and "nothing here" sit centred in the panel like the wizard's Busy and Result pages.
    if (view_ == View::Loading) {
        ui::ProgressPanelSpec panel;
        panel.showTitle = false;
        panel.status = safe(message_);
        panel.fraction = -1.0f;
        const int h = std::min(inner.h, t.px(120));
        ui::drawProgressPanel(p, t, {inner.x, inner.y + (inner.h - h) / 2, inner.w, h}, panel);
        return;
    }
    if (view_ == View::Error || view_ == View::Empty) {
        ui::WizardCentered c;
        c.icon = view_ == View::Error ? ui::IconId::Alert : ui::IconId::Search;
        c.tone = view_ == View::Error ? ui::Tone::Warning : ui::Tone::Neutral;
        c.headline = safe(message_);
        if (!detail_.empty()) c.lines = {detail_};
        ui::drawWizardCentered(p, t, inner, c);
        return;
    }
    // The text is a reading column centred on the page (the same padding left and right, text left-aligned); its scroll
    // bar sits right next to it, the pair is centred.
    const int bar = ui::scrollbarGutter(t);
    const int textW = std::min(std::max(0, inner.w - bar), ui::readingWidth(t));
    const ui::Rect column{inner.x + (inner.w - (textW + bar)) / 2, inner.y, textW, inner.h};
    layoutText(p, t, column.w);
    visibleLines_ = std::max(1, column.h / step);
    const int total = int(wrapped_.size());
    scroll_ = std::max(0, std::min(scroll_, total - visibleLines_));
    for (int i = scroll_; i < total && i < scroll_ + visibleLines_; ++i) {
        const Line &line = wrapped_[size_t(i)];
        if (line.blank) continue;
        ui::drawLabel(p, t, {column.x, column.y + (i - scroll_) * step, column.w, step}, line.text, ui::TextRole::Body,
                      ui::TextTier::Primary, ui::Align::Left, line.heading);
    }
    ui::drawScrollbar(p, t, {column.x, column.y, column.w + bar, column.h}, total, visibleLines_, scroll_);
}

bool InfoPage::handle(Event event, PageStack &stack) {
    if (kind_ == Kind::Facts) {
        if (event == Event::Up) { facts_.first = std::max(0, facts_.first - 1); return true; }
        if (event == Event::Down) { ++facts_.first; return true; }   // the draw clamps it
    }
    if (kind_ == Kind::Viewer && view_ == View::Text) {
        if (event == Event::Up) { scroll_ = std::max(0, scroll_ - 1); return true; }
        if (event == Event::Down) {
            const int maxScroll = std::max(0, int(wrapped_.size()) - visibleLines_);
            scroll_ = std::min(maxScroll, scroll_ + 1);
            return true;
        }
    }
    const bool confirm = event == Event::Confirm || event == Event::Start;
    if (confirm && kind_ == Kind::Viewer) {
        if (view_ == View::Error && retry_) {
            if (onRetry) onRetry(stack);
        } else if (view_ == View::Text && !spec_.actionHint.empty()) {
            if (spec_.closeOnAction) stack.pop();
            if (spec_.onAction) spec_.onAction(stack);
        }
        return true;   // A does nothing in a plain viewer
    }
    if (event == Event::Back || confirm) {   // facts: A and B close, as Classic's Info panel does
        stack.pop();
        if (spec_.onClose) spec_.onClose(stack);
    }
    return true;
}

std::vector<ui::Hint> InfoPage::hints() const {
    std::vector<ui::Hint> h;
    const ui::Text closeWords = spec_.closeHint.empty() ? ui::Text::key("hint.close") : spec_.closeHint;
    if (kind_ == Kind::Facts) {
        if (facts_.scrolls) h.push_back(ui::hint(ui::HintKey::UpDown, "hint.scroll"));
        h.push_back(ui::hint(ui::HintKey::AB, closeWords, true));
        return h;
    }
    if (view_ == View::Error && retry_) h.push_back(ui::hint(ui::HintKey::A, ui::Text::key("hint.tryAgain"), true));
    else if (view_ == View::Text && !spec_.actionHint.empty()) h.push_back(ui::hint(ui::HintKey::A, spec_.actionHint, true));
    if (view_ == View::Text) h.push_back(ui::hint(ui::HintKey::UpDown, spec_.scrollHint.empty() ? ui::Text::key("hint.scroll") : spec_.scrollHint, false));
    h.push_back(ui::hint(ui::HintKey::B, closeWords, true));
    return h;
}

}  // namespace patsgui
