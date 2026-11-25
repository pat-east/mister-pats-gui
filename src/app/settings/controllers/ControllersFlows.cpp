// The two sequences of the Controllers category as wizard flows (ui::Wizard through WizardFlowPage), one Medium frame each,
// one thing per page. Classic's state machines stay exactly (src/legacy/ControllersScreen.cpp: beginWizard / captureWizardStep /
// updateWizard / saveWizardResult, and enterBluetooth / handleBluetoothAction / updateBluetooth); only the presentation changed.
// The rules themselves are PadLogic's MappingWizard (steps, capture, the Start hold and skip, the timers).
//
// Button mapping:   Open -A/Start-> Capture (19 steps, raw input) -(done)-> Save -> Done
//                                        \-(hold Start 5 s, pad lost)-> Done (nothing saved)     Done "Try again" after a failed save -> Save
//   Open: B closes (nothing was saved, no message). Capture reads the pad itself (the host stops translating it into Actions
//   while the page is current: WizardFlowPage::onRawInput -> ControllersService::setRawInputWanted); its way out is Classic's:
//   hold Start 5 s (from the second step on: Start is what the first step captures).
//
// Bluetooth:        Intro -A-> Scan (Busy, timeout bar) -(new pad)-> Found -A-> button mapping flow (hand-off)
//                      \-(btpair did not start)-> Failed -A-> Intro        \-(timeout)-> Timeout -A-> Intro        (no adapter: None)
//   Every way out of the flow stops btpair, refreshes the list and puts its cursor on the first row (leaveBluetooth()), except
//   the hand-off, which puts it on the paired pad as Classic does.

#include <algorithm>
#include <cmath>
#include <memory>

#include "app/settings/WizardFlowPage.h"
#include "app/settings/controllers/ControllersPages.h"
#include "app/settings/controllers/PadLogic.h"
#include "app/settings/controllers/PadDiagram.h"

namespace patsgui {
namespace controllers {

namespace {

ui::Text key(const char *k) { return ui::Text::key(k); }
std::string whole(float v) { return std::to_string(long(std::lround(v))); }

bool listed(ControllersService &service, int deviceId) {
    for (const ControllerInfo &d : service.listDevices())
        if (d.deviceId == deviceId) return true;
    return false;
}

// ================================================================================================ button mapping

enum { kOpen = 0, kCapture = 1, kSave = 2, kDone = 3 };

struct MapData {
    ControllersService *svc = nullptr;
    ControllerInfo dev;
    MappingWizard wiz;
    enum class End { Saved, SaveFailed, Cancelled, Lost } end = End::Cancelled;
    size_t answered = 0, total = 0;
    unsigned signature = 0;
};
using MapPtr = std::shared_ptr<MapData>;

void addOpenPage(WizardFlowPage &flow, const MapPtr &d) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, key("settings.controllers.map.title")));
    page->onEnter = [d](ui::Wizard &) {   // beginWizard(): the device's file is the base (read-modify-write); nothing captured yet
        Slots existing{};
        d->svc->readMapping(d->dev.deviceId, existing);
        d->wiz.begin(existing, d->svc->pad(d->dev.deviceId));
    };
    page->onDraw = [d](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.headline = ui::Text::data(ui::format("settings.controllers.map.open.headline",
                                               {d->dev.name.empty() ? ui::Catalog::get().tr("settings.controllers.wizard.fallbackName") : d->dev.name}));
        c.lines = {key("settings.controllers.map.open.p1"), key("settings.controllers.map.open.p2")};
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] { return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true)}; };
    page->onEvent = [d](ui::WizardEvent e, ui::Wizard &w) {
        if (e != ui::WizardEvent::Confirm && e != ui::WizardEvent::Start) return;   // Classic: A and Start both begin
        d->wiz.confirmOpening(d->svc->pad(d->dev.deviceId));   // the press that confirmed must not answer step 0
        w.go(kCapture);
    };
    flow.add(kOpen, std::move(page));
}

// The 19 captures are one Capture page: the page polls the pad itself (raw input), the MappingWizard keeps Classic's rules.
void addCapturePage(WizardFlowPage &flow, const MapPtr &d) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Capture, key("settings.controllers.map.title")));
    page->onRawInput = [] { return true; };
    page->onProgress = [d] {
        return ui::format("settings.controllers.wizard.progress", {std::to_string(std::min(d->wiz.index() + 1, d->wiz.count())),
                                                                  std::to_string(d->wiz.count())});
    };
    page->onEnter = [d](ui::Wizard &) { d->signature = ~0u; };
    page->onUpdate = [d](float dt, ui::Wizard &w) {
        const bool present = listed(*d->svc, d->dev.deviceId);
        const PadSnapshot pad = d->svc->pad(d->dev.deviceId);
        const MappingWizard::Outcome o = d->wiz.update(dt, present, pad);
        if (o != MappingWizard::Outcome::Running) {
            d->answered = d->wiz.answered();
            d->total = d->wiz.count();
            if (o == MappingWizard::Outcome::Finished) { w.go(kSave); return true; }
            d->end = present && pad.connected ? MapData::End::Cancelled : MapData::End::Lost;   // the hold, or the pad went away
            w.go(kDone);
            return true;
        }
        const unsigned sig = unsigned(d->wiz.index()) * 1000003u + unsigned(d->wiz.stepIdle()) * 31u + (d->wiz.startHeld() ? 7u : 0u) +
                             unsigned(d->wiz.startHold() * 20.0f) * 101u;
        const bool changed = sig != d->signature;
        d->signature = sig;
        return changed;
    };
    page->onDraw = [d](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        if (d->wiz.index() >= d->wiz.count()) return;   // one frame before the save
        const WizardStep &step = d->wiz.step();
        const std::string label = ui::Catalog::get().tr(step.labelKey);
        const ui::Text prompt = ui::Text::data(step.isAxis ? label : ui::format("settings.controllers.wizard.press", {label}));
        const int headH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Heading));
        const int bodyH = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body));
        const int colW = std::min(body.w, t.px(480));
        const int colX = body.x + (body.w - colW) / 2;
        int y = body.y;
        // One fixed block from the top: the prompt, a status line, the timer bar and the hold row. Nothing moves between steps.
        // One line: the heading size when it fits the body, else the body size (long names, pseudo-language, Largest text).
        const ui::TextRole promptRole = ui::labelWidth(p, t, prompt.str(), ui::TextRole::Heading) <= body.w ? ui::TextRole::Heading : ui::TextRole::Body;
        ui::drawLabel(p, t, {body.x, y, body.w, headH}, prompt.str(), promptRole, ui::TextTier::Primary, ui::Align::Center, true);
        y += headH + t.px(6);
        const bool first = d->wiz.index() == 0;
        const std::string status = first
            ? ui::Catalog::get().tr("settings.controllers.wizard.first")
            : ui::format("settings.controllers.wizard.waiting", {whole(std::max(0.0f, MappingWizard::kStepIdleSeconds - d->wiz.stepIdle()))});
        ui::drawLabel(p, t, {body.x, y, body.w, bodyH}, status, ui::TextRole::Body, ui::TextTier::Muted, ui::Align::Center);
        y += bodyH + t.px(8);
        if (!first) {
            const float remaining = std::max(0.0f, MappingWizard::kStepIdleSeconds - d->wiz.stepIdle());
            ui::drawProgressBar(p, t, {colX, y, colW, t.px(10)}, remaining / MappingWizard::kStepIdleSeconds, false);
        }
        y += t.px(10) + t.px(10);
        const int holdH = std::max(t.px(40), bodyH + t.px(10));
        if (d->wiz.startHeld())
            ui::drawHoldProgress(p, t, {colX, y, colW, holdH}, ui::hint(ui::HintKey::Start, "settings.controllers.hint.cancelSetup", true, true),
                                 std::min(1.0f, d->wiz.startHold() / MappingWizard::kAbortHoldSeconds));
        else if (!first)
            ui::drawLabel(p, t, {body.x, y, body.w, holdH}, ui::Catalog::get().tr("settings.controllers.wizard.leave"), ui::TextRole::Small,
                          ui::TextTier::Muted, ui::Align::Center);
        y += holdH + t.px(8);
        // The part to press, lit; not for the menu steps (they are not tied to one spot on the pad).
        if (step.highlight != PadHighlight::None) {
            const ui::Rect diagram{body.x, y, body.w, body.bottom() - y};
            if (diagram.h > t.px(200)) drawWizardDiagram(p, t, diagram, step.highlight);
        }
    };
    // Classic: "Press Start to skip this button   Hold Start 5s to cancel setup". The first step has neither (Start is what
    // it captures); its page says so and what the way out is afterwards.
    page->onHints = [d] {
        if (d->wiz.index() == 0) return std::vector<ui::Hint>();
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::Start, "settings.controllers.hint.skip", true),
                                     ui::hint(ui::HintKey::Start, "settings.controllers.hint.cancelSetup", true, true)};
    };
    flow.add(kCapture, std::move(page));
}

// saveWizardResult(): a moment of work on its own page, so "Try again" after a failed write only repeats the write.
void addSavePage(WizardFlowPage &flow, const MapPtr &d) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(key("settings.controllers.map.title")));
    page->centered = true;
    page->cancelable = false;
    page->status = key("settings.controllers.map.saving");
    page->showElapsed = false;   // done within one frame
    page->onUpdate = [d](float, ui::Wizard &w) {
        d->end = d->svc->writeMapping(d->dev.deviceId, d->wiz.slots()) ? MapData::End::Saved : MapData::End::SaveFailed;
        w.go(kDone);
        return true;
    };
    flow.add(kSave, std::move(page));
}

void addDonePage(WizardFlowPage &flow, const MapPtr &d) {
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(key("settings.controllers.map.title")));
    ui::ResultPage *raw = page.get();
    page->onEnter = [d, raw](ui::Wizard &) {
        ui::WizardResult r;
        const std::string name = d->dev.name;
        switch (d->end) {
            case MapData::End::Saved: {
                const size_t skipped = d->total - std::min(d->total, d->answered);
                r.tone = ui::Tone::Done;
                r.headline = key("settings.controllers.map.saved");
                r.changed = ui::Text::data(skipped == 0 ? ui::format("settings.controllers.map.saved.all", {std::to_string(d->answered)})
                                                        : ui::format("settings.controllers.map.saved.some", {std::to_string(d->answered), std::to_string(skipped)}));
                r.next = key("settings.controllers.map.saved.next");
                break;
            }
            case MapData::End::SaveFailed:
                r.tone = ui::Tone::Error;
                r.headline = key("settings.controllers.saveFailed");
                r.changed = key("settings.controllers.map.failed.changed");
                r.next = key("settings.controllers.map.failed.next");
                r.details = key("settings.controllers.mapping.failed");
                r.retry = kSave;
                break;
            case MapData::End::Cancelled:
                r.tone = ui::Tone::Warning;
                r.status = key("settings.controllers.map.cancelled.word");
                r.headline = key("settings.controllers.map.cancelled");
                r.changed = key("settings.controllers.map.nothing");
                r.next = key("settings.controllers.map.cancelled.next");
                break;
            case MapData::End::Lost:
                r.tone = ui::Tone::Warning;
                r.status = key("settings.controllers.map.cancelled.word");
                r.headline = key("settings.controllers.map.lost");
                r.changed = key("settings.controllers.map.nothing");
                r.next = key("settings.controllers.map.lost.next");
                break;
        }
        raw->set(r);
    };
    flow.add(kDone, std::move(page));
}

std::unique_ptr<WizardFlowPage> buildMappingFlow(ControllersService &svc, const ControllerInfo &device) {
    auto d = std::make_shared<MapData>();
    d->svc = &svc;
    d->dev = device;
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    ControllersService *s = &svc;
    flow->onRawInput = [s](bool wanted) { s->setRawInputWanted(wanted); };
    addOpenPage(*flow, d);
    addCapturePage(*flow, d);
    addSavePage(*flow, d);
    addDonePage(*flow, d);
    return flow;
}

// ================================================================================================ Bluetooth

enum { kIntro = 0, kScan = 1, kFound = 2, kTimeout = 3, kFailed = 4, kNone = 5 };

struct BtData {
    ControllersService *svc = nullptr;
    std::shared_ptr<ListFlow> list;
    std::vector<int> before;
    float timer = 0.0f;
    std::string lastLine, foundName, foundIdstr;
    bool foundHasMapping = false;
    bool handedOff = false;
};
using BtPtr = std::shared_ptr<BtData>;

// leaveBluetooth(): stop btpair, a fresh list, the cursor on its first row. Runs when the flow ends (not for a hand-off).
void leaveBluetooth(const BtPtr &b, const ui::Wizard *w) {
    if (w->active() || b->handedOff) return;
    b->svc->cancelBluetoothScan();
    b->svc->refreshControllers();
    b->list->cursor = 0;
}

void addIntroPage(WizardFlowPage &flow, const BtPtr &b) {
    ui::Wizard *wz = &flow.wizard();
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, key("settings.controllers.bt.title")));
    page->onDraw = [](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Search;
        c.headline = key("settings.controllers.bt.intro.headline");
        c.lines = {key("settings.controllers.bt.intro.p1"), key("settings.controllers.bt.intro.p2")};
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] { return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true)}; };
    page->onEvent = [b](ui::WizardEvent e, ui::Wizard &w) {
        if (e != ui::WizardEvent::Confirm && e != ui::WizardEvent::Start) return;   // Classic: A and Start
        b->before.clear();
        for (const ControllerInfo &dev : b->svc->listDevices()) b->before.push_back(dev.deviceId);
        if (b->svc->startBluetoothScan()) w.go(kScan);
        else w.go(kFailed);   // showError("Could not start", "btpair could not be started.")
    };
    page->onLeave = [b, wz](bool) { leaveBluetooth(b, wz); };
    flow.add(kIntro, std::move(page));
}

std::string clock(float seconds) {   // "1:45"
    const int s = std::max(0, int(std::ceil(seconds)));
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d:%02d", s / 60, s % 60);
    return buf;
}

void addScanPage(WizardFlowPage &flow, const BtPtr &b) {
    ui::Wizard *wz = &flow.wizard();
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(key("settings.controllers.bt.title")));
    page->centered = true;
    page->showElapsed = false;   // the detail line counts the time left down, which is the more useful clock
    ui::BusyPage *raw = page.get();
    auto fill = [b, raw] {
        const float total = std::max(1.0f, b->svc->bluetoothTimeoutSeconds());
        raw->status = key("settings.controllers.bt.scanning");
        raw->fraction = std::min(1.0f, b->timer / total);
        // The detail changes once a second at most: the whole seconds left.
        raw->detail = ui::Text::data(ui::format("settings.controllers.bt.left", {clock(total - b->timer)}));
    };
    page->onEnter = [b, raw, fill](ui::Wizard &) {
        b->timer = 0.0f;
        b->lastLine.clear();
        raw->cancelable = true;
        raw->detail = ui::Text();
        fill();
    };
    page->onUpdate = [b, fill, raw](float dt, ui::Wizard &w) {
        const int before = int(std::ceil(std::max(1.0f, b->svc->bluetoothTimeoutSeconds()) - b->timer));
        b->timer += dt;
        std::string line;
        while (b->svc->nextBluetoothLine(line)) b->lastLine = line;   // btpair's own log: Details of a timeout only

        // The device-list difference, not btpair's log text, decides "a new controller connected".
        for (const ControllerInfo &dev : b->svc->listDevices()) {
            if (std::find(b->before.begin(), b->before.end(), dev.deviceId) != b->before.end()) continue;
            b->svc->cancelBluetoothScan();
            b->foundName = dev.name;
            b->foundIdstr = dev.idstr;
            Slots existing{};
            b->foundHasMapping = b->svc->readMapping(dev.deviceId, existing);
            w.go(kFound);
            return true;
        }
        if (b->timer >= b->svc->bluetoothTimeoutSeconds()) {   // a placeholder in Classic, unmeasured
            b->svc->cancelBluetoothScan();
            w.go(kTimeout);
            return true;
        }
        fill();
        (void)raw;
        return int(std::ceil(std::max(1.0f, b->svc->bluetoothTimeoutSeconds()) - b->timer)) != before;
    };
    page->onLeave = [b, wz](bool) { leaveBluetooth(b, wz); };   // B: leaveBluetooth() (btpair stopped by the service's cancel)
    flow.add(kScan, std::move(page));
}

void addFoundPage(WizardFlowPage &flow, const BtPtr &b) {
    ui::Wizard *wz = &flow.wizard();
    WizardFlowPage *self = &flow;
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, key("settings.controllers.bt.title")));
    page->onDraw = [b](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Check;
        c.tone = ui::Tone::Done;
        c.headline = ui::Text::data(ui::format("settings.controllers.bt.found", {b->foundName}));
        c.lines = {key(b->foundHasMapping ? "settings.controllers.bt.foundMapped" : "settings.controllers.bt.foundUnmapped")};
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.continue", true), ui::hint(ui::HintKey::B, "hint.later", true)};
    };
    page->onBack = [] { return ui::kClose; };
    page->onEvent = [b, self](ui::WizardEvent e, ui::Wizard &) {
        if (e != ui::WizardEvent::Confirm && e != ui::WizardEvent::Start) return;
        b->svc->refreshControllers();
        const std::vector<ControllerInfo> &rows = b->svc->controllers();
        int match = -1;
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i].idstr == b->foundIdstr) match = int(i);   // the last row with that id, as Classic
        if (match >= 0) {
            b->list->cursor = match;
            b->handedOff = true;
            std::unique_ptr<WizardFlowPage> next = buildMappingFlow(*b->svc, rows[size_t(match)]);
            next->wizard().open(kOpen);
            self->handOff(std::move(next));
        } else {
            self->wizard().close();   // Classic has no row to map; leave to the list instead of a dead wizard
        }
    };
    page->onLeave = [b, wz](bool) { leaveBluetooth(b, wz); };
    flow.add(kFound, std::move(page));
}

template <class P>
void closeGuard(P &page, const BtPtr &b, ui::Wizard *wz) {
    page->onFinished = [b, wz] { leaveBluetooth(b, wz); };
}

void addResultPages(WizardFlowPage &flow, const BtPtr &b) {
    ui::Wizard *wz = &flow.wizard();
    {   // Timeout: "Did not find a new device - try again?" as a result with its way back to the start
        std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(key("settings.controllers.bt.title")));
        ui::ResultPage *raw = page.get();
        page->onEnter = [b, raw](ui::Wizard &) {
            ui::WizardResult r;
            r.tone = ui::Tone::Warning;
            r.status = key("settings.controllers.bt.timeout.word");
            r.headline = key("settings.controllers.bt.timeout.headline");
            r.changed = key("settings.controllers.bt.timeout.changed");
            r.next = key("settings.controllers.bt.timeout.next");
            if (!b->lastLine.empty()) r.details = ui::Text::data(b->lastLine);
            r.retry = kIntro;
            raw->set(r);
        };
        closeGuard(page, b, wz);
        flow.add(kTimeout, std::move(page));
    }
    {   // btpair could not be started
        std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(key("settings.controllers.bt.title")));
        ui::ResultPage *raw = page.get();
        page->onEnter = [raw](ui::Wizard &) {
            ui::WizardResult r;
            r.tone = ui::Tone::Error;
            r.headline = key("settings.controllers.bt.startFailed");
            r.changed = key("settings.controllers.bt.startFailed.changed");
            r.next = key("settings.controllers.bt.startFailed.next");
            r.details = key("settings.controllers.bt.startFailed.body");
            r.retry = kIntro;
            raw->set(r);
        };
        closeGuard(page, b, wz);
        flow.add(kFailed, std::move(page));
    }
    {   // no adapter: information, A and B close
        std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(key("settings.controllers.bt.title")));
        ui::ResultPage *raw = page.get();
        page->onEnter = [raw](ui::Wizard &) {
            ui::WizardResult r;
            r.tone = ui::Tone::Neutral;
            r.headline = key("settings.controllers.bt.none.headline");
            r.changed = key("settings.controllers.bt.none");
            raw->set(r);
        };
        closeGuard(page, b, wz);
        flow.add(kNone, std::move(page));
    }
}

}  // namespace

void openMappingFlow(ControllersService &service, std::shared_ptr<ListFlow>, const ControllerInfo &device, PageStack &pages) {
    WizardFlowPage::launch(pages, buildMappingFlow(service, device), kOpen);
}

void openBluetoothFlow(ControllersService &service, std::shared_ptr<ListFlow> list, PageStack &pages) {
    auto b = std::make_shared<BtData>();
    b->svc = &service;
    b->list = std::move(list);
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    addIntroPage(*flow, b);
    addScanPage(*flow, b);
    addFoundPage(*flow, b);
    addResultPages(*flow, b);
    // enterBluetooth(): NoBluetooth when there is no adapter, else Idle.
    WizardFlowPage::launch(pages, std::move(flow), service.bluetoothAvailable() ? kIntro : kNone);
}

void registerFlowStrings(ui::Catalog &c) {
    // Button mapping
    c.set("settings.controllers.map.title", "Button mapping");
    c.set("settings.controllers.map.open.headline", "Set up the buttons of {1}");
    c.set("settings.controllers.map.open.p1", "You will press the buttons of your controller one after another.");
    c.set("settings.controllers.map.open.p2",
          "Start with the button you want to use as {START}. Later, hold it for 5 seconds to cancel. Nothing is saved then.");
    c.set("settings.controllers.wizard.press", "Press the button for {1}");
    c.set("settings.controllers.wizard.first", "This is also how you cancel later: hold it for 5 seconds.");
    c.set("settings.controllers.wizard.waiting", "No button detected \xE2\x80\x94 skipping in {1}s");
    c.set("settings.controllers.wizard.leave", "Hold {START} for 5 seconds to cancel.");
    c.set("settings.controllers.wizard.progress", "{1} / {2}");
    c.set("settings.controllers.hint.skip", "Skip");
    c.set("settings.controllers.hint.cancelSetup", "to cancel");   // "Hold Start to cancel"
    c.set("settings.controllers.map.saving", "Saving the button mapping");
    c.set("settings.controllers.map.saved", "Button mapping saved");
    c.set("settings.controllers.map.saved.all", "All {1} buttons are set.");
    c.set("settings.controllers.map.saved.some", "{1} buttons are set, {2} were skipped.");
    c.set("settings.controllers.map.saved.next", "You can change it again in Settings \xE2\x86\x92 Controllers.");
    c.set("settings.controllers.map.cancelled.word", "Cancelled");
    c.set("settings.controllers.map.cancelled", "Setup cancelled");
    c.set("settings.controllers.map.nothing", "Nothing was saved.");
    c.set("settings.controllers.map.cancelled.next", "You can set up the buttons again whenever you like.");
    c.set("settings.controllers.map.lost", "The controller was disconnected");
    c.set("settings.controllers.map.lost.next", "Connect it again, then set up the buttons again.");
    c.set("settings.controllers.map.failed.changed", "The button mapping was not saved.");
    c.set("settings.controllers.map.failed.next", "Try saving again. If it keeps failing, check the SD card.");
    // Bluetooth
    c.set("settings.controllers.bt.title", "Connect new controller");
    c.set("settings.controllers.bt.intro.headline", "Find a Bluetooth controller");
    c.set("settings.controllers.bt.intro.p1", "Put your controller into pairing mode.");
    c.set("settings.controllers.bt.intro.p2", "Then start the search. It stops after two minutes.");
    c.set("settings.controllers.bt.scanning", "Looking for controllers");
    c.set("settings.controllers.bt.left", "{1} left");
    c.set("settings.controllers.bt.found", "Found {1}");
    c.set("settings.controllers.bt.foundMapped", "It already has a button mapping. Set it up again now?");
    c.set("settings.controllers.bt.foundUnmapped", "It has no button mapping yet. Set it up now?");
    c.set("settings.controllers.bt.timeout.word", "Timeout");
    c.set("settings.controllers.bt.timeout.headline", "No new controller found");
    c.set("settings.controllers.bt.timeout.changed", "Nothing was connected.");
    c.set("settings.controllers.bt.timeout.next", "Check that the controller is in pairing mode, then try again.");
    c.set("settings.controllers.bt.startFailed", "The search could not start");
    c.set("settings.controllers.bt.startFailed.changed", "Nothing was connected.");
    c.set("settings.controllers.bt.startFailed.next", "Try again in a moment.");
    c.set("settings.controllers.bt.startFailed.body", "btpair could not be started.");
    c.set("settings.controllers.bt.none.headline", "No Bluetooth");
    c.set("settings.controllers.bt.none", "This MiSTer has no Bluetooth adapter.");
}

}  // namespace controllers
}  // namespace patsgui
