// The first start (see FirstRun.h). The state machine is Classic's ScanScreen with firstRun = true (src/legacy/ScanScreen.cpp):
//   Welcome (handle, case Welcome)          A -> JumpStart; B -> onOnboardingComplete_(), onClose_()
//   JumpStart                               A -> scan_.startJumpStart(roots_, 20, 20), scanRan_, Working; B -> complete + close
//   Working (jump-start variant)            B -> scan_.cancel(); when the scan ended: Done (not Done state) or, with games,
//                                           scraper_.start(boxart only) + ArtworkWorking, without games Overview (update())
//   ArtworkWorking (jump-start variant)     B -> scraper_.cancel(); when the job ended -> Overview
//   Overview                                A -> complete + close (B does nothing in Classic: here B does the same as A, as on every Result)
//   Done (jump start, scan not Done)        A -> scan_.startJumpStart again, Working; B -> complete + close
// New (owner decision): between the offer's Start and the scan one Busy page gets what the GUI normally downloads by itself, in this
// order: the Space Grotesk font (+ licence; App enables it when it arrived, PatsGuiScreen::syncLook applies it), then the system and
// tab icons. Each part is skipped when nothing is missing; with nothing missing at all no page shows. A failure never stops Jump
// Start (the Overview says so), B skips the rest of the step (Classic's downloads cannot be stopped; App finishes a running one).
// Presentation only is new: centred pages in one Medium frame, the Busy pages and the failure Result of the Library flows.

#include "app/FirstRun.h"

#include <memory>

#include "app/settings/SettingsServices.h"
#include "app/settings/library/FlowShared.h"
#include "ui-framework/kit/Text.h"
#include "ui-framework/kit/WizardParts.h"

namespace patsgui {

struct FirstRun::Data {
    library::VisitPtr visit{new library::Visit};
    bool noGames = false;     // the jump-start scan found no games (Classic: jumpStartNoGames_)
    bool aborting = false;    // the GUI closes: leaving a page is not the user's decision
    bool completed = false;   // setComplete() was called (exactly once per visit)
    enum class Step { None, Font, Icons, Over } step = Step::None;   // the assets step: font first, then icons
    bool assetsFailed = false;                                         // a download failed: the Overview says so
};

namespace {

enum { kWelcome = 0, kOffer = 1, kScan = 2, kCovers = 3, kFailed = 4, kOverview = 5, kAssets = 6 };

constexpr ui::WizardFrame kFrame = ui::WizardFrame::Medium;

using Data = FirstRun::Data;
using DataPtr = std::shared_ptr<FirstRun::Data>;

}  // namespace

namespace {

// The guide ended by the user: Skip, the failed page closed, the overview done. Classic: onOnboardingComplete_() and onClose_()
// (App::closeScan: covers that arrived reach the image cache). A GUI that closes does neither.
void finish(const DataPtr &d, LibraryService *lib, OnboardingService *onb) {
    if (d->aborting) return;
    if (!d->completed) {
        d->completed = true;
        onb->setComplete();
    }
    lib->libraryTaskClosed();
}

ui::Text flowTitle() { return ui::Text::key("firstrun.title"); }

// ---- Welcome (renderWelcome): where things are, in two sentences
void addWelcome(WizardFlowPage &flow, LibraryService *lib, OnboardingService *onb, DataPtr d) {
    ui::Wizard *wz = &flow.wizard();
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("firstrun.welcome.title")));
    page->onDraw = [](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Dot;
        c.headline = ui::Text::key("firstrun.welcome.headline");
        c.lines = {ui::Text::key("firstrun.welcome.p1"), ui::Text::key("firstrun.welcome.p2")};
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.continue", true), ui::hint(ui::HintKey::B, "hint.skip", true)};
    };
    page->onEvent = [](ui::WizardEvent e, ui::Wizard &w) {
        if (e == ui::WizardEvent::Confirm || e == ui::WizardEvent::Start) w.go(kOffer);   // Classic: Start acts like A
    };
    page->onBack = [] { return ui::kClose; };   // B = Skip: the guide is complete
    page->onLeave = [wz, lib, onb, d](bool) { if (!wz->active()) finish(d, lib, onb); };
    flow.add(kWelcome, std::move(page));
}

// ---- The assets step: what Classic's App downloads on its own (App::activateSpaceGrotesk, App::startAssetDownload), through the
// Accessibility and System services. One job at a time, the font first.
struct Assets {
    AccessibilityService *acc = nullptr;
    SystemService *sys = nullptr;
};

bool startFont(const Assets &a, Data &d) {
    if (!a.acc || a.acc->fontInstalled()) return false;      // Classic: the font counts as missing when the files are not there
    a.acc->activateFont();                                    // not installed: this starts the download
    const FontDownloadInfo f = a.acc->fontDownload();
    if (f.running) return true;
    if (f.failed) d.assetsFailed = true;
    return false;
}

bool startIcons(const Assets &a, Data &d) {
    if (!a.sys) return false;
    if (a.sys->icons().missing == 0) return false;            // nothing missing: no page, no toast
    if (a.sys->icons().running) return true;
    const IconStart r = a.sys->startIconDownload();
    if (r.kind == IconStart::Kind::Started || r.kind == IconStart::Kind::AlreadyRunning) return true;
    if (r.kind == IconStart::Kind::Failed) d.assetsFailed = true;
    return false;
}

// Moves to the next part that has something to get (true: its job runs and the page shows it), or past the step (false).
bool nextAssetStep(const Assets &a, Data &d) {
    if (d.step == Data::Step::None) { d.step = Data::Step::Font; if (startFont(a, d)) return true; }
    if (d.step == Data::Step::Font) { d.step = Data::Step::Icons; if (startIcons(a, d)) return true; }
    d.step = Data::Step::Over;
    return false;
}

// The quick scan after the assets step (and from the offer when nothing is missing).
void startScanPage(LibraryService *lib, const DataPtr &d, ui::Wizard &w) {
    lib->startJumpStart();
    d->visit->scanRan = true;
    w.go(kScan);
}

void addAssets(WizardFlowPage &flow, LibraryService *lib, Assets a, DataPtr d) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(flowTitle()));
    page->centered = true;
    ui::BusyPage *raw = page.get();
    auto fill = [raw, d] {
        const bool font = d->step == Data::Step::Font;
        raw->status = ui::Text::key(font ? "firstrun.assets.font" : "firstrun.assets.icons");
        raw->detail = ui::Text::key(font ? "firstrun.assets.font.detail" : "firstrun.assets.icons.detail");
        raw->waitNote = ui::Text::key(font ? "firstrun.assets.font.wait" : "firstrun.assets.icons.wait");
        raw->fraction = -1.0f;
    };
    page->onEnter = [fill](ui::Wizard &) { fill(); };
    // B: leave the step; the scan follows. A running download is not stopped (Classic cannot): App finishes it.
    page->onCancel = [lib, d](ui::Wizard &w) { d->step = Data::Step::Over; startScanPage(lib, d, w); };
    page->onUpdate = [lib, a, d, fill](float dt, ui::Wizard &w) {
        if (a.sys) a.sys->update(dt);
        if (a.acc) a.acc->update(dt);
        if (d->step == Data::Step::Font) {
            const FontDownloadInfo f = a.acc ? a.acc->fontDownload() : FontDownloadInfo();
            if (f.running) return false;
            if (f.failed) d->assetsFailed = true;
            if (nextAssetStep(a, *d)) { fill(); return true; }   // the font is in place (App enabled it): the icons follow
        } else if (d->step == Data::Step::Icons) {
            const IconDownloadInfo i = a.sys ? a.sys->icons() : IconDownloadInfo();
            if (i.running) return false;
            if (i.failed) d->assetsFailed = true;
            d->step = Data::Step::Over;
        }
        startScanPage(lib, d, w);
        return true;
    };
    flow.add(kAssets, std::move(page));
}

// ---- JumpStart (renderJumpStart): what it does, then Start or Skip
void addOffer(WizardFlowPage &flow, LibraryService *lib, OnboardingService *onb, Assets assets, DataPtr d) {
    ui::Wizard *wz = &flow.wizard();
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, flowTitle()));
    page->onDraw = [](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Search;
        c.headline = ui::Text::key("firstrun.offer.headline");
        c.lines = {ui::Text::key("firstrun.offer.p1"), ui::Text::key("firstrun.offer.p2"), ui::Text::key("firstrun.offer.p3")};
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true), ui::hint(ui::HintKey::B, "hint.skip", true)};
    };
    page->onEvent = [lib, assets, d](ui::WizardEvent e, ui::Wizard &w) {
        if (e != ui::WizardEvent::Confirm && e != ui::WizardEvent::Start) return;
        // The assets first (font, icons; no page when nothing is missing), then
        // jumpStartFlow_ = true; scan_.startJumpStart(roots_, 20, 20); scanRan_ = true; page Working
        *d->visit = library::Visit();
        d->noGames = false;
        d->step = Data::Step::None;
        d->assetsFailed = false;
        if (nextAssetStep(assets, *d)) w.go(kAssets);
        else startScanPage(lib, d, w);
    };
    page->onBack = [] { return ui::kClose; };   // B = Skip
    page->onLeave = [wz, lib, onb, d](bool) { if (!wz->active()) finish(d, lib, onb); };
    flow.add(kOffer, std::move(page));
}

// ---- The three rows of Classic's "When you're ready" list (renderOverview)
std::vector<ui::ResultItemSpec> nextSteps() {
    auto row = [](const char *label, const char *where) {
        ui::ResultItemSpec r;
        r.label = ui::Text::key(label);
        r.status = ui::Text::key(where);
        return r;
    };
    return {row("firstrun.next.library", "firstrun.next.library.where"), row("firstrun.next.art", "firstrun.next.art.where"),
            row("firstrun.next.pad", "firstrun.next.pad.where")};
}

// ---- Done, jump-start variant (renderDone): the scan did not finish. A starts it again, B ends the guide.
class FailedPage : public ui::ResultPage {
public:
    FailedPage(LibraryService *lib, DataPtr d) : ui::ResultPage(flowTitle()), lib_(lib), d_(std::move(d)) {}

    void handle(ui::WizardEvent event, ui::Wizard &w) override {
        if (event == ui::WizardEvent::Confirm && result().retry != ui::kNoPage) {
            // scan_.startJumpStart(roots_, 20, 20); scanRan_ = true; reported_ = false; page Working
            *d_->visit = library::Visit();
            d_->noGames = false;
            lib_->startJumpStart();
            d_->visit->scanRan = true;
            w.go(result().retry);
            return;
        }
        ui::ResultPage::handle(event, w);
    }

private:
    LibraryService *lib_;
    DataPtr d_;
};

void addFailed(WizardFlowPage &flow, LibraryService *lib, OnboardingService *onb, DataPtr d) {
    ui::Wizard *wz = &flow.wizard();
    std::unique_ptr<FailedPage> page(new FailedPage(lib, d));
    FailedPage *raw = page.get();
    page->onEnter = [lib, d, raw](ui::Wizard &) {
        const ScanProgress sp = lib->scan();
        ui::WizardResult r;
        if (d->visit->scanCancelled) {   // the user stopped it: nothing failed
            r.tone = ui::Tone::Warning;
            r.status = ui::Text::key("firstrun.failed.stopped.word");
            r.headline = ui::Text::key("firstrun.failed.stopped");
        } else {
            r.tone = ui::Tone::Error;
            r.headline = ui::Text::key("firstrun.failed.headline");
            r.details = ui::Text::data(sp.error);
        }
        r.changed = ui::Text::key("firstrun.failed.changed");   // "No game list was saved."
        r.next = ui::Text::key("firstrun.failed.next");
        r.retry = kScan;
        raw->set(r);
    };
    page->onFinished = [wz, lib, onb, d] { if (!wz->active()) finish(d, lib, onb); };   // B: Close; "Try again" goes to a page
    flow.add(kFailed, std::move(page));
}

// ---- Overview (renderOverview): what was found and where the rest is; the closing page
void addOverview(WizardFlowPage &flow, LibraryService *lib, OnboardingService *onb, DataPtr d) {
    ui::Wizard *wz = &flow.wizard();
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(flowTitle()));
    ui::ResultPage *raw = page.get();
    page->onEnter = [lib, d, raw](ui::Wizard &) {
        ui::WizardResult r;
        r.items = nextSteps();
        if (d->noGames) {
            r.tone = ui::Tone::Neutral;
            r.headline = ui::Text::key("firstrun.overview.none");
            r.changed = ui::Text::key("firstrun.overview.none.changed");
            r.next = ui::Text::key("firstrun.overview.none.next");
        } else {
            const ScanProgress sp = lib->scan();
            r.tone = ui::Tone::Done;
            r.headline = ui::Text::key("firstrun.overview.ready");
            r.changed = ui::Text::data(ui::format("firstrun.overview.ready.changed", {std::to_string(sp.systemsFound), std::to_string(sp.gamesFound)}));
            // renderOverview: the cover job ended Failed (also when stopped): say so, covers can be prepared later
            const bool coversFailed = d->visit->artworkRan && lib->artwork().state == ArtworkProgress::State::Failed;
            r.next = ui::Text::key(coversFailed ? "firstrun.overview.ready.covers" : "firstrun.overview.ready.next");
        }
        if (d->assetsFailed) r.next = ui::Text::data(r.next.str() + " " + ui::Text::key("firstrun.overview.assets").str());
        raw->set(r);
    };
    page->onFinished = [wz, lib, onb, d] { if (!wz->active()) finish(d, lib, onb); };   // A or B: Start using the GUI
    flow.add(kOverview, std::move(page));
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------- FirstRun

FirstRun &FirstRun::operator=(FirstRun &&other) noexcept {
    if (this != &other) {
        close();
        stack_ = std::move(other.stack_);
        data_ = std::move(other.data_);
        library_ = other.library_;
    }
    return *this;
}

bool FirstRun::open(Services &services, int firstPage, const std::string &pageId) {
    close();
    SettingsServices *ss = services.settings;
    LibraryService *lib = ss ? usable(ss->libraryService) : nullptr;
    OnboardingService *onb = ss ? usable(ss->onboardingService) : nullptr;
    if (!lib || !onb) return false;

    DataPtr d(new Data);
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(kFrame));
    addWelcome(*flow, lib, onb, d);
    Assets assets;
    assets.acc = usable(ss->accessibilityService);
    assets.sys = usable(ss->systemService);
    addOffer(*flow, lib, onb, assets, d);
    addAssets(*flow, lib, assets, d);
    // Working: the same page as "Build game list"; after a scan with games the cover step starts (MediaScraper boxart only), without
    // games the overview says so (update(), jumpStartNoGames_). A scan that failed or was stopped goes to the Failed page.
    library::addScanWorking(*flow, lib, d->visit, kScan, kOverview, kFailed, flowTitle(), [lib, d](const ScanProgress &sp) -> ui::PageId {
        d->noGames = sp.gamesFound == 0;
        if (d->noGames) return kOverview;
        lib->startJumpStartArtwork();
        d->visit->artworkRan = true;
        d->visit->artworkCancelled = false;
        return kCovers;
    });
    library::addArtworkWorking(*flow, lib, d->visit, kCovers, kOverview, flowTitle());
    addFailed(*flow, lib, onb, d);
    addOverview(*flow, lib, onb, d);

    stack_.setNotice(services.notice);
    data_ = d;
    library_ = lib;
    if (pageId == "working") { lib->startJumpStart(); d->visit->scanRan = true; }
    if (pageId == "covers") { lib->startJumpStartArtwork(); d->visit->scanRan = true; d->visit->artworkRan = true; }
    if (pageId == "stopped") { d->visit->scanRan = true; d->visit->scanCancelled = true; }
    if (pageId == "failed") d->visit->scanRan = true;
    if (pageId == "overview.nogames") { d->noGames = true; d->visit->scanRan = true; }
    if (pageId == "overview.nocovers") { d->visit->scanRan = true; d->visit->artworkRan = true; }
    if (pageId == "overview") d->visit->scanRan = true;
    if (pageId == "overview.noassets") { d->visit->scanRan = true; d->assetsFailed = true; }
    if (pageId == "assets") { d->step = Data::Step::Font; startFont(assets, *d); }
    if (pageId == "assets.icons") { d->step = Data::Step::Icons; startIcons(assets, *d); }
    WizardFlowPage::launch(stack_, std::move(flow), firstPage);
    return true;
}

bool FirstRun::start(Services &services) { return open(services, kWelcome, std::string()); }

std::vector<std::string> FirstRun::pageIds() {
    return {"welcome", "offer", "working", "covers", "failed", "stopped", "overview", "overview.nogames", "overview.nocovers", "overview.noassets", "assets", "assets.icons"};
}

bool FirstRun::startAt(Services &services, const std::string &id) {
    int first = -1;
    if (id == "welcome") first = kWelcome;
    else if (id == "offer") first = kOffer;
    else if (id == "working") first = kScan;
    else if (id == "covers") first = kCovers;
    else if (id == "failed" || id == "stopped") first = kFailed;
    else if (id == "overview" || id == "overview.nogames" || id == "overview.nocovers" || id == "overview.noassets") first = kOverview;
    else if (id == "assets" || id == "assets.icons") first = kAssets;
    return first >= 0 && open(services, first, id);
}

bool FirstRun::update(float deltaSeconds) {
    if (!active()) return false;
    // The scan and the box-art job are stepped by the host's frame loop; the service reads their state and reloads the library
    // when a scan ended well (before this page looks, as in ScanScreen::update()).
    if (library_) library_->update(deltaSeconds);
    return stack_.update(deltaSeconds);
}

bool FirstRun::handle(Event event) {
    if (!active()) return false;
    stack_.handle(event);
    return true;
}

void FirstRun::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) {
    if (active()) stack_.draw(p, t, content);
}

void FirstRun::close() {
    if (!active()) return;
    if (data_) data_->aborting = true;
    stack_.clear();   // onClose of the flow: leave(true) on the current page; a running scan or cover job is stopped
}

// ---------------------------------------------------------------------------------------------------------------- strings

void registerFirstRunStrings(ui::Catalog &c) {
    c.set("firstrun.title", "Jump Start");
    // Welcome (renderWelcome)
    c.set("firstrun.welcome.title", "Welcome");
    c.set("firstrun.welcome.headline", "Welcome to MiSTer Pat's GUI");
    c.set("firstrun.welcome.p1", "The tabs at the top are for your games. Settings is where you set everything up.");
    c.set("firstrun.welcome.p2", "The hints at the bottom of the screen show what each button does.");
    // Jump Start offer (renderJumpStart)
    c.set("firstrun.offer.headline", "A quick first look at your games?");
    c.set("firstrun.offer.p1", "It reads up to 20 games per system, then looks for matching box art.");
    c.set("firstrun.offer.p2", "Box art needs an Internet connection. Your full library is not read.");
    c.set("firstrun.offer.p3", "Skip it to read the full library later in Settings \xE2\x86\x92 Library.");
    // The assets step before the scan: font first, then icons
    c.set("firstrun.assets.font", "Getting the font");
    c.set("firstrun.assets.font.detail", "Space Grotesk and its licence");
    c.set("firstrun.assets.icons", "Getting icons");
    c.set("firstrun.assets.icons.detail", "Console and navigation icons");
    c.set("firstrun.assets.font.wait", "The font is downloaded from the Internet. A slow connection takes a little longer. Nothing is stuck.");
    c.set("firstrun.assets.icons.wait", "The icons are downloaded one by one. A slow connection takes a little longer. Nothing is stuck.");
    c.set("firstrun.overview.assets", "Icons and the font could not be downloaded. You can get them later in Settings.");
    // Failed or stopped scan (renderDone, jump-start branch)
    c.set("firstrun.failed.headline", "The library could not be read");
    c.set("firstrun.failed.stopped.word", "Stopped");
    c.set("firstrun.failed.stopped", "Jump Start stopped");
    c.set("firstrun.failed.changed", "No game list was saved.");
    c.set("firstrun.failed.next", "Check your storage, then try again. Or close this and read the library later in Settings.");
    // Overview (renderOverview)
    c.set("firstrun.overview.ready", "Your Jump Start is ready");
    c.set("firstrun.overview.ready.changed", "{1} systems and {2} games are ready. You can start playing now.");
    c.set("firstrun.overview.ready.next", "Box art was added where it was found. Settings has the rest, whenever you are ready.");
    c.set("firstrun.overview.ready.covers", "Some covers could not be prepared. You can try again later in Settings.");
    c.set("firstrun.overview.none", "No games found yet");
    c.set("firstrun.overview.none.changed", "The scan did not find any games on the connected storage.");
    c.set("firstrun.overview.none.next", "You can add your own games and read the full library later.");
    c.set("firstrun.next.library", "Read your full library");
    c.set("firstrun.next.library.where", "Settings \xE2\x86\x92 Library");
    c.set("firstrun.next.art", "Prepare box art for the full library");
    c.set("firstrun.next.art.where", "Settings \xE2\x86\x92 Library");
    c.set("firstrun.next.pad", "Set up your controller");
    c.set("firstrun.next.pad.where", "Settings \xE2\x86\x92 Controllers");
}

}  // namespace patsgui
