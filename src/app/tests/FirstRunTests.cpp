// The first start: Welcome, Jump Start offer, quick scan, covers, overview (src/app/FirstRun.cpp), hosted by the shell. Scripted key
// sequences against the sample services; the rules are Classic's ScanScreen with firstRun = true (see FirstRun.cpp).

#include "app/tests/Generic.h"
#include "app/tests/Test.h"

namespace tt {
namespace {

// The page ids of the flow (FirstRun.cpp): the contract of these tests.
enum { kWelcome = 0, kOffer = 1, kScan = 2, kCovers = 3, kFailed = 4, kOverview = 5, kAssets = 6 };

using K = ui::HintKey;

bool has(const std::string &text, const char *part) { return text.find(part) != std::string::npos; }

void expectEnded(Rig &r, int completes) {
    CHECK(!r.active());
    CHECK(!patsgui::firstRunActive(*r.shell()));
    CHECK_EQ(r.onboarding().completes, completes);
    CHECK_EQ(r.onboarding().done, completes > 0);
}

// ================================================================================================ the full path

TEST(firstrun_full_path) {
    Rig r;
    r.openFirstRun();
    CHECK_EQ(r.id(), int(kWelcome));
    CHECK_EQ(r.hintLine(), std::string("A Continue | B Skip"));
    CHECK_EQ(r.title(), std::string("Welcome"));
    r.keys("a");
    CHECK_EQ(r.id(), int(kOffer));
    CHECK_EQ(r.hintLine(), std::string("A Start | B Skip"));
    CHECK_EQ(r.lib().jumpStarts, 0);
    r.keys("a");
    CHECK_EQ(r.id(), int(kScan));
    CHECK_EQ(r.lib().jumpStarts, 1);      // scan_.startJumpStart(roots_, 20, 20): one call
    CHECK_EQ(r.lib().scanStarts, 0);      // and not the full scan
    CHECK(r.cancellableBusy());
    CHECK_EQ(r.hintLine(), std::string("B Cancel"));
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kCovers));       // games found: the cover step follows without a question
    CHECK_EQ(r.lib().jumpArtworkStarts, 1);
    CHECK_EQ(r.lib().artworkStarts, 0);
    CHECK_EQ(r.lib().lastArtworkKey, std::string(""));   // all systems
    CHECK(r.cancellableBusy());
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kOverview));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) {
        CHECK(res->tone == ui::Tone::Done);
        CHECK(has(res->changed.str(), "3 systems and 52 games"));
        CHECK_EQ(res->items.size(), size_t(3));   // Classic's "When you're ready" list
        CHECK(!has(res->next.str(), "Some covers"));
    }
    CHECK_EQ(r.hintWord(K::AB), std::string("Done"));
    CHECK_EQ(r.onboarding().completes, 0);   // not before the user ends it
    r.keys("a");
    expectEnded(r, 1);
    CHECK_EQ(r.lib().tasksClosed, 1);        // App::closeScan once
}

// ================================================================================================ skip and B (Classic's rules)

TEST(firstrun_skip_at_welcome) {
    Rig r;
    r.openFirstRun();
    r.keys("b");                              // Welcome: onOnboardingComplete_(), onClose_()
    expectEnded(r, 1);
    CHECK_EQ(r.lib().jumpStarts, 0);
}

TEST(firstrun_skip_at_offer) {
    Rig r;
    r.openFirstRun();
    r.keys("a,b");                            // JumpStart: B skips (not back to Welcome)
    expectEnded(r, 1);
    CHECK_EQ(r.lib().jumpStarts, 0);
}

TEST(firstrun_start_key_is_a) {
    Rig r;
    r.openFirstRun();
    r.keys("start");
    CHECK_EQ(r.id(), int(kOffer));
    r.keys("start");
    CHECK_EQ(r.id(), int(kScan));
    CHECK_EQ(r.lib().jumpStarts, 1);
}

TEST(firstrun_b_on_the_scan_stops_it_and_offers_again) {
    Rig r;
    r.openFirstRun();
    r.keys("a,a,b");                          // scan_.cancel(): the scan ends Failed, the page follows
    CHECK_EQ(r.id(), int(kFailed));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) {
        CHECK(res->tone == ui::Tone::Warning);   // stopped by the user, not a failure
        CHECK_EQ(res->retry, int(kScan));
    }
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    CHECK_EQ(r.hintWord(K::B), std::string("Close"));
    CHECK_EQ(r.onboarding().completes, 0);
    r.keys("a");                              // A: scan_.startJumpStart again, Working
    CHECK_EQ(r.id(), int(kScan));
    CHECK_EQ(r.lib().jumpStarts, 2);
    CHECK_EQ(r.onboarding().completes, 0);
    r.keys("b");
    CHECK_EQ(r.id(), int(kFailed));
    r.keys("b");                              // B: onOnboardingComplete_(), onClose_()
    expectEnded(r, 1);
}

TEST(firstrun_b_on_the_covers_goes_to_the_overview) {
    Rig r;
    r.openFirstRun();
    r.keys("a,a");
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kCovers));
    r.keys("b");                              // scraper_.cancel(): the job ended, the overview follows
    CHECK_EQ(r.id(), int(kOverview));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) {
        CHECK(res->tone == ui::Tone::Done);      // Classic still calls it ready
        CHECK(has(res->next.str(), "Some covers could not be prepared"));
    }
    CHECK_EQ(r.onboarding().completes, 0);
    r.keys("b");                              // B closes a Result (Classic ignored B here)
    expectEnded(r, 1);
}

// ================================================================================================ outcomes

TEST(firstrun_no_games_found) {
    Rig r;
    r.set("library.scan.ends", "nogames");
    r.openFirstRun();
    r.keys("a,a");
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kOverview));         // no cover step without games
    CHECK_EQ(r.lib().jumpArtworkStarts, 0);
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) {
        CHECK(res->tone == ui::Tone::Neutral);
        CHECK(has(res->headline.str(), "No games found"));
        CHECK(has(res->changed.str(), "did not find any games"));
        CHECK_EQ(res->items.size(), size_t(3));
    }
    r.keys("a");
    expectEnded(r, 1);
}

TEST(firstrun_scan_failure_and_retry) {
    Rig r;
    r.set("library.scan.ends", "failed");
    r.openFirstRun();
    r.keys("a,a");
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kFailed));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) {
        CHECK(res->tone == ui::Tone::Error);
        CHECK(has(res->headline.str(), "could not be read"));
        CHECK(!res->details.empty());         // the service text, small, as Details
        CHECK(!res->next.empty());
    }
    r.set("library.scan.ends", "done");
    r.keys("a");                              // Try again
    CHECK_EQ(r.id(), int(kScan));
    CHECK_EQ(r.lib().jumpStarts, 2);
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kCovers));
    CHECK_EQ(r.onboarding().completes, 0);
    r.wait(3.0f);
    r.keys("a");
    expectEnded(r, 1);
}

TEST(firstrun_no_drives) {
    Rig r;
    r.set("library.scan.ends", "nodrives");
    r.openFirstRun();
    r.keys("a,a");
    r.wait(2.0f);
    CHECK_EQ(r.id(), int(kFailed));
    r.keys("b");
    expectEnded(r, 1);
}

TEST(firstrun_covers_fail_but_the_overview_stays_ready) {
    Rig r;
    r.set("library.art.ends", "nomatch");
    r.openFirstRun();
    r.keys("a,a");
    r.wait(6.0f);
    CHECK_EQ(r.id(), int(kOverview));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) {
        CHECK(res->tone == ui::Tone::Done);
        CHECK(has(res->next.str(), "Some covers could not be prepared"));
    }
    r.keys("a");
    expectEnded(r, 1);
}

// ================================================================================================ the GUI closes

// Classic marked the guide started and never complete; it opens again at the next start. Nothing keeps running.
TEST(firstrun_gui_closes_in_the_middle) {
    const char *paths[] = {"", "a", "a,a", "a,a,wait4", "a,a,wait4,wait3"};   // Welcome, offer, scan, covers, overview
    for (const char *path : paths) {
        Rig r;
        r.note(std::string("path ") + path);
        r.openFirstRun();
        r.keys(path);
        CHECK(r.active());
        r.guiCloses();
        CHECK(!r.active());
        CHECK_EQ(r.onboarding().completes, 0);
        CHECK(!r.onboarding().done);
        CHECK_EQ(r.lib().tasksClosed, 0);
        using S = ScanProgress::State;
        const S s = r.lib().scan().state;
        CHECK(s != S::Discovering && s != S::Scanning && s != S::Writing);
        const ArtworkProgress::State a = r.lib().artwork().state;
        CHECK(a != ArtworkProgress::State::Preparing && a != ArtworkProgress::State::Working);
    }
}

// ================================================================================================ the shell underneath

TEST(firstrun_nothing_underneath_reacts) {
    Rig r;
    r.openFirstRun();
    patsgui::State &st = *r.shell();
    CHECK(st.tab == patsgui::Tab::Home);
    r.keys("tabnext,tabprev,xhold,bhold,select,y,x");
    CHECK(st.tab == patsgui::Tab::Home);        // the tabs are locked
    CHECK(patsgui::favoriteKey(st).empty());    // no hold on a game underneath
    CHECK(patsgui::removeKey(st).empty());
    CHECK_EQ(r.id(), int(kWelcome));
    patsgui::SystemValues values;
    const ui::ShellSpec spec = patsgui::shellFor(st, values, patsgui::Assets());
    CHECK(spec.tabsLocked);
    bool shoulders = false;
    for (const ui::Hint &h : spec.help) if (h.key == K::Shoulders) shoulders = true;
    CHECK(!shoulders);                          // "change tab" is not offered
    CHECK(!patsgui::handle(st, patsgui::Event::Back));   // the shell never closes from it
    expectEnded(r, 1);                          // (B was Skip)
    r.keys("tabnext");                          // and when it is over the shell is back
    CHECK(st.tab != patsgui::Tab::Home);
}

// ================================================================================================ every page of the host runner

TEST(firstrun_every_listed_page) {
    for (const std::string &id : patsgui::FirstRun::pageIds()) {
        Rig r;
        r.note("first-run " + id);
        CHECK(r.openFirstRunAt(id));
        checkPage(r);
        r.wait(1.0f);
        checkPage(r);
        const bool left = escapeWithB(r);
        CHECK_MSG(left, "B does not leave the first-start page " + id + " (" + r.hintLine() + ")");
        CHECK_EQ(r.onboarding().completes <= 1, true);
    }
}

}  // namespace
}  // namespace tt

namespace tt {
namespace {

// ================================================================================================ the assets step (font first, then icons)

std::string busyStatus(Rig &r) {
    ui::WizardPage *p = r.wpage();
    return p && p->kind() == ui::WizardPage::Kind::Busy ? static_cast<ui::BusyPage *>(p)->status.str() : std::string();
}

void openAtOffer(Rig &r) {
    CHECK(r.openFirstRunAt("welcome"));
    r.keys("a");
    CHECK_EQ(r.id(), int(kOffer));
}

TEST(firstrun_assets_font_then_icons_then_scan) {
    Rig r;
    r.set("accessibility.font", "missing");   // icons: 3 missing (sample default)
    openAtOffer(r);
    r.keys("a");
    CHECK_EQ(r.id(), int(kAssets));
    CHECK_EQ(r.title(), std::string("Jump Start"));
    CHECK_EQ(busyStatus(r), std::string("Getting the font"));
    CHECK(r.ss.accessibility.fontDownload().running);
    CHECK_EQ(r.calls("icons.start"), 0);   // one job at a time: the icons wait
    CHECK_EQ(r.lib().jumpStarts, 0);          // the scan comes after
    CHECK(r.cancellableBusy());
    CHECK_EQ(r.hintLine(), std::string("B Cancel"));
    for (int i = 0; i < 12 && busyStatus(r) == "Getting the font"; ++i) r.wait(0.5f);   // three files, a second each
    CHECK(r.ss.accessibility.fontInstalled());
    CHECK(r.ss.accessibility.fontEnabled());  // the font is on for the pages that follow
    CHECK_EQ(r.id(), int(kAssets));
    CHECK_EQ(busyStatus(r), std::string("Getting icons"));
    CHECK_EQ(r.calls("icons.start"), 1);
    r.wait(2.0f);
    CHECK_EQ(r.id(), int(kScan));
    CHECK_EQ(r.lib().jumpStarts, 1);
    r.wait(8.0f);
    CHECK_EQ(r.id(), int(kOverview));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) CHECK(!has(res->next.str(), "could not be downloaded"));
}

TEST(firstrun_assets_only_icons_missing) {
    Rig r;
    openAtOffer(r);
    r.keys("a");
    CHECK_EQ(r.id(), int(kAssets));
    CHECK_EQ(busyStatus(r), std::string("Getting icons"));
    r.wait(2.0f);
    CHECK_EQ(r.id(), int(kScan));
}

TEST(firstrun_assets_only_font_missing) {
    Rig r;
    r.set("system.icons", "ready");
    r.set("accessibility.font", "missing");
    openAtOffer(r);
    r.keys("a");
    CHECK_EQ(busyStatus(r), std::string("Getting the font"));
    r.wait(5.0f);
    CHECK_EQ(r.id(), int(kScan));
    CHECK_EQ(r.calls("icons.start"), 0);
}

TEST(firstrun_assets_nothing_missing_shows_no_page) {
    Rig r;
    r.set("system.icons", "ready");
    openAtOffer(r);
    r.keys("a");
    CHECK_EQ(r.id(), int(kScan));             // straight to the scan
    CHECK_EQ(r.calls("icons.start"), 0);      // and no icon toast either
    CHECK_EQ(r.lib().jumpStarts, 1);
}

TEST(firstrun_assets_offline_both_fail_but_jump_start_goes_on) {
    Rig r;
    r.set("accessibility.font", "failing");
    r.set("system.iconsresult", "fail");
    openAtOffer(r);
    r.keys("a");
    r.wait(2.5f);                             // the font fails on its first file
    CHECK_EQ(busyStatus(r), std::string("Getting icons"));
    r.wait(2.0f);
    CHECK_EQ(r.id(), int(kScan));
    CHECK_EQ(r.lib().jumpStarts, 1);
    r.wait(10.0f);
    CHECK_EQ(r.id(), int(kOverview));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) {
        CHECK(res->tone == ui::Tone::Done);
        CHECK(has(res->next.str(), "Icons and the font could not be downloaded. You can get them later in Settings."));
    }
    r.keys("a");
    expectEnded(r, 1);
}

TEST(firstrun_assets_font_ok_icons_offline) {
    Rig r;
    r.set("accessibility.font", "missing");
    r.set("system.iconsresult", "fail");
    openAtOffer(r);
    r.keys("a");
    r.wait(12.0f);
    CHECK_EQ(r.id(), int(kOverview));
    CHECK(r.ss.accessibility.fontInstalled());
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) CHECK(has(res->next.str(), "could not be downloaded"));
}

TEST(firstrun_assets_icons_cannot_start) {
    Rig r;
    r.set("system.icons", "startfails");
    openAtOffer(r);
    r.keys("a");
    CHECK_EQ(r.id(), int(kScan));             // the failure to start does not stop Jump Start
    r.wait(10.0f);
    CHECK_EQ(r.id(), int(kOverview));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) CHECK(has(res->next.str(), "could not be downloaded"));
}

TEST(firstrun_assets_b_skips_the_step) {
    Rig r;
    r.set("accessibility.font", "missing");
    openAtOffer(r);
    r.keys("a");
    CHECK_EQ(r.id(), int(kAssets));
    r.keys("b");                              // skips the rest of the step, icons included
    CHECK_EQ(r.id(), int(kScan));
    CHECK_EQ(r.lib().jumpStarts, 1);
    CHECK_EQ(r.calls("icons.start"), 0);
    CHECK_EQ(r.onboarding().completes, 0);
    r.wait(12.0f);
    CHECK_EQ(r.id(), int(kOverview));
    ui::WizardResult *res = r.result();
    CHECK(res != nullptr);
    if (res) CHECK(!has(res->next.str(), "could not be downloaded"));   // a skip is not a failure
}

TEST(firstrun_assets_gui_closes_during_the_step) {
    for (const char *font : {"missing", "on"}) {
        Rig r;
        r.set("accessibility.font", font);
        openAtOffer(r);
        r.keys("a");
        CHECK_EQ(r.id(), int(kAssets));
        r.guiCloses();
        CHECK(!r.active());
        CHECK_EQ(r.onboarding().completes, 0);
        CHECK(!r.onboarding().done);
        CHECK_EQ(r.lib().jumpStarts, 0);
        CHECK_EQ(r.lib().tasksClosed, 0);
    }
}

}  // namespace
}  // namespace tt
