// Prepare box art: Classic's ScanScreen pages ArtworkOffer, ArtworkWorking (Scan:382-444). Also the second half of
// "Build game list" (the scan ends in the offer). The Done page both flows end in is addDonePage() in ScanFlow.cpp.

#include <memory>

#include "app/settings/library/FlowShared.h"
#include "app/settings/library/LibraryPages.h"
#include "app/settings/library/Values.h"
#include "app/settings/StandardPages.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace library {
namespace {

bool artworkRunning(const ArtworkProgress &a) {
    return a.state == ArtworkProgress::State::Preparing || a.state == ArtworkProgress::State::Working;
}

// ---- ArtworkOffer (Scan:382-418, changed by the owner: a list with multiple selection instead of one choice)
// One page, laid out like the Drives page of "Build game list": the question, one muted line, the systems as a list ("All
// systems" first, chosen by default), and under it the scope line. Choosing a system takes "All systems" off; choosing "All
// systems" clears the others; clearing the last system chooses "All systems" again (ChooseList::setExclusiveFirst). A toggles
// the row, Start downloads, B skips; the scope line follows the choice. Why no extra explanation page: the one muted line says
// what happens, and the chain Start -> Drives -> Working -> this page is long enough already.
struct OfferData {
    std::vector<ArtworkSystem> systems;
    ui::ChooseList list{ui::ChooseMode::Multiple};
};

uint64_t scopeGames(LibraryService *svc, const OfferData &d) {
    if (d.list.items().empty() || d.list.items()[0].checked) return svc->libraryGameCount();
    uint64_t games = 0;
    for (int row : d.list.checked()) if (row >= 1) games += d.systems[size_t(row - 1)].games;
    return games;
}

std::vector<std::string> chosenKeys(const OfferData &d) {   // empty = all systems
    std::vector<std::string> keys;
    if (d.list.items().empty() || d.list.items()[0].checked) return keys;
    for (int row : d.list.checked()) if (row >= 1) keys.push_back(d.systems[size_t(row - 1)].key);
    return keys;
}

void addOffer(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId offerId, ui::PageId doneId) {
    std::shared_ptr<OfferData> data(new OfferData);
    data->list.setExclusiveFirst(true);
    std::unique_ptr<ui::FnPage> pg(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("settings.library.artwork.offer.title")));
    pg->continueKey_ = ui::HintKey::Start;
    pg->onEnter = [svc, data](ui::Wizard &) {   // refreshArtworkSystems(): read when the page opens
        data->systems = svc->artworkSystems();
        std::vector<ui::ChooseItem> items;
        ui::ChooseItem all;
        all.title = ui::Text::key("settings.library.artwork.offer.all");
        all.checked = true;
        items.push_back(all);
        for (const ArtworkSystem &s : data->systems) {
            ui::ChooseItem it;
            it.title = ui::Text::data(s.name);
            items.push_back(it);
        }
        data->list.setItems(items);
        data->list.setCursor(0);
    };
    pg->onDraw = [svc, data](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        const int statusH = ui::wizardStatusHeight(p, t);
        const ui::Rect area{body.x, body.y, body.w, body.h - statusH - t.px(8)};
        if (data->systems.empty()) {   // nothing to choose between: one centred question for all games
            ui::WizardCentered c;
            c.icon = ui::IconId::Search;
            c.headline = ui::Text::key("settings.library.artwork.offer.headline.all");
            c.lines = {ui::Text::key("settings.library.artwork.offer.line")};
            ui::drawWizardCentered(p, t, area, c);
        } else {
            drawQuestionList(p, t, area, ui::Text::key("settings.library.artwork.offer.headline"),
                             ui::Text::key("settings.library.artwork.offer.line"), data->list, 0);
        }
        ui::drawWizardStatus(p, t, {body.x, body.bottom() - statusH, body.w, statusH}, ui::Tone::Neutral, ui::Text(),
                             ui::Text::data(ui::format("settings.library.artwork.offer.scope", {std::to_string(scopeGames(svc, *data))})));
    };
    pg->onHints = [data]() {
        std::vector<ui::Hint> h;
        if (!data->systems.empty()) {
            h.push_back(ui::hint(ui::HintKey::UpDown, "hint.move"));
            h.push_back(ui::hint(ui::HintKey::A, "hint.select", false));
        }
        h.push_back(ui::hint(ui::HintKey::Start, "settings.library.artwork.offer.download", true));
        h.push_back(ui::hint(ui::HintKey::B, "hint.skip", true));
        return h;
    };
    pg->onEvent = [svc, visit, data, offerId](ui::WizardEvent e, ui::Wizard &w) {
        if (e == ui::WizardEvent::Start || (e == ui::WizardEvent::Confirm && data->systems.empty())) {
            svc->startArtwork(chosenKeys(*data));
            visit->artworkRan = true;
            visit->artworkCancelled = false;
            w.go(offerId + 1);
        } else if (!data->systems.empty()) {
            data->list.handle(e);
        }
    };
    // B = Skip (Classic): with a scan behind it the Done page, otherwise the visit just ends.
    pg->onBack = [visit, doneId]() { return visit->scanRan ? doneId : ui::kClose; };
    pg->onLeave = [svc, visit](bool cancelled) { if (cancelled && !visit->scanRan) svc->libraryTaskClosed(); };
    flow.add(offerId, std::move(pg));
}

// ---- ArtworkWorking (Scan:420-444): centred like the scan: one status phrase, one short detail line, a bar. The finish
// estimate is not shown; the Done page prints the complete counts.
}  // namespace

// Public: the first-run Jump Start shows the same page for its cover step (src/app/FirstRun.cpp).
void addArtworkWorking(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId id, ui::PageId doneId, const ui::Text &title) {
    std::unique_ptr<ui::BusyPage> pg(new ui::BusyPage(title.empty() ? ui::Text::key("settings.library.artwork.working.title") : title));
    pg->centered = true;
    ui::BusyPage *page = pg.get();
    auto stopping = std::make_shared<bool>(false);
    auto fill = [svc, page, stopping]() {
        const ArtworkProgress ap = svc->artwork();
        // One phrase for the whole job: the service goes through Preparing (cover list of a system) and Working again for
        // every system, so showing the two states made the text flip all the time. What now goes to the detail line, which
        // may change (the page shows a new value once a second at most).
        page->status = ui::Text::key(*stopping ? "settings.library.artwork.working.stopping"
                                               : "settings.library.artwork.working.status");
        page->waitNote = ui::Text::key("settings.library.artwork.working.wait");
        page->fraction = ap.progress;
        if (*stopping) { page->detail = ui::Text(); page->facts = ui::Text(); return; }
        ui::Text facts;
        if (ap.fetched != 0 || ap.prepared != 0)
            facts = ui::Text::data(ui::format("settings.library.artwork.working.counts", {std::to_string(ap.fetched), std::to_string(ap.prepared)}));
        page->setDetail(ap.statusLine.empty() ? ui::Text() : ui::Text::data(ap.statusLine), facts);
    };
    pg->onEnter = [fill, page, stopping](ui::Wizard &) {
        *stopping = false;
        page->cancelable = true;
        page->detail = ui::Text();
        page->facts = ui::Text();
        fill();
    };
    pg->onUpdate = [svc, fill, doneId](float, ui::Wizard &w) {
        const ArtworkProgress ap = svc->artwork();
        if (ap.state == ArtworkProgress::State::Done || ap.state == ArtworkProgress::State::Failed) { w.go(doneId); return true; }
        fill();
        return true;
    };
    // Stopping is harmless: whatever arrived stays, and the next run fills the rest (Scan:113). The service ends the
    // job (Failed with "stopped"); `artworkCancelled` lets the Done page tell it from a failure.
    pg->onCancel = [svc, visit, page, stopping, doneId](ui::Wizard &w) {
        if (artworkRunning(svc->artwork())) visit->artworkCancelled = true;
        svc->cancelArtwork();
        if (artworkRunning(svc->artwork())) {
            *stopping = true;
            page->status = ui::Text::key("settings.library.artwork.working.stopping");
            page->detail = ui::Text();
            page->cancelable = false;
        } else {
            w.go(doneId);
        }
    };
    pg->onLeave = [svc](bool) { if (artworkRunning(svc->artwork())) svc->cancelArtwork(); };
    flow.add(id, std::move(pg));
}

void addArtworkPages(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId offerId, ui::PageId doneId) {
    addOffer(flow, svc, visit, offerId, doneId);
    addArtworkWorking(flow, svc, visit, offerId + 1, doneId);
}

// Settings -> Library -> Prepare box art (and the host runner's artwork.offer): offer, working, done.
void openArtworkOffer(LibraryService &svc, PageStack &pages) {
    enum { kOffer = 0, kDone = 2 };   // the working page is kOffer + 1
    VisitPtr visit(new Visit);
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(kLibraryFlowFrame));
    addArtworkPages(*flow, &svc, visit, kOffer, kDone);
    addDonePage(*flow, &svc, visit, kDone, ui::kNoPage, kOffer);
    flow->wizard().open(kOffer);
    pages.push(std::unique_ptr<SettingsPage>(flow.release()));
}

void registerArtworkStrings(ui::Catalog &c) {
    // Prepare box art (Scan:382-444)
    c.set("settings.library.artwork.offer.title", "Prepare box art");
    c.set("settings.library.artwork.offer.headline", "Which systems should get box art?");
    c.set("settings.library.artwork.offer.headline.all", "Get box art for your games?");
    c.set("settings.library.artwork.offer.line", "Missing covers are downloaded; covers already on your drives are prepared too. No account is needed. Needs an Internet connection for missing covers.");
    c.set("settings.library.artwork.offer.all", "All systems");
    c.set("settings.library.artwork.offer.scope", "Covers checked for {1} games");
    c.set("settings.library.artwork.offer.download", "Download");
    c.set("settings.library.artwork.working.title", "Prepare box art");
    c.set("settings.library.artwork.working.status", "Downloading and preparing covers");
    c.set("settings.library.artwork.working.stopping", "Stopping the download");
    c.set("settings.library.artwork.working.counts", "Downloaded {1}, prepared {2}");
    c.set("settings.library.artwork.working.wait", "Missing covers are downloaded one by one. Without Internet only the covers on your drives are prepared.");
}

}  // namespace library
}  // namespace patsgui
