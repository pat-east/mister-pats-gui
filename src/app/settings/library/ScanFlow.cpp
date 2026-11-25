// Build game list: the port of Classic's ScanScreen pages Intro, Working and Done (src/legacy/ScanScreen.cpp), shown as a
// Wizard (ui::Wizard through WizardFlowPage): every page is a dialog page of one Medium frame, each page one thing, large and
// centred. The scan itself runs behind LibraryService (Classic's LibraryScan, stepped once per frame by the App loop); these
// pages only read its state, as ScanScreen::update() does. The state machine is Classic's, with one owner-approved addition:
// the Drives step (Classic scans every detected root without asking).
//
//   Start -A-> Drives -A/Start-> Working -(scan done)-> box-art Offer (ArtworkFlow.cpp) -A-> box-art Working -(ended)-> Done
//                                        \-(failed or cancelled)-> Done
// Back: Start "Close" (= Classic's "Do this later"), Drives "Back" (to Start), Working "Cancel" (stops the scan, then the page
// ends with the scan), Offer "Skip" (Done: a scan ran). The Done result ends the visit; "Try again" after a failed scan goes
// back to Drives. The first-run pages (Welcome, JumpStart, Overview) are not part of Settings.

#include <cstdio>
#include <memory>

#include "app/settings/library/FlowShared.h"
#include "app/settings/library/LibraryPages.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace library {
namespace {

// The ids of the pages of "Build game list" (the box-art pages are kOffer and kOffer + 1).
enum { kStart = 0, kDrives = 1, kWork = 2, kOffer = 3, kDone = 5 };

bool scanRunning(const ScanProgress &s) {
    return s.state == ScanProgress::State::Discovering || s.state == ScanProgress::State::Scanning ||
           s.state == ScanProgress::State::Writing;
}

// "420 GB used of 1 TB" (decimal units, as the drive's label says them).
std::string sizeText(uint64_t bytes) {
    const double gb = double(bytes) / 1e9;
    char buf[32];
    if (gb >= 1000.0) std::snprintf(buf, sizeof buf, "%.1f TB", gb / 1000.0);
    else std::snprintf(buf, sizeof buf, "%.0f GB", gb);
    return buf;
}

// ---- Start: what this does, before anything happens (Classic's Intro text, as one centred page)
void addStartPage(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit) {
    ui::Wizard *wz = &flow.wizard();
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("settings.library.scan.title")));
    page->onEnter = [visit](ui::Wizard &) { *visit = Visit(); };
    page->onDraw = [](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Search;
        c.headline = ui::Text::key("settings.library.scan.start.headline");
        c.lines = {ui::Text::key("settings.library.scan.start.p1"), ui::Text::key("settings.library.scan.start.p2")};
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] { return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true)}; };
    page->onEvent = [](ui::WizardEvent e, ui::Wizard &w) {
        if (e == ui::WizardEvent::Confirm || e == ui::WizardEvent::Start) w.go(kDrives);   // Classic: Start acts like A here
    };
    page->onLeave = [svc, wz](bool cancelled) {
        if (cancelled && !wz->active()) svc->libraryTaskClosed();   // B: onClose_ = App::closeScan
    };
    flow.add(kStart, std::move(page));
}

// ---- Drives (owner): which of the detected roots to search. All are checked (Classic scans all of them).
struct DrivesData {
    std::vector<std::string> roots;
    ui::ChooseList list{ui::ChooseMode::Multiple};
};

void addDrivesPage(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit) {
    auto data = std::make_shared<DrivesData>();
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("settings.library.scan.title")));
    page->continueKey_ = ui::HintKey::Start;
    auto reread = [svc, data] {   // ScanScreen::reset(): detectRoots() when the page opens
        data->roots = svc->scanRoots();
        std::vector<ui::ChooseItem> items;
        for (const std::string &root : data->roots) {
            ui::ChooseItem it;
            it.title = ui::Text::data(root);
            const DriveSpace space = svc->driveSpace(root);
            if (space.totalBytes > 0 && space.freeBytes <= space.totalBytes)
                it.detail = ui::Text::data(ui::format("settings.library.scan.drives.used",
                                                      {sizeText(space.totalBytes - space.freeBytes), sizeText(space.totalBytes)}));
            it.checked = true;
            items.push_back(it);
        }
        data->list.setItems(items);
        data->list.setCursor(0);
    };
    page->onEnter = [visit, reread](ui::Wizard &) { *visit = Visit(); reread(); };
    page->onCanContinue = [data](ui::Text &reason) {
        if (data->roots.empty() || data->list.checkedCount() > 0) return true;
        reason = ui::Text::key("settings.library.scan.drives.none");
        return false;
    };
    page->onDraw = [data](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        if (data->roots.empty()) {   // nothing to choose: a centred page, not an empty list
            ui::WizardCentered c;
            c.icon = ui::IconId::Drive;
            c.tone = ui::Tone::Warning;
            c.headline = ui::Text::key("settings.library.scan.nodrives.headline");
            c.lines = {ui::Text::key("settings.library.scan.nodrives.p1")};
            ui::drawWizardCentered(p, t, body, c);
            return;
        }
        drawQuestionList(p, t, body, ui::Text::key("settings.library.scan.drives.headline"),
                         ui::Text::key("settings.library.scan.drives.p1"), data->list, 0);
    };
    page->onHints = [data] {
        if (data->roots.empty()) return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.tryAgain", true)};
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::UpDown, "hint.move"), ui::hint(ui::HintKey::A, "hint.select", false),
                                     ui::hint(ui::HintKey::Start, "settings.library.scan.drives.go", true)};
    };
    page->onEvent = [svc, visit, data, reread](ui::WizardEvent e, ui::Wizard &w) {
        if (data->roots.empty()) {
            if (e == ui::WizardEvent::Confirm) reread();   // Try again: the drives are read anew
            return;
        }
        if (e == ui::WizardEvent::Start) {
            // scan_.start(roots_), scanRan_ = true, page Working
            std::vector<std::string> chosen;
            for (int row : data->list.checked()) chosen.push_back(data->roots[size_t(row)]);
            svc->startScan(chosen);
            visit->scanRan = true;
            w.go(kWork);
        } else {
            data->list.handle(e);
        }
    };
    page->onBack = [] { return kStart; };
    flow.add(kDrives, std::move(page));
}

// ---- Working: the phase and one bar for the whole scan.
// Classic's LibraryScan::progress() runs 0..1 for the systems, restarts for the Arcade part and restarts again for saving the
// lists. The page joins them into one bar that only moves forward. The weights are estimates, written here and nowhere
// else: systems 0..0.60, Arcade 0.60..0.85, saving 0.85..1.00 (saving is short but not instant: about 9 s on 10 000 games).
// A collection without Arcade games goes from 0.60 straight to the saving part: the bar jumps once, it never goes back.
enum class ScanPhase { Discovering, Systems, Arcade, Writing };

ScanPhase scanPhase(const ScanProgress &sp) {
    using S = ScanProgress::State;
    if (sp.state == S::Writing || sp.state == S::Done) return ScanPhase::Writing;
    if (sp.state == S::Scanning) return sp.statusLine.compare(0, 6, "Arcade") == 0 ? ScanPhase::Arcade : ScanPhase::Systems;
    return ScanPhase::Discovering;
}

// The Arcade lines of LibraryScan::statusLine() start with "Arcade" and a separator ("Arcade  \xC2\xB7  Checking Arcade game 3 of 40",
// "Arcade: 300 games ready ..."); the phrase above already says it, so the detail line starts after it.
std::string withoutArcadePrefix(const std::string &line) {
    size_t i = 6;
    while (i < line.size()) {
        if (line[i] == ' ' || line[i] == ':') ++i;
        else if (line.compare(i, 2, "\xC2\xB7") == 0) i += 2;
        else break;
    }
    return i < line.size() ? line.substr(i) : line;
}

float overallFraction(ScanPhase phase, float progress) {
    const float p = std::max(0.0f, std::min(1.0f, progress));
    switch (phase) {
        case ScanPhase::Discovering: return 0.0f;
        case ScanPhase::Systems: return 0.60f * p;
        case ScanPhase::Arcade: return 0.60f + 0.25f * p;
        case ScanPhase::Writing: return 0.85f + 0.15f * p;
    }
    return 0.0f;
}

}  // namespace

// ---- Working (Scan:274-281, update Scan:144-173). Public: the first-run Jump Start shows the same page (src/app/FirstRun.cpp).
void addScanWorking(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId id, ui::PageId doneId, ui::PageId failedId,
                    const ui::Text &title, std::function<ui::PageId(const ScanProgress &)> afterScan) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(title.empty() ? ui::Text::key("settings.library.scan.title") : title));
    page->centered = true;
    auto stopping = std::make_shared<bool>(false);
    auto peak = std::make_shared<float>(0.0f);   // the bar only moves forward (see overallFraction)
    ui::BusyPage *raw = page.get();
    auto fill = [svc, raw, stopping, peak]() {
        const ScanProgress sp = svc->scan();
        using S = ScanProgress::State;
        // The phrase names the phase; the service's own status line says what is read right now (Classic's text, shown
        // as the detail line; the page limits it to once a second).
        const ScanPhase phase = scanPhase(sp);
        const char *status = phase == ScanPhase::Writing ? "settings.library.scan.working.saving"
                           : phase == ScanPhase::Arcade ? "settings.library.scan.working.arcade"
                           : phase == ScanPhase::Systems ? "settings.library.scan.working.reading"
                           : "settings.library.scan.working.looking";
        const char *note = phase == ScanPhase::Writing ? "settings.library.scan.wait.saving"
                         : phase == ScanPhase::Arcade ? "settings.library.scan.wait.arcade"
                         : phase == ScanPhase::Systems ? "settings.library.scan.wait.reading"
                         : "settings.library.scan.wait.looking";
        raw->status = ui::Text::key(*stopping ? "settings.library.scan.working.stopping" : status);
        raw->waitNote = ui::Text::key(note);
        *peak = std::max(*peak, overallFraction(phase, sp.progress));
        raw->fraction = *peak;
        if (*stopping || sp.state == S::Failed || sp.state == S::Done) {
            raw->detail = ui::Text();
            raw->facts = ui::Text();
            return;
        }
        ui::Text facts;
        if (sp.systemsFound != 0 || sp.gamesFound != 0)
            facts = ui::Text::data(ui::plural("settings.library.scan.working.systems", long(sp.systemsFound)) + ", " +
                                   ui::plural("settings.library.scan.working.games", long(sp.gamesFound)));
        raw->setDetail(sp.statusLine.empty() ? ui::Text() : ui::Text::data(phase == ScanPhase::Arcade ? withoutArcadePrefix(sp.statusLine) : sp.statusLine), facts);
    };
    page->onEnter = [fill, stopping, raw, peak](ui::Wizard &) {
        *stopping = false;
        *peak = 0.0f;
        raw->cancelable = true;
        raw->detail = ui::Text();
        raw->facts = ui::Text();
        fill();
    };
    // B: scan_.cancel(); the page ends when the scan has (Failed with the service's "cancelled" text), so show "Stopping".
    page->onCancel = [svc, visit, stopping, raw](ui::Wizard &) {
        if (scanRunning(svc->scan())) visit->scanCancelled = true;
        svc->cancelScan();
        *stopping = true;
        raw->cancelable = false;
    };
    page->onUpdate = [svc, fill, doneId, failedId, afterScan](float, ui::Wizard &w) {
        fill();
        const ScanProgress sp = svc->scan();
        if (sp.state == ScanProgress::State::Done) {
            // The library was reloaded by the host (Classic: onFinished_ = App::reloadLibrary()) before this frame.
            // Default: the box-art offer, only worth showing once there is a library. `afterScan` decides otherwise (Jump Start).
            w.go(afterScan ? afterScan(sp) : doneId);
        } else if (sp.state == ScanProgress::State::Failed) {
            w.go(failedId);
        }
        return true;
    };
    page->onLeave = [svc](bool) { if (scanRunning(svc->scan())) svc->cancelScan(); };   // the GUI closed: nothing runs unseen
    flow.add(id, std::move(page));
}

namespace {

std::unique_ptr<WizardFlowPage> newFlow() { return std::unique_ptr<WizardFlowPage>(new WizardFlowPage(kLibraryFlowFrame)); }

}  // namespace

// The question and the rows are one block, top-aligned right under the title (like every list page; a long list fills the
// page and scrolls). Centring the block left an empty band above and below it, unlike the lists of the other flows.
void drawQuestionList(ui::Painter &p, const ui::Theme &t, const ui::Rect &body, const ui::Text &question, const ui::Text &line,
                      ui::ChooseList &list, int reserveBottom) {
    const ui::Rect col = ui::wizardTextColumn(t, body);
    const int headH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Heading));
    const int bodyH = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body));
    const int headLines = std::max(1, std::min<int>(2, int(ui::wrapText(p, t, question.str(), ui::TextRole::Heading, true, col.w).size())));
    const int lineLines = std::max(1, std::min<int>(5, int(ui::wrapText(p, t, line.str(), ui::TextRole::Body, false, col.w).size())));
    const int headBlock = headLines * (headH + t.px(ui::Space::lineExtra));
    const int lineBlock = lineLines * (bodyH + t.px(ui::Space::lineExtra));
    int y = body.y + t.px(12);
    ui::drawTextBlock(p, t, {col.x, y, col.w, headBlock}, question.str(), ui::TextRole::Heading, ui::TextTier::Primary, true, 2);
    y += headBlock + t.px(8);
    ui::drawTextBlock(p, t, {col.x, y, col.w, lineBlock}, line.str(), ui::TextRole::Body, ui::TextTier::Muted, false, 5);
    y += lineBlock + t.px(24);
    list.draw(p, t, {body.x, y, body.w, std::max(0, body.bottom() - reserveBottom - y)});
}

// ---- Done (Scan:492-553): the Result page both flows end in; A or B end the visit (onClose_ = App::closeScan)
void addDonePage(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId id, ui::PageId retryScan,
                 ui::PageId retryArtwork) {
    ui::Wizard *wz = &flow.wizard();
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(ui::Text::key("settings.library.scan.title")));
    ui::ResultPage *raw = page.get();
    page->onEnter = [svc, visit, raw, retryScan, retryArtwork](ui::Wizard &) {
        const ScanProgress sp = svc->scan();
        const ArtworkProgress ap = svc->artwork();
        using AS = ArtworkProgress::State;
        const std::string counts = ui::format("settings.library.done.art.counts", {std::to_string(ap.fetched), std::to_string(ap.prepared),
                                                                                    std::to_string(ap.skipped), std::to_string(ap.missing)});
        raw->title_ = ui::Text::key(visit->scanRan ? "settings.library.scan.title" : "settings.library.artwork.offer.title");
        ui::WizardResult r;
        if (visit->scanRan) {
            const bool ok = sp.state == ScanProgress::State::Done;
            if (ok) {
                r.tone = ui::Tone::Done;
                r.headline = ui::Text::key("settings.library.done.ready");
                r.changed = ui::Text::data(ui::format("settings.library.done.ready.body",
                                                      {std::to_string(sp.systemsFound), std::to_string(sp.gamesFound)}));
                r.next = ui::Text::key("settings.library.done.ready.next");
            } else if (visit->scanCancelled) {   // the user stopped it: nothing failed
                r.tone = ui::Tone::Warning;
                r.status = ui::Text::key("settings.library.done.stopped.word");
                r.headline = ui::Text::key("settings.library.done.stopped");
                r.changed = ui::Text::key("settings.library.done.stopped.changed");
                r.next = ui::Text::key("settings.library.done.stopped.next");
            } else {                             // the scan failed: say what, the service text is the Details
                r.tone = ui::Tone::Error;
                r.headline = ui::Text::key("settings.library.done.failed");
                r.changed = ui::Text::key("settings.library.done.stopped.changed");
                r.next = ui::Text::key("settings.library.done.failed.next");
                r.details = ui::Text::data(sp.error);
                r.retry = retryScan;
            }
            if (ok && visit->artworkRan) {   // Classic prints these whenever the shared state is not Idle; here only for a job of this visit
                std::string art = counts;
                if (ap.state == AS::Failed)
                    art = ui::Catalog::get().tr(visit->artworkCancelled ? "settings.library.done.art.cancelled" : "settings.library.done.art.unfinished") + " " + art;
                r.changed = ui::Text::data(r.changed.str() + " " + art);
                if (ap.state == AS::Failed && !visit->artworkCancelled && !ap.error.empty()) r.details = ui::Text::data(ap.error);
                if (ap.missing) r.next = ui::Text::data(r.next.str() + " " + ui::format("settings.library.done.art.misses", {ap.missesFile}));
            }
        } else {
            const bool ok = ap.state == AS::Done;
            const bool nothing = ap.fetched == 0 && ap.prepared == 0 && ap.skipped == 0;   // a job that ended without any cover
            if (ok) {
                r.tone = ui::Tone::Done;
                r.headline = ui::Text::key("settings.library.done.art.ready");
                r.changed = ui::Text::data(counts);
                r.next = ui::Text::key("settings.library.done.art.ready.next");
            } else if (visit->artworkCancelled) {   // the user stopped it: nothing failed, what arrived stays
                r.tone = ui::Tone::Warning;
                r.status = ui::Text::key("settings.library.done.stopped.word");
                r.headline = ui::Text::key("settings.library.done.art.stopped");
                r.changed = ui::Text::data(nothing ? ui::Catalog::get().tr("settings.library.done.art.none") : counts);
                r.next = ui::Text::key("settings.library.done.art.stopped.next");
            } else {                                // failed, or no cover matched: say what, the service text is the Details
                r.tone = ui::Tone::Error;
                r.headline = ui::Text::key(nothing ? "settings.library.done.art.nomatch" : "settings.library.done.art.failed");
                r.changed = ui::Text::data(nothing ? ui::Catalog::get().tr("settings.library.done.art.none")
                                                   : ui::Catalog::get().tr("settings.library.done.art.unfinished") + " " + counts);
                r.next = ui::Text::key(nothing ? "settings.library.done.art.next.none" : "settings.library.done.art.next");
                r.details = ui::Text::data(ap.error);
                r.retry = retryArtwork;
            }
            if (ap.missing && (ok || !nothing))
                r.next = ui::Text::data((r.next.empty() ? std::string() : r.next.str() + " ") +
                                        ui::format("settings.library.done.art.misses", {ap.missesFile}));
        }
        raw->set(r);
    };
    page->onFinished = [svc, wz] {
        if (!wz->active()) svc->libraryTaskClosed();   // A or B ended the visit (not "Try again", which goes on to a page)
    };
    flow.add(id, std::move(page));
}

namespace {
// The whole flow; `first` is the page it opens at (the host runner opens it in the middle).
std::unique_ptr<WizardFlowPage> makeScanFlow(LibraryService *svc, VisitPtr visit) {
    std::unique_ptr<WizardFlowPage> flow = newFlow();
    addStartPage(*flow, svc, visit);
    addDrivesPage(*flow, svc, visit);
    addScanWorking(*flow, svc, visit, kWork, kOffer, kDone);
    addArtworkPages(*flow, svc, visit, kOffer, kDone);
    addDonePage(*flow, svc, visit, kDone, kDrives, ui::kNoPage);
    return flow;
}
}  // namespace

void openScanIntro(LibraryService &svc, PageStack &pages) {
    VisitPtr visit(new Visit);
    WizardFlowPage::launch(pages, makeScanFlow(&svc, visit), kStart);
}

namespace detail {
// For the host runner: the flow opened in a given state (the sample service decides what the page shows).
// which: 0 scan working, 1 scan done/stopped, 2 box-art working, 3 box-art done/stopped.
void openFlowAt(LibraryService *svc, PageStack &pages, int which) {
    VisitPtr visit(new Visit);
    visit->scanRan = which <= 1;
    visit->artworkRan = which >= 2 || svc->artwork().state != ArtworkProgress::State::Idle;
    if (which == 0 && svc->scan().state == ScanProgress::State::Idle) svc->startScan(svc->scanRoots());
    if (which == 2 && svc->artwork().state == ArtworkProgress::State::Idle) svc->startArtwork({});
    const ui::PageId first = which == 0 ? kWork : which == 2 ? kOffer + 1 : kDone;
    WizardFlowPage::launch(pages, makeScanFlow(svc, visit), first);
}
}  // namespace detail

void registerScanStrings(ui::Catalog &c) {
    // The flow's title: one name for the row, the pages and the result.
    c.set("settings.library.scan.title", "Build game list");
    // Start, Drives
    c.set("settings.library.scan.start.headline", "Build the game list");
    c.set("settings.library.scan.start.p1", "Searches your drives and saves a new game list.");
    c.set("settings.library.scan.start.p2", "It only reads, nothing on your drives is changed. This can take a few minutes.");
    c.set("settings.library.scan.drives.headline", "Which drives should be searched?");
    c.set("settings.library.scan.drives.p1", "Checked drives are searched for games.");
    c.set("settings.library.scan.drives.go", "Search");
    c.set("settings.library.scan.drives.used", "{1} used of {2}");
    c.set("settings.library.scan.drives.none", "Check at least one drive to search.");
    c.set("settings.library.scan.nodrives.headline", "No drives found");
    c.set("settings.library.scan.nodrives.p1", "Connect a game drive, then try again.");
    // Working (Scan:274-281, 596-606): one closed phrase per phase and one short detail line
    c.set("settings.library.scan.working.looking", "Looking for game folders");
    c.set("settings.library.scan.working.reading", "Reading your game folders");
    c.set("settings.library.scan.working.saving", "Saving the game list");
    c.set("settings.library.scan.working.stopping", "Stopping the scan");
    c.set("settings.library.scan.working.arcade", "Reading the Arcade games");
    c.set("settings.library.scan.working.systems.one", "{1} system");
    c.set("settings.library.scan.working.systems.other", "{1} systems");
    c.set("settings.library.scan.working.games.one", "{1} game");
    c.set("settings.library.scan.working.games.other", "{1} games");
    // Shown by the page when nothing changed for 10 s: what is being waited for, and that nothing is stuck.
    c.set("settings.library.scan.wait.looking", "Looking through the drives takes longer when there are many folders. Nothing is stuck.");
    c.set("settings.library.scan.wait.reading", "Big collections take a few minutes. Nothing is stuck.");
    c.set("settings.library.scan.wait.arcade", "Reading the Arcade games takes the longest on big collections. Nothing is stuck.");
    c.set("settings.library.scan.wait.saving", "Saving the lists to the drive takes a moment. Nothing is stuck.");
    // Done (Scan:492-553, 611-615)
    c.set("settings.library.done.ready", "Game list ready");
    c.set("settings.library.done.ready.body", "{1} systems and {2} games were added to the game list.");
    c.set("settings.library.done.ready.next", "The saved list is kept on the MiSTer. Your game files are not changed.");
    c.set("settings.library.done.stopped.word", "Stopped");
    c.set("settings.library.done.stopped", "Scan stopped");
    c.set("settings.library.done.stopped.changed", "Nothing new was saved. The old game list is kept.");
    c.set("settings.library.done.stopped.next", "You can start another scan in Settings \xE2\x86\x92 Library \xE2\x86\x92 Build game list.");
    c.set("settings.library.done.failed", "The scan could not finish");
    c.set("settings.library.done.failed.next", "Check that your game drives are connected, then try again.");
    c.set("settings.library.done.art.ready", "Box art ready");
    c.set("settings.library.done.art.ready.next", "Covers are used in the game views. Your game files are not changed.");
    c.set("settings.library.done.art.stopped", "Box art stopped");
    c.set("settings.library.done.art.stopped.next", "Run Prepare box art again to continue; covers found so far are kept.");
    c.set("settings.library.done.art.failed", "Box art could not be finished");
    c.set("settings.library.done.art.nomatch", "No box art could be added");
    c.set("settings.library.done.art.none", "No covers were added.");
    c.set("settings.library.done.art.unfinished", "The cover search did not finish.");
    c.set("settings.library.done.art.cancelled", "The cover search was stopped.");
    c.set("settings.library.done.art.next.none", "Check your Internet connection, then try again.");
    c.set("settings.library.done.art.next", "Covers found so far are kept. Check your Internet connection, then try again.");
    c.set("settings.library.done.art.counts", "Downloaded {1} covers, prepared {2}, {3} already ready, {4} not found.");
    c.set("settings.library.done.art.misses", "Games with no matching cover are listed in {1}");
}

}  // namespace library
}  // namespace patsgui
