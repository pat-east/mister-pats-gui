// Build game list, Prepare box art, Core and MRA updates: scripted key sequences against the sample Library service.

#include "app/tests/Generic.h"
#include "app/tests/Test.h"

namespace tt {
namespace {

// The page ids of the flows (src/app/settings/library/ScanFlow.cpp, ArtworkFlow.cpp, CoreFlow.cpp).
enum { kStart = 0, kDrives = 1, kWork = 2, kOffer = 3, kArtWork = 4, kScanDone = 5 };            // Build game list
enum { kArtOffer = 0, kArtWorking = 1, kArtDone = 2 };                                          // Prepare box art on its own
enum { kIntro = 0, kChecking = 1, kList = 2, kReview = 3, kInstalling = 4, kResult = 5, kFailed = 6 };   // Core and MRA updates

using K = ui::HintKey;

void expectResult(Rig &r, ui::Tone tone, const char *what) {
    ui::WizardResult *res = r.result();
    CHECK_MSG(res != nullptr, std::string(what) + ": not on a Result page, page id " + std::to_string(r.id()));
    if (res) CHECK_MSG(res->tone == tone, std::string(what) + ": the Result has the wrong tone");
}

// ================================================================================================ Build game list

TEST(scan_happy_path) {
    Rig r;
    r.open("library", "library.build");
    CHECK_EQ(r.id(), int(kStart));
    CHECK_EQ(r.hintLine(), std::string("A Start | B Close"));
    CHECK_EQ(r.lib().scanStarts, 0);
    r.keys("a");
    CHECK_EQ(r.id(), int(kDrives));
    CHECK(r.hasHint(K::Start));                     // two drives, both checked: Start is offered
    r.keys("start");
    CHECK_EQ(r.id(), int(kWork));
    CHECK_EQ(r.lib().scanStarts, 1);
    CHECK_EQ(r.lib().lastScanRoots.size(), size_t(2));
    CHECK(r.cancellableBusy());
    CHECK_EQ(r.hintWord(K::B), std::string("Cancel"));
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kOffer));                  // the scan finished: the box-art offer
    CHECK_EQ(r.lib().tasksClosed, 0);               // the visit is not over yet
    r.keys("b");                                    // Skip
    CHECK_EQ(r.id(), int(kScanDone));
    expectResult(r, ui::Tone::Done, "scan done");
    CHECK_EQ(r.hintWord(K::AB), std::string("Done"));
    r.keys("a");
    CHECK(!r.active());
    CHECK_EQ(r.lib().tasksClosed, 1);
    CHECK_EQ(r.lib().artworkStarts, 0);
}

TEST(scan_with_box_art_to_the_end) {
    Rig r;
    r.open("library", "library.build");
    r.keys("a,start");
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kOffer));
    r.keys("start");                                // all systems
    CHECK_EQ(r.id(), int(kArtWork));
    CHECK_EQ(r.lib().artworkStarts, 1);
    CHECK_EQ(r.lib().lastArtworkKey, std::string(""));
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kScanDone));
    expectResult(r, ui::Tone::Done, "scan + art done");
    r.keys("b");                                    // B closes a Result as well
    CHECK(!r.active());
    CHECK_EQ(r.lib().tasksClosed, 1);
}

TEST(scan_drives_toggling) {
    Rig r;
    r.open("library", "library.build");
    r.keys("a");
    // uncheck the first drive: only /media/usb0 is searched
    r.keys("a,start");
    CHECK_EQ(r.id(), int(kWork));
    CHECK_EQ(r.lib().lastScanRoots.size(), size_t(1));
    CHECK_EQ(r.lib().lastScanRoots[0], std::string("/media/usb0"));
}

TEST(scan_all_drives_unchecked_blocks_start) {
    Rig r;
    r.open("library", "library.build");
    r.keys("a");
    r.keys("a,down,a");                             // both unchecked
    CHECK(!r.hasHint(K::Start));                    // the way on is not offered
    r.keys("start");
    CHECK_EQ(r.id(), int(kDrives));                 // and Start does nothing
    CHECK_EQ(r.lib().scanStarts, 0);
    r.keys("a");                                    // check the second again
    CHECK(r.hasHint(K::Start));
    r.keys("start");
    CHECK_EQ(r.id(), int(kWork));
    CHECK_EQ(r.lib().scanStarts, 1);
    CHECK_EQ(r.lib().lastScanRoots.size(), size_t(1));
    CHECK_EQ(r.lib().lastScanRoots[0], std::string("/media/usb0"));
}

TEST(scan_no_drives) {
    Rig r;
    r.set("library.roots", "none");
    r.open("library", "library.build");
    r.keys("a");
    CHECK_EQ(r.id(), int(kDrives));
    CHECK(!r.hasHint(K::Start));
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.keys("start");
    CHECK_EQ(r.id(), int(kDrives));
    CHECK_EQ(r.lib().scanStarts, 0);
    r.set("library.roots", "two");
    r.keys("a");                                    // Try again: the drives are read anew
    CHECK(r.hasHint(K::Start));
    r.keys("start");
    CHECK_EQ(r.id(), int(kWork));
    CHECK_EQ(r.lib().scanStarts, 1);
}

TEST(scan_back_at_every_step) {
    {   // Start: B closes the visit (Classic: "Do this later")
        Rig r;
        r.open("library", "library.build");
        r.keys("b");
        CHECK(!r.active());
        CHECK_EQ(r.lib().tasksClosed, 1);
        CHECK_EQ(r.lib().scanStarts, 0);
    }
    {   // Drives: B back to Start, then B closes; nothing was started
        Rig r;
        r.open("library", "library.build");
        r.keys("a,b");
        CHECK_EQ(r.id(), int(kStart));
        CHECK_EQ(r.lib().tasksClosed, 0);
        r.keys("b");
        KNOWN_BUG(!r.active(), "Wizard history: Drives -B-> Start (an explicit target) leaves Start in the history, so B on Start goes 'back' to Start itself "
                               "(re-enters it) and the flow needs a second B to close; the hint says Back, not Close");
        if (r.active()) r.keys("b");
        CHECK(!r.active());
        CHECK_EQ(r.lib().tasksClosed, 1);
        CHECK_EQ(r.lib().scanStarts, 0);
    }
    {   // Working: B stops the scan; the result says Stopped (a Warning, not a failure) and A ends the visit
        Rig r;
        r.open("library", "library.build");
        r.keys("a,start,wait0.5,b");
        CHECK_EQ(r.id(), int(kScanDone));
        expectResult(r, ui::Tone::Warning, "stopped scan");
        CHECK(r.result() && !r.result()->status.empty());   // its own status word ("Stopped")
        CHECK_EQ(r.hintWord(K::AB), std::string("Done"));
        CHECK_EQ(r.lib().scan().state, ScanProgress::State::Failed);   // the service stopped it
        r.keys("a");
        CHECK(!r.active());
        CHECK_EQ(r.lib().tasksClosed, 1);
    }
    {   // Offer: B is Skip and shows what the scan did
        Rig r;
        r.open("library", "library.build");
        r.keys("a,start,wait4");
        CHECK_EQ(r.id(), int(kOffer));
        CHECK_EQ(r.hintWord(K::B), std::string("Skip"));
        r.keys("b");
        CHECK_EQ(r.id(), int(kScanDone));
        expectResult(r, ui::Tone::Done, "skip");
    }
    {   // Box-art working: B stops it; the finished scan stays a success
        Rig r;
        r.open("library", "library.build");
        r.keys("a,start,wait4,start,wait0.5,b");
        CHECK_EQ(r.id(), int(kScanDone));
        expectResult(r, ui::Tone::Done, "scan ok, art stopped");
        r.keys("a");
        CHECK(!r.active());
        CHECK_EQ(r.lib().tasksClosed, 1);
    }
}

TEST(scan_failed_and_try_again) {
    Rig r;
    r.set("library.scan.ends", "failed");
    r.open("library", "library.build");
    r.keys("a,start");
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kScanDone));
    expectResult(r, ui::Tone::Error, "scan failed");
    CHECK(r.result() && !r.result()->details.empty());     // the service text only as Details
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    CHECK_EQ(r.lib().tasksClosed, 0);
    r.set("library.scan.ends", "done");
    r.keys("a");                                           // Try again goes back to the drives
    CHECK_EQ(r.id(), int(kDrives));
    CHECK_EQ(r.lib().tasksClosed, 0);                      // the visit goes on
    r.keys("start");
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kOffer));
    CHECK_EQ(r.lib().scanStarts, 2);
    r.keys("b,a");
    CHECK(!r.active());
    CHECK_EQ(r.lib().tasksClosed, 1);
}

TEST(scan_no_drives_found_by_the_scan) {
    Rig r;
    r.set("library.scan.ends", "nodrives");
    r.open("library", "library.build");
    r.keys("a,start,wait1.5");
    CHECK_EQ(r.id(), int(kScanDone));
    expectResult(r, ui::Tone::Error, "nodrives");
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.lib().tasksClosed, 1);
}

TEST(scan_gui_closes_while_working) {
    Rig r;
    r.open("library", "library.build");
    r.keys("a,start,wait0.5");
    r.guiCloses();
    CHECK(r.stack().empty());
    CHECK_MSG(r.lib().scan().state != ScanProgress::State::Discovering && r.lib().scan().state != ScanProgress::State::Scanning,
              "the scan still runs after the GUI closed");
    CHECK(r.lib().tasksClosed <= 1);
}

TEST(scan_task_closed_once_on_every_end) {
    // every way the visit can end: libraryTaskClosed() exactly once
    const char *paths[] = {"b", "a,b,b,b", "a,start,wait0.5,b,a", "a,start,wait4,b,a", "a,start,wait4,b,b", "a,start,wait4,start,wait3,a",
                           "a,start,wait4,start,wait0.5,b,b"};
    for (const char *p : paths) {
        Rig r;
        r.open("library", "library.build");
        r.keys(p);
        CHECK_MSG(!r.active(), std::string("the visit did not end: ") + p);
        CHECK_MSG(r.lib().tasksClosed == 1, std::string("libraryTaskClosed called ") + std::to_string(r.lib().tasksClosed) + " times for " + p);
    }
}

// ================================================================================================ Prepare box art

TEST(artwork_all_systems_by_default) {
    Rig r;
    r.open("library", "library.boxart");
    CHECK_EQ(r.id(), int(kArtOffer));
    CHECK(r.hasHint(K::Start));
    CHECK_EQ(r.hintWord(K::B), std::string("Skip"));
    r.keys("start");
    CHECK_EQ(r.id(), int(kArtWorking));
    CHECK_EQ(r.lib().artworkStarts, 1);
    CHECK_EQ(r.lib().lastArtworkKey, std::string(""));       // empty = all systems
}

TEST(artwork_selection_rules) {
    struct Case { const char *keys; const char *expect; const char *why; };
    const Case cases[] = {
        {"down,a", "NES", "choosing a system clears All systems"},
        {"down,a,down,a", "NES,SNES", "two systems"},
        {"down,a,down,a,down,a", "NES,SNES,MegaDrive", "three systems"},
        {"down,a,a", "", "clearing the last system chooses All systems again"},
        {"down,a,down,a,a", "NES", "clearing one of two keeps the other"},
        {"down,a,down,a,up,up,a", "", "choosing All systems clears the systems"},
        {"down,a,down,a,up,up,a,down,a", "NES", "a system after All systems again: only that one"},
        {"a", "", "A on All systems when it is checked: it stays checked (nothing else is)"},
    };
    for (const Case &c : cases) {
        Rig r;
        r.open("library", "library.boxart");
        r.note(c.why);
        r.keys(c.keys);
        r.keys("start");
        CHECK_EQ(r.id(), int(kArtWorking));
        CHECK_MSG(r.lib().lastArtworkKey == c.expect, std::string(c.why) + ": started with \"" + r.lib().lastArtworkKey + "\", want \"" + c.expect + "\"");
    }
}

TEST(artwork_no_systems_means_one_question) {
    Rig r;
    r.set("library.systems", "none");
    r.open("library", "library.boxart");
    CHECK(!r.hasHint(K::A) || r.hintWord(K::A) != "Select");   // nothing to choose
    r.keys("start");
    CHECK_EQ(r.id(), int(kArtWorking));
    CHECK_EQ(r.lib().lastArtworkKey, std::string(""));
}

TEST(artwork_skip_closes_the_visit) {
    Rig r;
    r.open("library", "library.boxart");
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.lib().artworkStarts, 0);
    CHECK_EQ(r.lib().tasksClosed, 1);
}

TEST(artwork_done_outcomes) {
    {   // finished
        Rig r;
        r.open("library", "library.boxart");
        r.keys("start,wait3");
        CHECK_EQ(r.id(), int(kArtDone));
        expectResult(r, ui::Tone::Done, "art done");
        CHECK(r.result() && !r.result()->changed.empty());
        r.keys("a");
        CHECK(!r.active());
        CHECK_EQ(r.lib().tasksClosed, 1);
    }
    {   // stopped by B: a Warning with its own word, nothing failed
        Rig r;
        r.open("library", "library.boxart");
        r.keys("start,wait1.2,b");
        CHECK_EQ(r.id(), int(kArtDone));
        expectResult(r, ui::Tone::Warning, "art stopped");
        CHECK(r.result() && !r.result()->status.empty());
        r.keys("b");
        CHECK(!r.active());
        CHECK_EQ(r.lib().tasksClosed, 1);
    }
    {   // no cover matched: an Error with Try again, which goes back to the offer
        Rig r;
        r.set("library.art.ends", "nomatch");
        r.open("library", "library.boxart");
        r.keys("start,wait0.3");
        CHECK_EQ(r.id(), int(kArtDone));
        expectResult(r, ui::Tone::Error, "art nomatch");
        CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
        CHECK_EQ(r.lib().tasksClosed, 0);
        r.keys("a");
        CHECK_EQ(r.id(), int(kArtOffer));
        r.set("library.art.ends", "done");
        r.keys("start,wait3");
        CHECK_EQ(r.id(), int(kArtDone));
        expectResult(r, ui::Tone::Done, "art retried");
        r.keys("a");
        CHECK(!r.active());
        CHECK_EQ(r.lib().tasksClosed, 1);
        CHECK_EQ(r.lib().artworkStarts, 2);
    }
}

TEST(artwork_gui_closes_while_working) {
    Rig r;
    r.open("library", "library.boxart");
    r.keys("start,wait0.5");
    r.guiCloses();
    CHECK(r.stack().empty());
    CHECK_MSG(r.lib().artwork().state != ArtworkProgress::State::Working && r.lib().artwork().state != ArtworkProgress::State::Preparing,
              "the box-art job still runs after the GUI closed");
    KNOWN_BUG(r.lib().tasksClosed == 1,
              "the GUI closed during the box-art job: libraryTaskClosed() was not called, so covers fetched in this visit reach the image cache only at the next reload (low; Classic's closeScan runs on B/A only too: confirm it is wanted)");
}

// ================================================================================================ Core and MRA updates

TEST(cores_offline) {
    Rig r;
    r.set("library.online", "0");
    r.open("library", "library.coreupdates");
    CHECK_EQ(r.id(), int(kIntro));
    CHECK(!r.hasHint(K::A));                        // greyed out while offline: no A, the reason line says why
    r.keys("a,start");
    CHECK_EQ(r.id(), int(kIntro));
    CHECK_EQ(r.lib().coreChecks, 0);
    r.set("library.online", "1");
    r.wait(2.5f);                                   // the page looks again every 2 s
    CHECK(r.hasHint(K::A));
    r.keys("a");
    CHECK_EQ(r.id(), int(kChecking));
    CHECK_EQ(r.lib().coreChecks, 1);
}

TEST(cores_check_list_review_install) {
    Rig r;
    r.open("library", "library.coreupdates");
    CHECK_EQ(r.id(), int(kIntro));
    r.keys("a");
    CHECK_EQ(r.id(), int(kChecking));
    CHECK_EQ(r.hintWord(K::B), std::string("Cancel"));
    r.wait(3.5f);
    CHECK_EQ(r.id(), int(kList));
    CHECK(r.hasHint(K::Start));                     // some files are preselected (missing or changed)
    r.keys("start");
    CHECK_EQ(r.id(), int(kReview));
    r.keys("b");
    CHECK_EQ(r.id(), int(kList));                   // Review: B goes back to the list
    r.keys("start,a");
    CHECK_EQ(r.id(), int(kInstalling));
    CHECK_EQ(r.lib().coreInstalls, 1);
    CHECK(!r.hasHint(K::B));                        // no cancel, no hint
    r.keys("b");                                    // swallowed
    CHECK_EQ(r.id(), int(kInstalling));
    r.wait(30.0f);
    CHECK_EQ(r.id(), int(kResult));
    expectResult(r, ui::Tone::Done, "install ok");
    r.keys("a");
    CHECK(!r.active());
}

TEST(cores_filters_and_selection_survive_review) {
    Rig r;
    r.open("library", "library.coreupdates");
    r.keys("a,wait3.5");
    CHECK_EQ(r.id(), int(kList));
    const auto selected = [&r]() {
        int n = 0;
        for (const CoreMraFile &f : r.lib().cores().entries) n += f.selected ? 1 : 0;
        return n;
    };
    const int before = selected();
    CHECK(before > 0);
    r.keys("left,right,right,left");                // the filter wraps around (needs update / all / up to date / changed)
    CHECK_EQ(r.id(), int(kList));
    r.keys("a");                                    // toggles the file under the cursor (a file that needs an update)
    const int after = selected();
    CHECK(after != before);
    r.keys("start,b");                              // Review and back
    CHECK_EQ(r.id(), int(kList));
    CHECK_EQ(selected(), after);                    // Review back keeps the selection
    r.keys("x");                                    // X selects every missing or outdated file again
    CHECK_EQ(selected(), before);
    CHECK_EQ(r.lib().coreInstalls, 0);
}

TEST(cores_nothing_selected_blocks_review) {
    Rig r;
    r.set("library.cores.count", "6");
    r.open("library", "library.coreupdates");
    r.keys("a,wait3.5");
    CHECK_EQ(r.id(), int(kList));
    // deselect everything that is selected: with the filter on "needs update" the rows are exactly those
    for (int i = 0; i < 8; ++i) r.keys("a,down");
    int n = 0;
    for (const CoreMraFile &f : r.lib().cores().entries) n += f.selected ? 1 : 0;
    if (n == 0) {
        CHECK(!r.hasHint(K::Start));
        r.keys("start");
        CHECK_EQ(r.id(), int(kList));
    }
}

TEST(cores_check_again_with_y) {
    Rig r;
    r.open("library", "library.coreupdates");
    r.keys("a,wait3.5");
    CHECK_EQ(r.id(), int(kList));
    CHECK_EQ(r.lib().coreFreshChecks, 0);
    r.keys("y");
    CHECK_EQ(r.id(), int(kChecking));
    CHECK_EQ(r.lib().coreFreshChecks, 1);
    r.wait(3.5f);
    CHECK_EQ(r.id(), int(kList));
}

TEST(cores_check_fails) {
    Rig r;
    r.set("library.cores.ends", "failed");
    r.open("library", "library.coreupdates");
    r.keys("a,wait3");
    CHECK_EQ(r.id(), int(kFailed));
    expectResult(r, ui::Tone::Error, "check failed");
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.keys("a");                                    // back to the intro (Classic)
    CHECK_EQ(r.id(), int(kIntro));
    r.set("library.cores.ends", "ready");
    r.keys("a,wait3.5");
    CHECK_EQ(r.id(), int(kList));
}

TEST(cores_install_fails) {
    {   // one file of many fails: a Warning that lists what was installed and what was not; Try again checks anew
        Rig r;
        r.set("library.install.ends", "failed");
        r.open("library", "library.coreupdates");
        r.keys("a,wait3.5,start,a,wait30");
        CHECK_EQ(r.id(), int(kResult));
        expectResult(r, ui::Tone::Warning, "install partly failed");
        CHECK(r.result() && r.result()->items.size() == 2);
        CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
        r.keys("a");
        CHECK_EQ(r.id(), int(kIntro));
    }
    {   // the only file fails: an Error
        Rig r;
        r.set("library.cores.count", "2");
        r.set("library.install.ends", "failed");
        r.open("library", "library.coreupdates");
        r.keys("a,wait3.5,start,a,wait10");
        CHECK_EQ(r.id(), int(kResult));
        expectResult(r, ui::Tone::Error, "install failed");
        r.keys("b");
        CHECK(!r.active());
    }
}

TEST(cores_back_at_every_step) {
    {   // intro: B closes
        Rig r;
        r.open("library", "library.coreupdates");
        r.keys("b");
        CHECK(!r.active());
    }
    {   // checking: B stops the check and closes the flow
        Rig r;
        r.open("library", "library.coreupdates");
        r.keys("a,wait0.5,b");
        CHECK(!r.active());
        CHECK(r.lib().cores().phase != CoreMraProgress::Phase::Checking);
    }
    {   // list: B closes the whole flow
        Rig r;
        r.open("library", "library.coreupdates");
        r.keys("a,wait3.5,b");
        CHECK(!r.active());
    }
    {   // review: B back to the list, B again closes
        Rig r;
        r.open("library", "library.coreupdates");
        r.keys("a,wait3.5,start,b");
        CHECK_EQ(r.id(), int(kList));
        r.keys("b");
        CHECK(!r.active());
    }
    {   // result: A and B both end it
        Rig r;
        r.open("library", "library.coreupdates");
        r.keys("a,wait3.5,start,a,wait30,b");
        CHECK(!r.active());
    }
}

}  // namespace
}  // namespace tt
