// Core and MRA updates: the port of Classic's six dialogs (SettingsScreen::handleCoreUpdater / renderCoreUpdater,
// src/legacy/SettingsScreen.cpp:1209-1650), shown as a Wizard (ui::Wizard through WizardFlowPage): every dialog is a page
// of one Large frame. The work is CoreMraUpdater behind LibraryService; these pages only show its snapshot and send the
// same calls Classic sends. The state machine is Classic's, unchanged:
//
//   Intro -A (online)-> Checking -(ready)-> List -Start-> Review -A-> Installing -(ended)-> Result
//                                  \-(failed)-> Failed (check failed) -A-> Intro
//   List: Y checks again (fresh) -> Checking.  B closes the whole flow from Intro, Checking (stops the check), List.
//   Review: B back to the list.  Installing: no cancel, no back.
//
// Presentation: Intro and Checking and Installing are centred pages; the List is a multiple-selection ChooseList with the
// filter tabs above it (Left/Right) and the selection summary below; the Review is a scrolling text; Result and Failed
// use the one Result layout.

#include <algorithm>
#include <ctime>
#include <memory>

#include "app/settings/library/LibraryPages.h"
#include "app/settings/library/Values.h"
#include "app/settings/WizardFlowPage.h"
#include "ui-framework/kit/TabRow.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace library {
namespace {

using Status = CoreMraFile::Status;
using Type = CoreMraFile::Type;

// The ids of the pages of the flow.
enum { kIntro = 0, kChecking = 1, kList = 2, kReview = 3, kInstalling = 4, kResult = 5, kFailed = 6 };

// The list holds many two-line rows, the filter tabs and the summary line: the largest page decides (Wizard.md: Core updater, Large).
constexpr ui::WizardFrame kCoreFrame = ui::WizardFrame::Large;

const char *stateWord(Status s) {    // the list rows (SS:1537-1540)
    return s == Status::UpToDate ? "settings.library.cores.state.uptodate" : s == Status::Missing ? "settings.library.cores.state.missing"
         : s == Status::UpdateAvailable ? "settings.library.cores.state.update" : s == Status::Modified ? "settings.library.cores.state.changed"
         : "settings.library.cores.state.error";
}
const char *kindWord(Type t) {       // what the file is (Classic groups the rows under these headings, SS:1509-1512)
    return t == Type::ArcadeCore ? "settings.library.cores.kind.arcade" : t == Type::ConsoleCore ? "settings.library.cores.kind.console"
         : t == Type::OtherCore ? "settings.library.cores.kind.other" : "settings.library.cores.kind.mra";
}

bool anySelected(const CoreMraProgress &c) {
    return std::any_of(c.entries.begin(), c.entries.end(), [](const CoreMraFile &f) { return f.selected; });
}

// The flow's data: one struct captured by the pages' lambdas.
struct Data {
    // Intro (SS:1355-1366, update SS:1014-1021)
    bool online = false;
    float introTimer = 2.0f;
    // Checking
    float sinceDetail = 0.0f;
    // List (SS:1410-1570)
    int filter = 0;                 // 0 needs update, 1 all, 2 up to date, 3 changed
    bool freshList = true;          // the list was just produced by a check: filter and cursor start over
    std::vector<size_t> visible;    // indices into the snapshot of the rows the filter shows
    ui::ChooseList list{ui::ChooseMode::Multiple};
    size_t selectedCount = 0, selectedCores = 0, selectedMras = 0;
    uint64_t selectedBytes = 0;
    // Review (SS:1571-1624)
    ui::WizardScrollText review;
};
using DataPtr = std::shared_ptr<Data>;

// ---- Intro: what this does; offline it says what is missing (the page checks again by itself, SS:1014-1021)
void addIntroPage(WizardFlowPage &flow, LibraryService *svc, DataPtr d) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("settings.library.cores.title")));
    page->onEnter = [svc, d](ui::Wizard &) {   // openCoreUpdater()
        d->online = svc->internetRouteAvailable();
        d->introTimer = 2.0f;
    };
    page->onUpdate = [svc, d](float dt, ui::Wizard &) {
        // Picks up a connection that comes up while the dialog is open, e.g. after Wi-Fi setup.
        d->introTimer -= dt;
        if (d->introTimer > 0.0f) return false;
        d->introTimer = 2.0f;
        const bool now = svc->internetRouteAvailable();
        if (now == d->online) return false;
        d->online = now;
        return true;
    };
    page->onDraw = [d](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        if (d->online) {
            c.icon = ui::IconId::Download;
            c.headline = ui::Text::key("settings.library.cores.intro.headline");
            c.lines = {ui::Text::key("settings.library.cores.intro.p1"), ui::Text::key("settings.library.cores.intro.p2")};
        } else {
            c.icon = ui::IconId::Alert;
            c.tone = ui::Tone::Warning;
            c.headline = ui::Text::key("settings.library.cores.intro.offline");
            c.lines = {ui::Text::key("settings.library.cores.intro.offline.p1"), ui::Text::key("settings.library.cores.intro.offline.p2")};
        }
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onCanContinue = [d](ui::Text &reason) {   // greyed out while offline: A does nothing
        if (d->online) return true;
        reason = ui::Text::key("settings.library.cores.intro.offline.reason");
        return false;
    };
    page->onHints = [] { return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true)}; };
    page->onEvent = [svc, d](ui::WizardEvent e, ui::Wizard &w) {
        if (e != ui::WizardEvent::Confirm && e != ui::WizardEvent::Start) return;
        d->online = svc->internetRouteAvailable();
        if (d->online && svc->startCoreCheck(false)) w.go(kChecking);   // beginCoreCheck()
    };
    page->onBack = [] { return ui::kClose; };
    flow.add(kIntro, std::move(page));
}

// ---- Checking (SS:1367-1409): B stops the check and closes the flow (cancelCheck(); the worker ends by itself)
void addCheckingPage(WizardFlowPage &flow, LibraryService *svc, DataPtr d) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(ui::Text::key("settings.library.cores.title")));
    page->centered = true;
    ui::BusyPage *raw = page.get();
    auto fill = [svc, raw] {
        const CoreMraProgress &c = svc->cores();
        raw->status = ui::Text::key(c.totalFiles ? "settings.library.cores.checking.comparing" : "settings.library.cores.checking.loading");
        raw->waitNote = ui::Text::key(c.totalFiles ? "settings.library.cores.checking.wait.comparing" : "settings.library.cores.checking.wait.loading");
        raw->fraction = c.currentTotal ? float(double(c.completed) / double(c.currentTotal)) : -1.0f;
        raw->setDetail(c.completed == 0 ? ui::Text() : ui::Text::data(ui::plural("settings.library.cores.checking.counts", long(c.completed))));
    };
    page->onEnter = [raw, fill](ui::Wizard &) {
        raw->detail = ui::Text();
        fill();
    };
    page->onUpdate = [svc, d, fill](float, ui::Wizard &w) {
        const CoreMraProgress &c = svc->cores();
        if (c.phase == CoreMraProgress::Phase::Ready) { d->freshList = true; w.go(kList); }
        else if (c.phase == CoreMraProgress::Phase::Failed) w.go(kFailed);
        else fill();
        return true;
    };
    page->onLeave = [svc](bool) { if (svc->cores().phase == CoreMraProgress::Phase::Checking) svc->cancelCoreCheck(); };
    flow.add(kChecking, std::move(page));
}

// ---- List (SS:1410-1570, input SS:1264-1318): Multiple ChooseList over the rows the filter shows
void refreshList(LibraryService *svc, Data &d) {
    const CoreMraProgress &c = svc->cores();
    d.visible.clear();
    std::vector<ui::ChooseItem> items;
    for (size_t i = 0; i < c.entries.size(); ++i) {
        const CoreMraFile &f = c.entries[i];
        if (!filterShows(d.filter, f.status)) continue;
        d.visible.push_back(i);
        ui::ChooseItem it;
        it.title = ui::Text::data(friendlyName(f.name));
        it.detail = ui::Text::data(ui::joinParts({ui::Text::key(kindWord(f.type)).str(), f.path, sizeText(f.size)}));
        it.status = ui::Text::key(stateWord(f.status));
        it.statusTone = f.status == Status::Error ? ui::Tone::Warning : f.status == Status::UpToDate ? ui::Tone::Neutral : ui::Tone::Action;
        it.disabled = !selectable(f.status);   // a checkbox only where the file can be picked
        it.checked = f.selected && !it.disabled;
        items.push_back(it);
    }
    d.list.setItems(items);
    // The summary counts ALL files, not only the visible ones (SS:1456-1463, 1560-1568).
    d.selectedCount = d.selectedCores = d.selectedMras = 0;
    d.selectedBytes = 0;
    for (const CoreMraFile &f : c.entries) {
        if (!f.selected) continue;
        ++d.selectedCount;
        d.selectedBytes += f.size;
        if (f.type == Type::ArcadeMra) ++d.selectedMras; else ++d.selectedCores;
    }
}

void addListPage(WizardFlowPage &flow, LibraryService *svc, DataPtr d) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("settings.library.cores.title")));
    page->continueKey_ = ui::HintKey::Start;
    page->onEnter = [svc, d](ui::Wizard &) {
        if (d->freshList) { d->filter = 0; d->list.setCursor(0); d->freshList = false; }   // a new list starts at "Needs update"
        refreshList(svc, *d);
    };
    page->onCanContinue = [svc](ui::Text &reason) {
        if (anySelected(svc->cores())) return true;
        reason = ui::Text::key("settings.library.cores.list.pick");   // Start does nothing without a selection
        return false;
    };
    page->onDraw = [svc, d](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        const CoreMraProgress &c = svc->cores();
        // The filter row (tabs) with the age of the check at the right.
        static const char *names[] = {"settings.library.cores.filter.needs", "settings.library.cores.filter.all",
                                      "settings.library.cores.filter.uptodate", "settings.library.cores.filter.changed"};
        ui::TabRowSpec tabs;
        for (const char *n : names) tabs.labels.push_back(ui::Text::key(n));
        tabs.active = d->filter;
        const int tabH = ui::drawTabRow(p, t, {body.x, body.y, body.w, body.h}, tabs);
        const long long age = std::max(0LL, (long long)std::time(nullptr) - (long long)c.checkedAt);
        const std::string checked = age < 60 ? ui::Text::key("settings.library.cores.list.justNow").str()
                                             : ui::plural("settings.library.cores.list.minutes", long(age / 60));
        const int bodyLine = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body));
        const int rightW = std::max(0, std::min(p.measure(ui::FontRole::Regular, t.size(ui::TextRole::Body), checked), body.w / 3));
        ui::drawLabel(p, t, {body.right() - rightW, body.y + std::max(0, tabH - bodyLine) / 2, rightW, bodyLine}, checked,
                      ui::TextRole::Body, ui::TextTier::Muted, ui::Align::Right);
        int y = body.y + tabH + t.px(8);
        ui::drawLabel(p, t, {body.x, y, body.w, bodyLine}, ui::Text::key("settings.library.cores.list.intro").str(), ui::TextRole::Body,
                      ui::TextTier::Muted);
        y += bodyLine + t.px(12);
        // The summary line (always reserved): how many are selected over ALL files, and what they will download.
        const int statusH = ui::wizardStatusHeight(p, t);
        const ui::Rect status{body.x, body.bottom() - statusH, body.w, statusH};
        const ui::Rect area{body.x, y, body.w, std::max(0, status.y - t.px(8) - y)};
        if (d->visible.empty()) {
            ui::WizardCentered e;
            const bool needs = d->filter == 0;
            e.icon = needs ? ui::IconId::Check : ui::IconId::None;
            e.tone = needs ? ui::Tone::Done : ui::Tone::Neutral;
            e.headline = ui::Text::key(needs ? "settings.library.cores.list.emptyNeeds" : "settings.library.cores.list.emptyFilter");
            e.lines = {ui::Text::key("settings.library.cores.list.emptyHint")};
            ui::drawWizardCentered(p, t, area, e);
        } else {
            d->list.draw(p, t, area);
        }
        ui::Text line;   // nothing selected: the reason line below says so
        if (d->selectedCount)
            line = ui::Text::data(ui::format("settings.library.cores.list.status",
                {std::to_string(d->selectedCount),
                 ui::format("settings.library.cores.list.detail", {ui::plural("settings.library.cores.list.cores", long(d->selectedCores)),
                            ui::plural("settings.library.cores.list.mras", long(d->selectedMras)), sizeText(d->selectedBytes)})}));
        ui::drawWizardStatus(p, t, status, ui::Tone::Neutral, ui::Text(), line);
    };
    page->onHints = [d] {
        std::vector<ui::Hint> h;
        if (!d->visible.empty()) h.push_back(ui::hint(ui::HintKey::UpDown, "hint.move"));
        h.push_back(ui::hint(ui::HintKey::LeftRight, "settings.library.cores.hint.filter"));
        if (!d->visible.empty()) h.push_back(ui::hint(ui::HintKey::A, "hint.select", true));
        h.push_back(ui::hint(ui::HintKey::X, "settings.library.cores.hint.selectAll"));
        h.push_back(ui::hint(ui::HintKey::Y, "settings.library.cores.hint.again"));
        h.push_back(ui::hint(ui::HintKey::Start, "settings.library.cores.hint.review", true));
        return h;
    };
    page->onEvent = [svc, d](ui::WizardEvent e, ui::Wizard &w) {
        switch (e) {
            case ui::WizardEvent::Left:
            case ui::WizardEvent::Right:
                d->filter = (d->filter + (e == ui::WizardEvent::Left ? 3 : 1)) % 4;
                d->list.setCursor(0);
                refreshList(svc, *d);
                break;
            case ui::WizardEvent::X:   // select every missing or outdated file (over all files, not only the visible ones)
                svc->selectAllMissingOrOutdated();
                refreshList(svc, *d);
                break;
            case ui::WizardEvent::Y:   // ask the official source again instead of using the saved result
                if (svc->startCoreCheck(true)) w.go(kChecking);
                break;
            case ui::WizardEvent::Start:
                if (anySelected(svc->cores())) w.go(kReview);
                break;
            default:
                if (d->list.handle(e) == ui::ChooseOutcome::Toggled && !d->visible.empty()) {
                    svc->toggleCoreSelected(d->visible[size_t(d->list.cursor())]);
                    refreshList(svc, *d);
                }
                break;
        }
    };
    page->onBack = [] { return ui::kClose; };   // B closes the whole flow (SS:1253)
    flow.add(kList, std::move(page));
}

// ---- Review (SS:1571-1624, input SS:1253-1264): what will be downloaded, as scrolling text
void addReviewPage(WizardFlowPage &flow, LibraryService *svc, DataPtr d) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("settings.library.cores.title")));
    page->onEnter = [svc, d](ui::Wizard &) {
        const CoreMraProgress &c = svc->cores();
        long count = 0;
        uint64_t bytes = 0;
        std::string files;
        for (const CoreMraFile &f : c.entries) {
            if (!f.selected) continue;
            ++count;
            bytes += f.size;
            if (!files.empty()) files += "\n";
            files += ui::joinParts({f.path, sizeText(f.size)});
        }
        d->review.set({{ui::Text::data(ui::plural("settings.library.cores.review.count", count, {sizeText(bytes)})), ui::TextTier::Primary, true},
                       {ui::Text::key("settings.library.cores.review.note1"), ui::TextTier::Muted, false},
                       {ui::Text::key("settings.library.cores.review.note2"), ui::TextTier::Muted, false},
                       {ui::Text::data(files), ui::TextTier::Primary, false}});
    };
    page->onDraw = [d](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) { d->review.draw(p, t, body); };
    page->onHints = [d] {
        std::vector<ui::Hint> h;
        if (d->review.scrolls()) h.push_back(ui::hint(ui::HintKey::UpDown, "hint.scroll"));
        h.push_back(ui::hint(ui::HintKey::A, "settings.library.cores.hint.install", true));
        return h;
    };
    page->onEvent = [svc, d](ui::WizardEvent e, ui::Wizard &w) {
        if (e == ui::WizardEvent::Confirm || e == ui::WizardEvent::Start) {
            if (svc->startCoreInstall()) w.go(kInstalling);   // the install replaces the whole flow's page
        } else {
            d->review.handle(e);
        }
    };
    page->onBack = [] { return kList; };
    flow.add(kReview, std::move(page));
}

// ---- Installing (SS:1625-1632): no cancel, no back; one status line and a calm detail line, the counts are in the Result
void addInstallingPage(WizardFlowPage &flow, LibraryService *svc) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(ui::Text::key("settings.library.cores.title")));
    page->centered = true;
    page->cancelable = false;
    page->status = ui::Text::key("settings.library.cores.installing.status");
    page->detail = ui::Text::key("settings.library.cores.installing.power");
    page->waitNote = ui::Text::key("settings.library.cores.installing.wait");
    ui::BusyPage *raw = page.get();
    auto fill = [svc, raw] {
        const CoreMraProgress &c = svc->cores();
        if (!c.totalFiles) { raw->fraction = -1.0f; return; }
        // What now: the file being written; the power warning stays in the wait note's place only when nothing else says it.
        raw->setDetail(ui::Text::data(ui::format("settings.library.cores.installing.file",
                                                 {std::to_string(std::min<uint64_t>(c.installed + c.failed + 1, c.totalFiles)), std::to_string(c.totalFiles)})));
        const double file = c.currentTotal ? std::min(1.0, double(c.currentBytes) / double(c.currentTotal)) : 0.0;
        raw->fraction = float(std::min(1.0, (double(c.installed + c.failed) + file) / double(c.totalFiles)));
    };
    page->onEnter = [fill](ui::Wizard &) { fill(); };
    page->onUpdate = [svc, fill](float, ui::Wizard &w) {
        const CoreMraProgress &c = svc->cores();
        if (c.phase == CoreMraProgress::Phase::Complete || c.phase == CoreMraProgress::Phase::Failed) w.go(kResult);
        else fill();
        return true;
    };
    flow.add(kInstalling, std::move(page));
}

// ---- Result after an install (SS:1633-1640): the counts, and which kind of files did not make it
void addResultPage(WizardFlowPage &flow, LibraryService *svc) {
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(ui::Text::key("settings.library.cores.title")));
    ui::ResultPage *raw = page.get();
    page->onEnter = [svc, raw](ui::Wizard &) {
        const CoreMraProgress &c = svc->cores();
        ui::WizardResult r;
        if (!c.failed) {
            r.tone = ui::Tone::Done;
            r.headline = ui::Text::key("settings.library.cores.result.done");
            r.changed = ui::Text::data(ui::plural("settings.library.cores.result.installed", long(c.installed)));
            r.next = ui::Text::key("settings.library.cores.result.next");
        } else {
            r.tone = c.installed ? ui::Tone::Warning : ui::Tone::Error;
            r.headline = ui::Text::key(c.installed ? "settings.library.cores.result.some" : "settings.library.cores.result.none");
            r.changed = ui::Text::key("settings.library.cores.result.kept");
            if (c.installed) {
                ui::ResultItemSpec ok;
                ok.label = ui::Text::key("settings.library.cores.result.item.installed");
                ok.tone = ui::Tone::Done;
                ok.status = ui::Text::key("result.status.done");
                ok.count = ui::Text::data(std::to_string(c.installed));
                r.items.push_back(ok);
            }
            ui::ResultItemSpec bad;
            bad.label = ui::Text::key("settings.library.cores.result.item.failed");
            bad.tone = ui::Tone::Error;
            bad.status = ui::Text::key("settings.library.cores.result.item.failed.word");
            bad.count = ui::Text::data(std::to_string(c.failed));
            bad.details = ui::Text::key("settings.library.cores.result.item.failed.details");
            r.items.push_back(bad);
            r.next = ui::Text::key("settings.library.cores.result.retry");
            r.details = ui::Text::data(c.detail);
            r.retry = kIntro;   // check again; the list is built anew
        }
        raw->set(r);
    };
    flow.add(kResult, std::move(page));
}

// ---- Failed check (SS:1385-1395): nothing on the MiSTer was touched, so say that instead of counters
void addFailedPage(WizardFlowPage &flow, LibraryService *svc) {
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(ui::Text::key("settings.library.cores.title")));
    ui::ResultPage *raw = page.get();
    page->onEnter = [svc, raw](ui::Wizard &) {
        const CoreMraProgress &c = svc->cores();
        ui::WizardResult r;
        r.tone = ui::Tone::Error;
        r.headline = ui::Text::key("settings.library.cores.failed.headline");
        r.changed = ui::Text::key("settings.library.cores.failed.unchanged");
        r.next = ui::Text::key("settings.library.cores.failed.next");
        r.details = ui::Text::data(ui::joinParts({c.message.empty() ? ui::Catalog::get().tr("settings.library.cores.failed.default") : c.message,
                                                  c.detail}));
        r.retry = kIntro;   // A: back to the first dialog (openCoreUpdater())
        raw->set(r);
    };
    flow.add(kFailed, std::move(page));
}

std::unique_ptr<WizardFlowPage> makeCoreFlow(LibraryService *svc) {
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(kCoreFrame));
    DataPtr d(new Data);
    addIntroPage(*flow, svc, d);
    addCheckingPage(*flow, svc, d);
    addListPage(*flow, svc, d);
    addReviewPage(*flow, svc, d);
    addInstallingPage(*flow, svc);
    addResultPage(*flow, svc);
    addFailedPage(*flow, svc);
    return flow;
}

}  // namespace

void openCoreUpdater(LibraryService &svc, PageStack &pages) { WizardFlowPage::launch(pages, makeCoreFlow(&svc), kIntro); }

namespace detail {
// For the host runner: the flow opened at a page (the sample service decides what the page shows).
// which: 0 intro, 1 checking, 2 list, 3 review, 4 installing, 5 result, 6 failed check.
void openCoreAt(LibraryService *svc, PageStack &pages, int which) {
    static const ui::PageId ids[] = {kIntro, kChecking, kList, kReview, kInstalling, kResult, kFailed};
    if (which == 1 && svc->cores().phase != CoreMraProgress::Phase::Checking) svc->startCoreCheck(false);
    WizardFlowPage::launch(pages, makeCoreFlow(svc), ids[which]);
}
}  // namespace detail

void registerCoreStrings(ui::Catalog &c) {
    // The flow's title: one name for every page (the row, the pages and the results).
    c.set("settings.library.cores.title", "Core and MRA updates");
    // Intro (SS:1355-1366)
    c.set("settings.library.cores.intro.headline", "Check for new cores and MRAs");
    c.set("settings.library.cores.intro.p1", "Compares your cores and Arcade MRAs with the official MiSTer Distribution. The check changes nothing.");
    c.set("settings.library.cores.intro.p2", "You choose what to download. BIOS files and ROMs are not included.");
    c.set("settings.library.cores.intro.offline", "No internet connection");
    c.set("settings.library.cores.intro.offline.p1", "Checking for updates needs an Internet connection. Your installed cores keep working.");
    c.set("settings.library.cores.intro.offline.p2", "Connect under Settings \xE2\x86\x92 System \xE2\x86\x92 Connect to Wi-Fi. This page notices when you are online.");
    c.set("settings.library.cores.intro.offline.reason", "Connect to a network to check for updates.");
    // Checking (SS:1367-1409)
    c.set("settings.library.cores.checking.comparing", "Comparing with the official list");
    c.set("settings.library.cores.checking.loading", "Loading the official list");
    c.set("settings.library.cores.checking.counts.one", "{1} file checked");
    c.set("settings.library.cores.checking.counts.other", "{1} files checked");
    // List (SS:1410-1570, 2780-2790)
    c.set("settings.library.cores.list.intro", "Pick the files to install. Files that are not installed or changed differ from the official ones.");
    c.set("settings.library.cores.list.justNow", "Checked just now");
    c.set("settings.library.cores.list.minutes.one", "Checked {1} minute ago");
    c.set("settings.library.cores.list.minutes.other", "Checked {1} minutes ago");
    c.set("settings.library.cores.filter.needs", "Needs update");
    c.set("settings.library.cores.filter.all", "All");
    c.set("settings.library.cores.filter.uptodate", "Up to date");
    c.set("settings.library.cores.filter.changed", "Changed");
    c.set("settings.library.cores.kind.arcade", "Arcade core");
    c.set("settings.library.cores.kind.console", "Console core");
    c.set("settings.library.cores.kind.other", "Other official core");
    c.set("settings.library.cores.kind.mra", "Arcade MRA");
    c.set("settings.library.cores.state.uptodate", "Up to date");
    c.set("settings.library.cores.state.missing", "Not installed");
    c.set("settings.library.cores.state.update", "Update available");
    c.set("settings.library.cores.state.changed", "Changed");
    c.set("settings.library.cores.state.error", "Cannot be checked");
    c.set("settings.library.cores.list.emptyNeeds", "Everything is up to date");
    c.set("settings.library.cores.list.emptyFilter", "No files match this filter");
    c.set("settings.library.cores.list.emptyHint", "Change the filter above to see other files.");
    c.set("settings.library.cores.list.pick", "Pick at least one file to continue.");
    c.set("settings.library.cores.list.status", "{1} selected  \xC2\xB7  {2}");
    c.set("settings.library.cores.list.detail", "{1} + {2}  \xC2\xB7  download: {3}");
    c.set("settings.library.cores.list.cores.one", "{1} core");
    c.set("settings.library.cores.list.cores.other", "{1} cores");
    c.set("settings.library.cores.list.mras.one", "{1} MRA");
    c.set("settings.library.cores.list.mras.other", "{1} MRAs");
    c.set("settings.library.cores.hint.filter", "Change filter");
    c.set("settings.library.cores.hint.selectAll", "Select needed");
    c.set("settings.library.cores.hint.again", "Check again");
    c.set("settings.library.cores.hint.review", "Review");
    // Review (SS:1571-1624)
    c.set("settings.library.cores.review.count.one", "{1} file will be downloaded ({2}).");
    c.set("settings.library.cores.review.count.other", "{1} files will be downloaded ({2}).");
    c.set("settings.library.cores.review.note1", "Each file is checked against the official manifest before it replaces its target.");
    c.set("settings.library.cores.review.note2", "No system files, firmware, BIOS files or ROMs are updated.");
    c.set("settings.library.cores.hint.install", "Install");
    // Installing and results (SS:1625-1640)
    c.set("settings.library.cores.installing.status", "Installing updates");
    c.set("settings.library.cores.installing.power", "Keep the MiSTer powered on.");
    c.set("settings.library.cores.installing.file", "File {1} of {2}. Keep the MiSTer powered on.");
    c.set("settings.library.cores.installing.wait", "Large files take a while to download. Nothing is stuck; do not switch the MiSTer off.");
    c.set("settings.library.cores.checking.wait.loading", "The official list is downloaded first. A slow connection takes longer. Nothing is stuck.");
    c.set("settings.library.cores.checking.wait.comparing", "Every file on your drives is compared with the official list. Nothing is stuck.");
    c.set("settings.library.cores.result.done", "Updates installed");
    c.set("settings.library.cores.result.installed.one", "{1} file was installed.");
    c.set("settings.library.cores.result.installed.other", "{1} files were installed.");
    c.set("settings.library.cores.result.next", "You can check again any time under Settings \xE2\x86\x92 Library \xE2\x86\x92 Core and MRA updates.");
    c.set("settings.library.cores.result.some", "Some updates could not be installed");
    c.set("settings.library.cores.result.none", "No update could be installed");
    c.set("settings.library.cores.result.kept", "Files that could not be installed were left as they were.");
    c.set("settings.library.cores.result.retry", "Try again to check for updates and install the rest.");
    c.set("settings.library.cores.result.item.installed", "Installed files");
    c.set("settings.library.cores.result.item.failed", "Files not installed");
    c.set("settings.library.cores.result.item.failed.word", "Failed");
    c.set("settings.library.cores.result.item.failed.details", "The old files are still in place.");
    // Check failed (SS:1385-1395)
    c.set("settings.library.cores.failed.headline", "Could not check for updates");
    c.set("settings.library.cores.failed.default", "The official update list could not be loaded.");
    c.set("settings.library.cores.failed.unchanged", "Nothing on your MiSTer was changed.");
    c.set("settings.library.cores.failed.next", "Check the network connection, then try again.");
}

}  // namespace library
}  // namespace patsgui
