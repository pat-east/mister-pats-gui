#include "app/tests/Rig.h"

#include <cstdio>
#include <cstdlib>

#include "app/PatsGui.h"
#include "app/tests/Test.h"
#include "ui-framework/kit/WizardParts.h"

namespace tt {

const char *keyName(ui::HintKey key) {
    switch (key) {
        case ui::HintKey::Dpad: return "DPAD";
        case ui::HintKey::UpDown: return "UD";
        case ui::HintKey::LeftRight: return "LR";
        case ui::HintKey::Shoulders: return "SH";
        case ui::HintKey::Triggers: return "TR";
        case ui::HintKey::X: return "X";
        case ui::HintKey::Y: return "Y";
        case ui::HintKey::A: return "A";
        case ui::HintKey::AB: return "AB";
        case ui::HintKey::Start: return "Start";
        case ui::HintKey::B: return "B";
    }
    return "?";
}

bool eventNamed(const std::string &w, Event &e) {
    using E = Event;
    static const struct { const char *name; E event; } table[] = {
        {"up", E::Up}, {"down", E::Down}, {"left", E::Left}, {"right", E::Right}, {"a", E::Confirm}, {"b", E::Back},
        {"x", E::X}, {"y", E::Y}, {"start", E::Start}, {"xhold", E::XHold}, {"bhold", E::BHold}, {"lprev", E::LetterPrev},
        {"lnext", E::LetterNext}, {"select", E::Select}, {"tabprev", E::TabPrev}, {"tabnext", E::TabNext}};
    for (const auto &row : table)
        if (w == row.name) { e = row.event; return true; }
    return false;
}

namespace {
// The failure text carries the last part of the key sequence only.
std::string tail(const std::string &s) { return s.size() <= 260 ? s : "..." + s.substr(s.size() - 257); }
void registerOnce() {
    static bool done = false;
    if (done) return;
    done = true;
    ui::registerFrameworkStrings();
    patsgui::registerStrings();
}
}  // namespace

Rig::Rig()
    : services{library}, tab((registerOnce(), services.settings = &ss.services(), services)), canvas_(1920, 1080), theme_(1920, 1080) {
    regular_.load({});   // the built-in face: the tests need no font file
    bold_.load({});
    painter_.reset(new ui::SoftPainter(canvas_, regular_, bold_));
    tab.onShow();
    tab.update(0.0f);
}

void Rig::set(const std::string &key, const std::string &value) {
    const bool ok = ss.set(key, value);
    CHECK_MSG(ok, "unknown sample state " + key + "=" + value);
    trace_ += " [" + key + "=" + value + "]";
    context() = tail(trace_);
}

void Rig::open(const std::string &category, const std::string &rowId) {
    base_ = stack().size();
    const bool ok = tab.activateRowForTest(category, rowId);
    CHECK_MSG(ok, "no category " + category);
    trace_ += " <open " + rowId + ">";
    context() = tail(trace_);
    frame();
}

bool Rig::openPage(const std::string &categoryAndPage) {
    base_ = stack().size();
    const size_t slash = categoryAndPage.find('/');
    const bool ok = slash != std::string::npos && tab.openPageForTest(categoryAndPage.substr(0, slash), categoryAndPage.substr(slash + 1));
    trace_ += " <page " + categoryAndPage + ">";
    context() = tail(trace_);
    frame();
    return ok;
}

// The tests of the Welcome / scan / covers path assume the assets are in place (nothing to download); the assets step has its own tests,
// which call openFirstRunAt() with their own sample states.
void Rig::openFirstRun() {
    ss.system.iconsMissing = 0;
    ss.accessibility.installed = true;
    CHECK_MSG(openFirstRunAt("welcome"), "the first-start guide did not open");
}

bool Rig::openFirstRunAt(const std::string &pageId) {
    state_.reset(new patsgui::State);
    patsgui::attach(*state_, services);
    assets_ = patsgui::makeAssets(theme_, nullptr);
    firstRun_ = true;
    base_ = 0;
    const bool ok = state_->firstRun.startAt(services, pageId);
    trace_ += " <first-run " + pageId + ">";
    context() = tail(trace_);
    patsgui::update(*state_, 0.0f);
    return ok;
}

void Rig::press(Event e) {
    if (watch) watch(*this, true);
    if (firstRun_) {
        patsgui::handle(*state_, e);
        patsgui::update(*state_, 0.05f);
    } else {
        tab.handle(e);
        tab.update(0.05f);
    }
    if (watch) watch(*this, false);
}

void Rig::keys(const std::string &list) {
    size_t at = 0;
    while (at < list.size()) {
        const size_t comma = list.find(',', at);
        const std::string word = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
        at = comma == std::string::npos ? list.size() : comma + 1;
        if (word.empty()) continue;
        Event e;
        if (!eventNamed(word, e)) {
            // "wait2.5" lets 2.5 seconds pass inside a key list
            if (word.compare(0, 4, "wait") == 0) { wait(float(std::atof(word.c_str() + 4))); continue; }
            CHECK_MSG(false, "unknown key word " + word);
            continue;
        }
        trace_ += " " + word;
        context() = tail(trace_);
        press(e);
    }
}

void Rig::wait(float seconds) {
    trace_ += " ~" + std::to_string(seconds).substr(0, 4) + "s";
    context() = tail(trace_);
    for (float t = 0.0f; t < seconds - 1e-4f; t += 0.1f) {
        if (watch) watch(*this, true);
        if (firstRun_) patsgui::update(*state_, 0.1f);
        else tab.update(0.1f);
        if (watch) watch(*this, false);
    }
}

void Rig::note(const std::string &text) {
    trace_ += " {" + text + "}";
    context() = tail(trace_);
}

WizardFlowPage *Rig::flow() { return dynamic_cast<WizardFlowPage *>(stack().top()); }

int Rig::id() {
    WizardFlowPage *f = flow();
    return f ? f->wizard().pageId() : -9;
}

ui::WizardPage *Rig::wpage() {
    WizardFlowPage *f = flow();
    return f ? f->wizard().current() : nullptr;
}

ui::WizardPage::Kind Rig::kind() {
    ui::WizardPage *p = wpage();
    return p ? p->kind() : ui::WizardPage::Kind::Step;
}

std::string Rig::title() {
    ui::WizardPage *p = wpage();
    return p ? p->title().str() : std::string();
}

std::vector<HintView> Rig::hints() {
    std::vector<HintView> out;
    for (const ui::Hint &h : stack().hints()) out.push_back({h.key, h.action.str(), h.hold});
    return out;
}

std::string Rig::hintLine() {
    std::string s;
    for (const HintView &h : hints()) s += (s.empty() ? "" : " | ") + std::string(h.hold ? "Hold " : "") + keyName(h.key) + " " + h.word;
    return s;
}

bool Rig::hasHint(ui::HintKey key) {
    for (const HintView &h : hints())
        if (h.key == key) return true;
    return false;
}

std::string Rig::hintWord(ui::HintKey key) {
    for (const HintView &h : hints())
        if (h.key == key) return h.word;
    return std::string();
}

ui::WizardResult *Rig::result() {
    ui::ResultPage *p = dynamic_cast<ui::ResultPage *>(wpage());
    return p ? &p->result() : nullptr;
}

bool Rig::cancellableBusy() {
    ui::WizardPage *p = wpage();
    return p && p->kind() == ui::WizardPage::Kind::Busy && p->cancellable();
}

bool Rig::rawInputWanted() {
    WizardFlowPage *f = flow();
    return f && f->wantsRawInput();
}

int Rig::calls(const std::string &prefix) {
    int n = 0;
    for (const std::string &c : ss.system.calls)
        if (c.compare(0, prefix.size(), prefix) == 0) ++n;
    return n;
}

std::string Rig::callsText() {
    std::string s;
    for (const std::string &c : ss.system.calls) s += (s.empty() ? "" : " | ") + c;
    return s;
}

void Rig::draw() {
    if (firstRun_) {   // the whole shell with the guide over it
        patsgui::SystemValues values;
        values.clock = "10:42"; values.date = "Tue 22 Nov"; values.version = "0.9.1";
        const ui::ShellSpec shell = patsgui::shellFor(*state_, values, assets_);
        const ui::Rect content = ui::drawShell(*painter_, theme_, shell);
        patsgui::drawTab(*painter_, theme_, *state_, content);
        patsgui::drawOverlays(*painter_, theme_, *state_, shell);
        return;
    }
    tab.draw(*painter_, theme_, ui::Rect{0, 0, 1920, 1080});
}

}  // namespace tt
