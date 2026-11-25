// Connect to Wi-Fi: Classic's state machine (SettingsScreen::startWifiSetup / handleWifiFlow / cancelWifiFlow, src/legacy/
// SettingsScreen.cpp) shown as a Wizard (ui::Wizard through WizardFlowPage): one Medium frame for every page, each page one
// thing, large and centred where it is not a list or the keyboard. The steps and every rule are Classic's; only the
// presentation changed. The service seam is SystemService (wifiStartScan ... wifiRollback).
//
//   Scan -(done)-> Networks -A on a network-> [Password] -> Prompt -A-> Testing -(connected)-> Review -A-> [Country] -> Done
//                     |  \-A on "hidden"-> Ssid -A-> Password                 \-(failed)-> Failed -A-> Prompt (open) or Password
//                     \-A on "Scan again"-> Scan                                      Review -A, save fails-> SaveFailed -A-> Review
// Back (Classic's rules): Scan, Networks, Prompt, Failed: the whole flow ends. Ssid and Password: back to Networks. Country:
// back to the Review. Testing and Review: the whole flow ends and the previous connection is restored (leaveGuard).
// The previous connection is also restored whenever the flow ends for any other reason while a test is still running
// (the tab or the GUI closes), and a typed password is wiped when the flow ends.

#include "app/settings/system/WifiFlow.h"

#include <algorithm>

#include "app/settings/SystemService.h"
#include "app/settings/WizardFlowPage.h"

namespace patsgui {
namespace {

// The page ids of the flow.
enum { kScan = 0, kNetworks, kSsid, kPassword, kPrompt, kTesting, kReview, kCountry, kFailed, kSaveFailed, kScanFailed, kDone };

// One line under a list or a keyboard: the status icon + word (never colour alone) and the text.
struct Line {
    ui::Tone tone = ui::Tone::Neutral;
    ui::Text word, text;
    bool empty() const { return word.empty() && text.empty(); }
};

// The flow's data: Classic's SettingsScreen members wifiNetworks_, wifiSsid_, wifiPassword_, wifiMessage_, wifiOpen_,
// wifiHidden_ in one object that every page's lambdas capture.
struct Flow {
    SystemService *svc = nullptr;
    ui::Wizard *wz = nullptr;
    std::vector<WifiNetwork> networks;
    std::string ssid, password;
    bool open = false, hidden = false;
    std::string status;           // the test's last status line (Wifi::pollTest keeps it between its polls)
    std::string failure;          // the raw text of a failed test (Details)
    std::string saveError;        // the raw text of a failed save (Details)
    std::string scanFailure;      // the raw text of a scan that could not start (Details)
    bool scanBegun = false;       // the scan was started by the Networks page ("Scan again")
    Line scanLine;                // Classic's wifiMessage_ after the scan: no networks, or the scan's error
    Line list;                    // the status line of the Networks page (scanLine, or a failed "Scan again")
    Line note;                    // the status line of the entry page being opened
    ~Flow() { wipe(password); }
    static void wipe(std::string &s) {   // Classic: std::fill(password, '\0'); clear()
        volatile char *d = s.empty() ? nullptr : &s[0];
        for (size_t i = 0; i < s.size(); ++i) d[i] = '\0';
        s.clear();
    }
};
using FlowPtr = std::shared_ptr<Flow>;

ui::Text key(const char *k) { return ui::Text::key(k); }
ui::Text data(const std::string &s) { return ui::Text::data(s); }
ui::Text flowTitle() { return key("settings.system.wifi.title"); }

Line warn(const char *wordKey, const ui::Text &text) { return Line{ui::Tone::Warning, key(wordKey), text}; }

// Classic: cancelWifiFlow(true) = `if (wifi_.testing()) wifi_.rollback()` plus the wipe of the password. Every page runs it
// when it is left while the flow ends: B on Testing or Review, B anywhere, the tab or the GUI closing. A page that moves on
// (w.go) keeps the connection under test.
void endGuard(const FlowPtr &f) {
    if (f->wz->active()) return;
    if (f->svc->wifiTesting()) f->svc->wifiRollback();
    Flow::wipe(f->password);
}

template <class P>
void guard(P &page, const FlowPtr &f) {
    page.onLeave = [f](bool) { endGuard(f); };
}

// Classic: startWifiSetup(). False: the scan could not start (the error is in `error`).
bool beginScan(const FlowPtr &f, std::string &error) {
    if (!f->svc->wifiStartScan(error)) return false;
    f->networks.clear();
    return true;
}

// ---- Scan (Classic: Scanning, update(): scanComplete)
void addScanPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(flowTitle()));
    page->centered = true;
    page->status = key("settings.system.wifi.scan.status");
    page->waitNote = key("settings.system.wifi.scan.wait");
    page->waitNoteAfter = 8.0f;
    page->onEnter = [f](ui::Wizard &) {
        f->scanFailure.clear();
        if (f->scanBegun) f->scanBegun = false;          // "Scan again" started it already
        else {
            std::string error;
            if (!beginScan(f, error)) f->scanFailure = error;
        }
    };
    page->onUpdate = [f](float, ui::Wizard &w) {
        if (!f->scanFailure.empty()) { w.go(kScanFailed); return true; }
        if (!f->svc->wifiScanComplete()) return false;
        std::string error;
        f->networks = f->svc->wifiScanResults(error);
        if (!error.empty()) f->scanLine = warn("settings.system.wifi.scanFailed.word", data(error));
        else if (f->networks.empty()) f->scanLine = warn("settings.system.wifi.noNetworks.word", key("settings.system.wifi.noNetworks"));
        else f->scanLine = Line();
        w.go(kNetworks);
        return true;
    };
    guard(*page, f);   // B: Classic cancelWifiFlow(false), the default Busy cancel leaves the flow
    flow.add(kScan, std::move(page));
}

// ---- Networks (Classic: Networks; rows: the networks, "hidden", "Scan again")
struct NetworksData {
    ui::ChooseList list{ui::ChooseMode::Single};
    bool fresh = true;   // the scan just finished: the cursor starts at the top (Classic: wifiCursor_ = 0)
};

const char *signalWord(int dbm) {
    return dbm >= -60 ? "settings.system.wifi.signal.strong" : dbm >= -75 ? "settings.system.wifi.signal.fair" : "settings.system.wifi.signal.weak";
}

void addNetworksPage(WizardFlowPage &flow, const FlowPtr &f) {
    auto nd = std::make_shared<NetworksData>();
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, flowTitle()));
    page->onEnter = [f, nd](ui::Wizard &) {
        const int cursor = nd->fresh ? 0 : nd->list.cursor();   // coming back by B keeps the row, a new scan starts at the top
        nd->fresh = false;
        f->list = f->scanLine;   // Classic (owner D2): Back to the list resets the line
        std::vector<ui::ChooseItem> items;
        for (const WifiNetwork &n : f->networks) {
            ui::ChooseItem it;
            it.title = data(n.ssid);
            it.status = key(n.open ? "settings.system.wifi.net.open" : n.supported ? "settings.system.wifi.net.secured"
                                                                                   : "settings.system.wifi.net.unsupported");
            if (!n.supported) { it.statusTone = ui::Tone::Warning; it.disabled = true; }
            it.detail = data(ui::format("settings.system.wifi.net.signal", {ui::Catalog::get().tr(signalWord(n.signalDbm)),
                                                                            std::to_string(n.signalDbm)}));
            items.push_back(it);
        }
        ui::ChooseItem hidden, rescan;
        hidden.title = key("settings.system.wifi.hidden");
        hidden.detail = key("settings.system.wifi.hidden.line");
        rescan.title = key("settings.system.wifi.rescan");
        rescan.detail = key("settings.system.wifi.rescan.line");
        items.push_back(hidden);
        items.push_back(rescan);
        nd->list.setItems(items);
        nd->list.setCursor(cursor);
    };
    page->onDraw = [f, nd](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        const ui::Rect col = ui::wizardTextColumn(t, body);
        const int statusH = ui::wizardStatusHeight(p, t);
        const int headH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Heading));
        const std::string question = key("settings.system.wifi.networks.headline").str();
        const int lines = std::min<int>(2, int(ui::wrapText(p, t, question, ui::TextRole::Heading, true, col.w).size()));
        const int questionH = lines * (headH + t.px(ui::Space::lineExtra));
        ui::drawTextBlock(p, t, {col.x, body.y, col.w, questionH}, question, ui::TextRole::Heading, ui::TextTier::Primary, true, 2);
        const int top = body.y + questionH + t.px(16);
        nd->list.draw(p, t, {body.x, top, body.w, std::max(0, body.bottom() - statusH - t.px(8) - top)});
        // The line under the list: why the row under the cursor cannot be joined, else the scan's own note.
        Line line = f->list;
        const int at = nd->list.cursor();
        if (at >= 0 && at < int(f->networks.size()) && !f->networks[size_t(at)].supported)
            line = warn("settings.system.wifi.net.unsupported", key("settings.system.wifi.unsupported"));
        ui::drawWizardStatus(p, t, {body.x, body.bottom() - statusH, body.w, statusH}, line.tone, line.word, line.text);
    };
    page->onHints = [nd] {
        std::vector<ui::Hint> h{ui::hint(ui::HintKey::UpDown, "hint.move")};
        const int at = nd->list.cursor();
        if (at >= 0 && at < int(nd->list.items().size()) && !nd->list.items()[size_t(at)].disabled)
            h.push_back(ui::hint(ui::HintKey::A, "hint.choose", true));
        h.push_back(ui::hint(ui::HintKey::B, "hint.cancel", true));   // Classic: B cancels the whole flow
        return h;
    };
    page->onEvent = [f, nd](ui::WizardEvent e, ui::Wizard &w) {
        if (nd->list.handle(e) != ui::ChooseOutcome::Chosen) return;
        const int at = nd->list.cursor(), hiddenRow = int(f->networks.size());
        if (at == hiddenRow) {   // Connect to hidden Wi-Fi access point
            f->hidden = true;
            f->open = false;
            f->ssid.clear();
            f->note = Line{ui::Tone::Neutral, ui::Text(), key("settings.system.wifi.ssid.enter")};
            w.go(kSsid);
        } else if (at == hiddenRow + 1) {   // Scan again
            std::string error;
            if (beginScan(f, error)) { f->scanBegun = true; nd->fresh = true; w.go(kScan); }
            else f->list = warn("settings.system.wifi.scanFailed.word", data(error));   // Classic: a toast, the list stays
        } else if (at >= 0 && at < hiddenRow) {
            const WifiNetwork &n = f->networks[size_t(at)];
            f->ssid = n.ssid;
            f->open = n.open;
            f->hidden = false;
            if (f->open) w.go(kPrompt);
            else {
                f->note = Line{ui::Tone::Neutral, ui::Text(), data(ui::format("settings.system.wifi.password.for", {f->ssid}))};
                w.go(kPassword);
            }
        }
    };
    page->onBack = [] { return ui::kClose; };
    guard(*page, f);
    flow.add(kNetworks, std::move(page));
}

// ---- The entry pages: the on-screen keyboard in the frame, the status slot is Classic's wifiMessage_
// EntryPage wipes its typed text when left; this one also runs the end guard (the GUI closes on the country page while the test runs).
class WifiEntryPage : public ui::EntryPage {
public:
    WifiEntryPage(const FlowPtr &f, const ui::TextEntrySpec &spec) : ui::EntryPage(flowTitle(), spec), f_(f) {}
    void leave(bool cancelled) override { ui::EntryPage::leave(cancelled); endGuard(f_); }

private:
    FlowPtr f_;
};

std::unique_ptr<ui::EntryPage> entryPage(const FlowPtr &f, const char *placeholder, bool masked) {
    ui::TextEntrySpec spec;
    spec.title = flowTitle();
    spec.placeholder = key(placeholder);
    spec.masked = masked;
    spec.maxLength = 64;   // Classic: OnScreenKeyboard{64}
    spec.live = false;
    std::unique_ptr<ui::EntryPage> page(new WifiEntryPage(f, spec));
    ui::EntryPage *raw = page.get();
    page->onEnter = [f, raw](ui::Wizard &) {
        raw->statusTone = f->note.tone;
        raw->statusWord = f->note.word;
        raw->statusLine = f->note.text;
    };
    return page;
}

// ---- Ssid (Classic: SsidEntry; Back: Networks)
void addSsidPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::EntryPage> page = entryPage(f, "settings.system.wifi.ssid.placeholder", false);
    page->backTo = kNetworks;
    page->validate = [](const std::string &typed) {   // Classic: stays on the entry, the typed text is kept
        return typed.empty() || typed.size() > 32 ? key("settings.system.wifi.ssid.invalid") : ui::Text();
    };
    page->onSubmit = [f](const std::string &typed, ui::Wizard &w) {
        f->ssid = typed;
        f->note = Line{ui::Tone::Neutral, ui::Text(), data(ui::format("settings.system.wifi.password.for", {f->ssid}))};
        w.go(kPassword);
    };
    flow.add(kSsid, std::move(page));
}

// ---- Password (Classic: PasswordEntry; Back: Networks; Start submits)
void addPasswordPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::EntryPage> page = entryPage(f, "settings.system.wifi.password.placeholder", true);
    page->backTo = kNetworks;
    page->onSubmit = [f](const std::string &typed, ui::Wizard &w) {
        if (typed.size() < 8 || typed.size() > 63) {   // Classic: the typed text is wiped, the message says why, the keys stay masked
            f->note = warn("wizard.entry.check", key("settings.system.wifi.password.invalid"));
            w.cancelTo(kPassword);                      // the same page again: a new keyboard, no new page
            return;
        }
        f->password = typed;
        w.go(kPrompt);
    };
    flow.add(kPassword, std::move(page));
}

// ---- Prompt (Classic: TestPrompt; B: the whole flow ends)
void addPromptPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, flowTitle()));
    page->onDraw = [f](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Wifi;
        c.headline = key("settings.system.wifi.prompt.headline");
        c.lines = {data(ui::format("settings.system.wifi.prompt.network", {f->ssid})), key("settings.system.wifi.prompt.line1"),
                   key("settings.system.wifi.prompt.line2")};
        c.firstPrimary = true;
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true), ui::hint(ui::HintKey::B, "hint.cancel", true)};
    };
    page->onEvent = [f](ui::WizardEvent e, ui::Wizard &w) {
        if (e != ui::WizardEvent::Confirm) return;
        std::string error;   // Classic: wifi_.beginTest(...), the password is wiped either way
        const bool ok = f->svc->wifiBeginTest(f->ssid, f->password, f->open, f->hidden, error);
        Flow::wipe(f->password);
        if (!ok) { f->failure = error; w.go(kFailed); }
        else {
            f->status = ui::format("settings.system.wifi.testing.joining", {f->ssid});
            w.go(kTesting);
        }
    };
    page->onBack = [] { return ui::kClose; };
    guard(*page, f);
    flow.add(kPrompt, std::move(page));
}

// ---- Testing (Classic: Testing, update(): pollTest; B: restore and end)
void addTestingPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(flowTitle()));
    page->centered = true;
    page->detail = key("settings.system.wifi.testing.restore");
    page->waitNote = key("settings.system.wifi.testing.wait");
    ui::BusyPage *raw = page.get();
    page->onEnter = [f, raw](ui::Wizard &) { raw->status = data(f->status); };
    page->onUpdate = [f, raw](float dt, ui::Wizard &w) {
        const WifiTest t = f->svc->wifiPollTest(dt);
        // Between two polls (every 0.5 s) Classic's Wifi::pollTest leaves the status text as it was: an empty status
        // means "unchanged", not "blank". Taking it literally made the text blink every half second.
        if (!t.status.empty() && t.status != f->status) {
            f->status = t.status;
            raw->status = data(f->status);
        }
        if (t.result == WifiTest::Result::Connected) { w.go(kReview); return true; }
        if (t.result == WifiTest::Result::Failed) { f->failure = f->status; w.go(kFailed); return true; }
        return false;
    };
    guard(*page, f);   // B: leave(true), the flow ends, endGuard restores the previous connection
    flow.add(kTesting, std::move(page));
}

// ---- Review (Classic: TestReview; A: save (or ask the country code first), B: restore and end)
void addReviewPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, flowTitle()));
    page->tone_ = ui::Tone::Done;
    page->onDraw = [f](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Check;
        c.tone = ui::Tone::Done;
        c.headline = key("settings.system.wifi.review.headline");
        c.lines = {data(f->status), key("settings.system.wifi.review.question"), key("settings.system.wifi.review.cancel")};
        c.firstPrimary = true;
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.save", true), ui::hint(ui::HintKey::B, "hint.cancel", true)};
    };
    page->onEvent = [f](ui::WizardEvent e, ui::Wizard &w) {
        if (e != ui::WizardEvent::Confirm) return;
        if (f->svc->wifiNeedsCountryCode()) {
            f->note = Line{ui::Tone::Neutral, ui::Text(), key("settings.system.wifi.country.message")};
            w.go(kCountry);
            return;
        }
        std::string error;
        if (f->svc->wifiSaveTestedNetwork(error)) w.go(kDone);
        else { f->saveError = error; w.go(kSaveFailed); }
    };
    page->onBack = [] { return ui::kClose; };   // the guard restores the previous connection
    guard(*page, f);
    flow.add(kReview, std::move(page));
}

// ---- Country (Classic: CountryEntry; Back: the review)
void addCountryPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::EntryPage> page = entryPage(f, "settings.system.wifi.country.placeholder", false);
    page->backTo = kReview;
    page->onSubmit = [f](const std::string &typed, ui::Wizard &w) {
        std::string error;
        if (!f->svc->wifiSetCountryCode(typed, error)) {   // Classic: the text is cleared, the message says why
            f->note = warn("wizard.entry.check", data(error));
            w.cancelTo(kCountry);
            return;
        }
        if (f->svc->wifiSaveTestedNetwork(error)) w.go(kDone);
        else { f->saveError = error; w.go(kSaveFailed); }
    };
    flow.add(kCountry, std::move(page));
}

// ---- The Result pages: Failed (Classic TestFailed), SaveFailed (Classic: the review with the error), ScanFailed, Done
std::unique_ptr<ui::ResultPage> resultPage(const FlowPtr &f, std::function<void(ui::WizardResult &)> fill) {
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(flowTitle()));
    ui::ResultPage *raw = page.get();
    page->onEnter = [raw, fill](ui::Wizard &) {
        ui::WizardResult r;
        fill(r);
        raw->set(r);
    };
    page->onFinished = [f] { endGuard(f); };
    return page;
}

void addResultPages(WizardFlowPage &flow, const FlowPtr &f) {
    // A test that did not connect. The service already put the previous connection back (pollTest rolls back on a timeout,
    // beginTest on a failed start), so the page says so.
    flow.add(kFailed, resultPage(f, [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Error;
        r.headline = data(ui::format("settings.system.wifi.failed.headline", {f->ssid}));
        r.changed = key("settings.system.wifi.failed.changed");
        r.next = key(f->open ? "settings.system.wifi.failed.next.open" : "settings.system.wifi.failed.next");
        r.details = data(f->failure);
        r.retry = f->open ? kPrompt : kPassword;   // Classic TestFailed + A
    }));
    // The tested network is still active; saving it did not work. A tries again, B restores the previous connection.
    flow.add(kSaveFailed, resultPage(f, [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Error;
        r.headline = key("settings.system.wifi.saveFailed.headline");
        r.changed = key("settings.system.wifi.saveFailed.changed");
        r.next = key("settings.system.wifi.saveFailed.next");
        r.details = data(f->saveError);
        r.retry = kReview;
    }));
    // The scan could not start (no adapter, the supplicant refused): Classic stayed in Settings with a toast.
    flow.add(kScanFailed, resultPage(f, [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Error;
        r.headline = key("settings.system.wifi.scanFailed.headline");
        r.changed = key("settings.system.wifi.scanFailed.changed");
        r.next = key("settings.system.wifi.scanFailed.next");
        r.details = data(f->scanFailure);
        r.retry = kScan;
    }));
    // Saved (Classic: the toast "Wi-Fi saved. Restart MiSTer to connect automatically." and the flow ends).
    flow.add(kDone, resultPage(f, [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Done;
        r.headline = key("settings.system.wifi.saved.headline");
        r.changed = data(ui::format("settings.system.wifi.saved.changed", {f->ssid}));
        r.next = key("settings.system.wifi.saved.next");
    }));
}

std::unique_ptr<WizardFlowPage> makeFlow(const FlowPtr &f) {
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    f->wz = &flow->wizard();
    addScanPage(*flow, f);
    addNetworksPage(*flow, f);
    addSsidPage(*flow, f);
    addPasswordPage(*flow, f);
    addPromptPage(*flow, f);
    addTestingPage(*flow, f);
    addReviewPage(*flow, f);
    addCountryPage(*flow, f);
    addResultPages(*flow, f);
    return flow;
}

FlowPtr newFlow(SystemService &service) {
    FlowPtr f = std::make_shared<Flow>();
    f->svc = &service;
    return f;
}

}  // namespace

// Classic: startWifiSetup(). The scan starts when the first page opens; a scan that cannot start ends on a Result page.
bool openWifiFlow(SystemService &service, PageStack &pages) {
    FlowPtr f = newFlow(service);
    WizardFlowPage::launch(pages, makeFlow(f), kScan);
    return true;
}

bool openWifiStepForTest(SystemService &service, PageStack &pages, const std::string &step) {
    FlowPtr f = newFlow(service);
    std::string error;
    f->networks = service.wifiScanResults(error);
    f->ssid = "Example-Home";
    ui::PageId first = kNetworks;
    if (step == "networks") first = kNetworks;
    else if (step == "scanning") first = kScan;
    else if (step == "ssid") { f->hidden = true; f->note = Line{ui::Tone::Neutral, ui::Text(), key("settings.system.wifi.ssid.enter")}; first = kSsid; }
    else if (step == "password") {
        f->note = Line{ui::Tone::Neutral, ui::Text(), data(ui::format("settings.system.wifi.password.for", {f->ssid}))};
        first = kPassword;
    } else if (step == "prompt") first = kPrompt;
    else if (step == "testing" || step == "review" || step == "country") {
        service.wifiBeginTest(f->ssid, "correct horse", false, false, error);
        if (step == "testing") { f->status = ui::format("settings.system.wifi.testing.joining", {f->ssid}); first = kTesting; }
        else {
            f->status = "Connected to " + f->ssid + " \xE2\x80\x94 IP 192.168.1.40";
            f->note = Line{ui::Tone::Neutral, ui::Text(), key("settings.system.wifi.country.message")};
            first = step == "review" ? kReview : kCountry;
        }
    } else if (step == "failed") {
        f->failure = "Could not connect to " + f->ssid + ". Check the password and signal.";
        first = kFailed;
    } else if (step == "savefailed") {
        service.wifiBeginTest(f->ssid, "correct horse", false, false, error);
        f->status = "Connected to " + f->ssid + " \xE2\x80\x94 IP 192.168.1.40";
        f->saveError = "Cannot save Wi-Fi config: Permission denied";
        first = kSaveFailed;
    } else if (step == "scanfailed") {
        f->scanFailure = "No Wi-Fi adapter";
        first = kScanFailed;
    } else if (step == "done") first = kDone;
    else return false;
    WizardFlowPage::launch(pages, makeFlow(f), first);
    return true;
}

void registerWifiStrings(ui::Catalog &c) {
    // Scan
    c.set("settings.system.wifi.title", "Connect to Wi-Fi");
    c.set("settings.system.wifi.scan.status", "Looking for networks");
    c.set("settings.system.wifi.scanFailed.word", "Scan failed");
    c.set("settings.system.wifi.scanFailed.headline", "Could not look for networks");
    c.set("settings.system.wifi.scanFailed.changed", "Nothing was changed.");
    c.set("settings.system.wifi.scanFailed.next", "Check that the Wi-Fi adapter is plugged in, then try again.");
    // Networks
    c.set("settings.system.wifi.networks.headline", "Choose the Wi-Fi network to join");
    c.set("settings.system.wifi.noNetworks.word", "No networks");
    c.set("settings.system.wifi.noNetworks", "Scan again or enter the name yourself.");
    c.set("settings.system.wifi.net.open", "Open");
    c.set("settings.system.wifi.net.secured", "Secured");
    c.set("settings.system.wifi.net.unsupported", "Not supported");
    c.set("settings.system.wifi.unsupported", "This network's security type cannot be used here.");
    c.set("settings.system.wifi.net.signal", "{1} signal, {2} dBm");
    c.set("settings.system.wifi.signal.strong", "Strong");
    c.set("settings.system.wifi.signal.fair", "Fair");
    c.set("settings.system.wifi.signal.weak", "Weak");
    c.set("settings.system.wifi.hidden", "Connect to a hidden network");
    c.set("settings.system.wifi.hidden.line", "Type the network name yourself");
    c.set("settings.system.wifi.rescan", "Scan again");
    c.set("settings.system.wifi.rescan.line", "Look for networks once more");
    // Entry pages
    c.set("settings.system.wifi.ssid.placeholder", "Wi-Fi network name");
    c.set("settings.system.wifi.ssid.enter", "Enter the hidden network name");
    c.set("settings.system.wifi.ssid.invalid", "Enter a network name with 1 to 32 characters");
    c.set("settings.system.wifi.password.placeholder", "Enter password\xE2\x80\xA6");
    c.set("settings.system.wifi.password.for", "Enter the password for {1}");
    c.set("settings.system.wifi.password.invalid", "Password must be 8 to 63 characters");
    c.set("settings.system.wifi.country.placeholder", "Two-letter country code (for example, US)");
    c.set("settings.system.wifi.country.message",
          "MiSTer did not report your country. Enter its two-letter country code, for example US.");
    // Prompt, Testing, Review
    c.set("settings.system.wifi.prompt.headline", "Test the connection");
    c.set("settings.system.wifi.prompt.network", "Network: {1}");
    c.set("settings.system.wifi.prompt.line1", "The test temporarily switches the Wi-Fi connection.");
    c.set("settings.system.wifi.prompt.line2", "If it fails or you cancel, the previous connection will be restored.");
    c.set("settings.system.wifi.testing.joining", "Joining {1}\xE2\x80\xA6");
    c.set("settings.system.wifi.testing.restore", "Cancel restores the previous connection.");
    c.set("settings.system.wifi.review.headline", "The connection works");
    c.set("settings.system.wifi.review.question", "Save this network so MiSTer reconnects after restart?");
    c.set("settings.system.wifi.review.cancel", "Cancel restores the previous connection.");
    // Results
    c.set("settings.system.wifi.failed.headline", "Could not connect to {1}");
    c.set("settings.system.wifi.failed.changed", "Nothing was saved. Your previous connection is in use.");
    c.set("settings.system.wifi.failed.next", "Check the password and the signal, then try again.");
    c.set("settings.system.wifi.failed.next.open", "Check the signal, then try again.");
    c.set("settings.system.wifi.saveFailed.headline", "Could not save the network");
    c.set("settings.system.wifi.saveFailed.changed", "The tested connection is still active, but it is not saved.");
    c.set("settings.system.wifi.saveFailed.next", "Try again, or close to restore the previous connection.");
    c.set("settings.system.wifi.saved.headline", "Wi-Fi saved");
    c.set("settings.system.wifi.saved.changed", "MiSTer is connected to {1}.");
    c.set("settings.system.wifi.saved.next", "Restart MiSTer to connect automatically.");
}

}  // namespace patsgui
