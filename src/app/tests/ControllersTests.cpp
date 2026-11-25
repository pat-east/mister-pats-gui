// Controllers: the button mapping wizard and Bluetooth pairing (page ids: src/app/settings/controllers/ControllersFlows.cpp).
// Both open over the controllers list (one page under the flow). The raw-input flag is the one the host reads
// (ControllersService::setRawInputWanted): set while a Capture page that reads the pad is current, cleared on leaving it,
// when the flow ends and when the GUI closes.

#include "app/tests/Generic.h"
#include "app/tests/Test.h"

namespace tt {
namespace {

enum { kOpen = 0, kCapture = 1, kSave = 2, kDone = 3 };                                     // button mapping
enum { kIntro = 0, kScan = 1, kFound = 2, kTimeout = 3, kBtFailed = 4, kNone = 5 };         // Bluetooth
using K = ui::HintKey;

struct PadRig : Rig {
    void mapping() { openPage("controllers/wizard"); setBase(1); }
    void bluetooth() { openPage("controllers/bluetooth"); setBase(1); }
    void tap(const char *keys, float seconds = 0.4f) {   // a button press on the pad itself
        set("controllers.script", "none");
        set("controllers.pad.keys", keys);
        wait(seconds);
        set("controllers.pad.keys", "");
        wait(0.3f);
    }
    bool flowOpen() { return stack().size() > 1; }      // the list stays under the flow
};

void expectTone(Rig &r, ui::Tone tone, const char *what) {
    ui::WizardResult *res = r.result();
    CHECK_MSG(res != nullptr, std::string(what) + ": not a Result page (page " + std::to_string(r.id()) + ")");
    if (res) CHECK_MSG(res->tone == tone, std::string(what) + ": wrong tone");
}

// ================================================================================================ button mapping

TEST(mapping_open_and_cancel) {
    PadRig r;
    r.mapping();
    CHECK_EQ(r.id(), int(kOpen));
    CHECK_EQ(r.hintLine(), std::string("A Start | B Close"));
    CHECK(!r.rawInputWanted());
    CHECK(!r.padRawFlag());
    r.keys("b");
    CHECK(!r.flowOpen());                           // back on the list
    CHECK_EQ(r.stack().size(), size_t(1));
    CHECK_EQ(r.pads().mappingsWritten, 0);
    CHECK(!r.padRawFlag());
}

TEST(mapping_raw_input_follows_the_capture_page) {
    PadRig r;
    r.mapping();
    CHECK(!r.padRawFlag());
    r.keys("a");
    CHECK_EQ(r.id(), int(kCapture));
    CHECK(r.rawInputWanted());
    CHECK(r.padRawFlag());                          // the host stops translating the pad
    r.keys("b");                                    // B is not interpreted while capturing
    CHECK_EQ(r.id(), int(kCapture));
    CHECK(r.padRawFlag());
    // GUI closes while capturing: the flag is cleared
    r.guiCloses();
    CHECK(r.stack().empty());
    CHECK(!r.padRawFlag());
}

TEST(mapping_full_run_is_saved) {
    PadRig r;
    r.mapping();
    r.set("controllers.script", "wizard");          // the sample presses every button by itself once capturing starts
    r.keys("a");
    CHECK(r.padRawFlag());
    r.wait(12.0f);
    CHECK_EQ(r.id(), int(kDone));
    CHECK(!r.padRawFlag());                         // cleared on leaving the capture page
    CHECK(!r.rawInputWanted());
    expectTone(r, ui::Tone::Done, "mapping saved");
    CHECK_EQ(r.pads().mappingsWritten, 1);
    r.keys("a");
    CHECK(!r.flowOpen());
    CHECK(!r.padRawFlag());
}

TEST(mapping_hold_start_cancels_from_the_second_step_on) {
    PadRig r;
    r.mapping();
    r.keys("a");
    CHECK_EQ(r.id(), int(kCapture));
    CHECK(r.hintLine().empty());                    // the first step: Start is what it captures (see the known bug in the generic tests)
    r.tap("start");                                 // answers step 0
    CHECK(r.hasHint(K::Start));
    bool hold = false;
    for (const HintView &h : r.hints()) hold = hold || (h.hold && h.key == K::Start && h.word == "to cancel");
    CHECK_MSG(hold, "the second step does not list Hold Start to cancel: " + r.hintLine());
    r.set("controllers.pad.keys", "start");         // hold Start on the pad
    r.wait(2.0f);
    CHECK_EQ(r.id(), int(kCapture));                // not yet: 5 s
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kDone));
    expectTone(r, ui::Tone::Warning, "mapping cancelled");
    CHECK(r.result() && !r.result()->status.empty());
    CHECK_EQ(r.pads().mappingsWritten, 0);          // nothing saved
    CHECK(!r.padRawFlag());
    r.set("controllers.pad.keys", "");
    r.keys("a");
    CHECK(!r.flowOpen());
}

TEST(mapping_start_confirms_the_opening_page_and_then_answers_step_0) {
    // The owner pressed Start to begin (the opening page accepts it) and then "Press the button for START" did nothing: the button
    // that confirmed was still down when the wizard began and stayed "held at the start" for ever.
    PadRig r;
    r.mapping();
    r.set("controllers.script", "none");
    r.set("controllers.pad.keys", "start");         // Start is down ...
    r.wait(0.3f);
    r.keys("start");                                 // ... while it confirms the opening page
    CHECK_EQ(r.id(), int(kCapture));
    r.wait(0.3f);
    r.set("controllers.pad.keys", "");              // released
    r.wait(0.5f);
    ui::WizardPage *p = r.wpage();
    CHECK(p != nullptr);
    const std::string before = p ? p->progress() : "";
    r.tap("start");                                  // a new press answers step 0
    CHECK(p && p->progress() != before);
}

TEST(mapping_short_start_skips_the_step) {
    PadRig r;
    r.mapping();
    r.keys("a");
    r.tap("start");
    ui::WizardPage *p = r.wpage();
    CHECK(p != nullptr);
    const std::string before = p ? p->progress() : "";
    r.tap("start", 0.3f);                           // a short press skips this button
    CHECK_EQ(r.id(), int(kCapture));
    CHECK(p && p->progress() != before);
}

TEST(mapping_hold_start_scripted) {
    PadRig r;
    r.mapping();
    r.set("controllers.script", "wizard-abort");    // Start answers step 0, then is held for 7 s
    r.keys("a");
    r.wait(9.0f);
    CHECK_EQ(r.id(), int(kDone));
    expectTone(r, ui::Tone::Warning, "mapping cancelled by the hold");
    CHECK_EQ(r.pads().mappingsWritten, 0);
    CHECK(!r.padRawFlag());
}

TEST(mapping_pad_is_lost) {
    PadRig r;
    r.mapping();
    r.keys("a");
    r.tap("start");
    r.set("controllers.unplug", "0.5");
    r.wait(1.5f);
    CHECK_EQ(r.id(), int(kDone));
    expectTone(r, ui::Tone::Warning, "pad lost");
    CHECK_EQ(r.pads().mappingsWritten, 0);
    CHECK(!r.padRawFlag());
}

TEST(mapping_save_fails_then_try_again) {
    PadRig r;
    r.mapping();
    r.set("controllers.mapping.fail", "1");
    r.set("controllers.script", "wizard");
    r.keys("a");
    r.wait(12.0f);
    CHECK_EQ(r.id(), int(kDone));
    expectTone(r, ui::Tone::Error, "mapping save failed");
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    CHECK_EQ(r.pads().mappingsWritten, 0);
    CHECK(!r.padRawFlag());                         // Try again only repeats the write: no capture again
    r.set("controllers.mapping.fail", "0");
    r.keys("a");
    r.wait(0.5f);
    CHECK_EQ(r.id(), int(kDone));
    expectTone(r, ui::Tone::Done, "mapping saved after retry");
    CHECK_EQ(r.pads().mappingsWritten, 1);
}

// ================================================================================================ Bluetooth

TEST(bluetooth_intro_and_cancel) {
    PadRig r;
    r.bluetooth();
    CHECK_EQ(r.id(), int(kIntro));
    CHECK_EQ(r.hintLine(), std::string("A Start | B Close"));
    r.keys("b");
    CHECK(!r.flowOpen());
    CHECK(!r.pads().scanning());
}

TEST(bluetooth_scan_and_cancel) {
    PadRig r;
    r.bluetooth();
    r.keys("a");
    CHECK_EQ(r.id(), int(kScan));
    CHECK(r.pads().scanning());
    CHECK(r.cancellableBusy());
    CHECK_EQ(r.hintWord(K::B), std::string("Cancel"));
    CHECK(!r.padRawFlag());                         // a scan does not capture the pad
    r.wait(2.0f);
    r.keys("b");
    CHECK(!r.flowOpen());
    CHECK(!r.pads().scanning());                    // btpair stopped
}

TEST(bluetooth_timeout) {
    PadRig r;
    r.set("controllers.bt.timeout", "3");
    r.bluetooth();
    r.keys("a");
    r.wait(2.5f);
    CHECK_EQ(r.id(), int(kScan));
    r.wait(1.0f);
    CHECK_EQ(r.id(), int(kTimeout));
    expectTone(r, ui::Tone::Warning, "bluetooth timeout");
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    CHECK(!r.pads().scanning());
    r.keys("a");
    CHECK_EQ(r.id(), int(kIntro));                  // Try again: the start page
    r.keys("a");
    CHECK_EQ(r.id(), int(kScan));
    CHECK(r.pads().scanning());
    r.keys("b");
    CHECK(!r.flowOpen());
    CHECK(!r.pads().scanning());
}

TEST(bluetooth_found_and_hand_off_to_the_mapping) {
    PadRig r;
    r.set("controllers.bt.found", "1.5");
    r.bluetooth();
    r.keys("a");
    r.wait(2.5f);
    CHECK_EQ(r.id(), int(kFound));
    CHECK(!r.pads().scanning());
    CHECK_EQ(r.hintWord(K::A), std::string("Continue"));
    CHECK_EQ(r.hintWord(K::B), std::string("Later"));
    r.keys("a");                                    // hands off to the mapping wizard of the new pad
    CHECK_EQ(r.id(), int(kOpen));
    CHECK_EQ(r.stack().size(), size_t(2));          // the list, the mapping flow (the Bluetooth flow is gone)
    CHECK_EQ(r.hintLine(), std::string("A Start | B Close"));
    CHECK(!r.padRawFlag());
    r.keys("a");
    CHECK(r.padRawFlag());
    r.keys("b");                                    // not read while capturing
    CHECK(r.padRawFlag());
    r.guiCloses();
    CHECK(!r.padRawFlag());
    CHECK(r.stack().empty());
}

TEST(bluetooth_found_later) {
    PadRig r;
    r.set("controllers.bt.found", "1");
    r.bluetooth();
    r.keys("a,wait1.5");
    CHECK_EQ(r.id(), int(kFound));
    r.keys("b");
    CHECK(!r.flowOpen());
    CHECK(!r.pads().scanning());
}

TEST(bluetooth_cannot_start) {
    PadRig r;
    r.set("controllers.bt.start", "fail");
    r.bluetooth();
    r.keys("a");
    CHECK_EQ(r.id(), int(kBtFailed));
    expectTone(r, ui::Tone::Error, "btpair did not start");
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.keys("a");
    CHECK_EQ(r.id(), int(kIntro));
    r.set("controllers.bt.start", "ok");
    r.keys("a");
    CHECK_EQ(r.id(), int(kScan));
    r.keys("b");
    CHECK(!r.flowOpen());
}

TEST(bluetooth_no_adapter) {
    PadRig r;
    r.set("controllers.bluetooth", "none");
    r.bluetooth();
    CHECK_EQ(r.id(), int(kNone));
    CHECK_EQ(r.hintWord(K::AB), std::string("Done"));
    r.keys("a");
    CHECK(!r.flowOpen());
}

TEST(bluetooth_gui_closes_while_scanning) {
    PadRig r;
    r.bluetooth();
    r.keys("a,wait0.5");
    CHECK(r.pads().scanning());
    r.guiCloses();
    CHECK(r.stack().empty());
    CHECK(!r.pads().scanning());
}

}  // namespace
}  // namespace tt
