// Wi-Fi: the 15 paths of the first throwaway driver, plus the rollback invariant (see Flows.cpp: wifiRollback) on every one.
// Page ids (src/app/settings/system/WifiFlow.cpp).

#include "app/tests/Generic.h"
#include "app/tests/Test.h"

namespace tt {
namespace {

enum { kScan = 0, kNetworks = 1, kSsid = 2, kPassword = 3, kPrompt = 4, kTesting = 5, kReview = 6, kCountry = 7, kFailed = 8, kSaveFailed = 9,
       kScanFailed = 10, kDone = 11 };

using K = ui::HintKey;

struct WifiRig : Rig {
    WifiRig() {
        watch = specNamed("system.wifi").watch;
    }
    void start() { open("system", "system.wifi"); }
    // At the end of a test: the GUI closes and nothing may be left over.
    void finish() {
        mem["guiClosing"] = 1;
        guiCloses();
        specNamed("system.wifi").atEnd(*this);
    }
    // The on-screen keyboard starts on "!": A types it n times.
    void typeBang(int n) { for (int i = 0; i < n; ++i) press(Event::Confirm); }
    // Scan, pick the n-th network (0 = Example-Home secured, 1 = Example-Guest open).
    void pick(int row) {
        start();
        wait(3.0f);
        for (int i = 0; i < row; ++i) press(Event::Down);
        press(Event::Confirm);
    }
};

TEST(wifi_success_secured) {
    WifiRig r;
    r.start();
    CHECK_EQ(r.id(), int(kScan));
    CHECK(r.cancellableBusy());
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kNetworks));
    r.keys("a");
    CHECK_EQ(r.id(), int(kPassword));
    r.typeBang(8);
    r.keys("start");
    CHECK_EQ(r.id(), int(kPrompt));
    r.keys("a");
    CHECK_EQ(r.id(), int(kTesting));
    CHECK_EQ(r.calls("wifi.begin Example-Home secured pw=8"), 1);
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kReview));
    CHECK_EQ(r.hintWord(K::A), std::string("Save"));
    r.keys("a");
    CHECK_EQ(r.id(), int(kDone));
    CHECK_EQ(r.calls("wifi.save"), 1);
    CHECK_EQ(r.calls("wifi.rollback"), 0);
    r.keys("a");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 0);
    r.finish();
}

TEST(wifi_wrong_password_or_failed_test) {
    WifiRig r;
    r.sys().testMode = SampleSystemService::TestMode::Fail;
    r.pick(0);
    r.typeBang(8);
    r.keys("start,a");
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kFailed));
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.keys("a");
    CHECK_EQ(r.id(), int(kPassword));                  // Try again: the password again
    r.typeBang(3);
    r.keys("start");
    CHECK_EQ(r.id(), int(kPassword));                  // a short password stays
    CHECK_EQ(r.stack().size(), size_t(1));
    r.keys("b");
    CHECK_EQ(r.id(), int(kNetworks));
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 0);             // the service rolled back by itself
    r.finish();
}

TEST(wifi_open_network_b_at_prompt_closes) {
    WifiRig r;
    r.pick(1);
    CHECK_EQ(r.id(), int(kPrompt));
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 0);             // no test was running
    r.finish();
}

TEST(wifi_cancel_while_testing_rolls_back_once) {
    WifiRig r;
    r.pick(1);
    r.keys("a");
    CHECK_EQ(r.id(), int(kTesting));
    r.wait(0.5f);
    CHECK(r.cancellableBusy());
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 1);
    r.finish();
}

TEST(wifi_b_at_review_restores) {
    WifiRig r;
    r.pick(1);
    r.keys("a");
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kReview));
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 1);
    CHECK_EQ(r.calls("wifi.save"), 0);
    r.finish();
}

TEST(wifi_hidden_network) {
    WifiRig r;
    r.start();
    r.wait(3.0f);
    for (int i = 0; i < 5; ++i) r.press(Event::Down);   // the "hidden network" row
    r.keys("a");
    CHECK_EQ(r.id(), int(kSsid));
    r.keys("start");
    CHECK_EQ(r.id(), int(kSsid));                       // an empty name stays
    r.keys("a,start");                                  // "!" as the name
    CHECK_EQ(r.id(), int(kPassword));
    r.typeBang(9);
    r.keys("start");
    CHECK_EQ(r.id(), int(kPrompt));
    r.keys("a");
    CHECK_EQ(r.calls("wifi.begin ! secured hidden pw=9"), 1);
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 1);
    r.finish();
}

TEST(wifi_no_networks_rescan_and_ssid) {
    WifiRig r;
    r.sys().scanNone = true;
    r.start();
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kNetworks));
    r.keys("down,a");                                   // the first row is "scan again"? (cursor 1 = rescan in the sample list)
    CHECK_EQ(r.id(), int(kScan));
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kNetworks));
    r.keys("a");
    CHECK_EQ(r.id(), int(kSsid));
    r.keys("b");
    CHECK_EQ(r.id(), int(kNetworks));
    r.keys("b");
    CHECK(!r.active());
    r.finish();
}

TEST(wifi_no_adapter) {
    WifiRig r;
    r.sys().wifiPresent = false;
    r.start();
    r.wait(0.3f);
    CHECK_EQ(r.id(), int(kScanFailed));
    CHECK(r.result() && r.result()->tone == ui::Tone::Error);
    r.keys("a");                                        // Try again
    r.wait(0.3f);
    CHECK_EQ(r.id(), int(kScanFailed));
    CHECK_EQ(r.calls("wifi.scan"), 2);
    r.keys("b");
    CHECK(!r.active());
    CHECK(r.stack().empty());
    r.finish();
}

TEST(wifi_scan_error_shows_the_list_with_a_line) {
    WifiRig r;
    r.sys().scanError = true;
    r.start();
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kNetworks));
    r.keys("b");
    CHECK(!r.active());
    r.finish();
}

TEST(wifi_country_code) {
    WifiRig r;
    r.sys().needsCountry = true;
    r.pick(1);
    r.keys("a");
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kReview));
    r.keys("a");
    CHECK_EQ(r.id(), int(kCountry));
    CHECK_EQ(r.calls("wifi.save"), 0);                  // not saved yet
    r.keys("b");
    CHECK_EQ(r.id(), int(kReview));
    r.keys("a");
    CHECK_EQ(r.id(), int(kCountry));
    r.keys("start");
    CHECK_EQ(r.id(), int(kCountry));                    // empty is not a code
    CHECK_EQ(r.stack().size(), size_t(1));
    r.keys("a,a,start");                                // "!!" is not a code either
    CHECK_EQ(r.id(), int(kCountry));
    r.keys("b,b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 1);
    r.finish();
}

TEST(wifi_save_fails_then_retry) {
    WifiRig r;
    r.sys().saveOk = false;
    r.pick(1);
    r.keys("a");
    r.wait(3.0f);
    r.keys("a");
    CHECK_EQ(r.id(), int(kSaveFailed));
    r.keys("a");
    CHECK_EQ(r.id(), int(kReview));                     // Try again: the review
    r.keys("a");
    CHECK_EQ(r.id(), int(kSaveFailed));
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 1);
    r.finish();
}

TEST(wifi_gui_closes_during_the_test_rolls_back) {
    {
        WifiRig r;
        r.pick(1);
        r.keys("a");
        r.wait(0.5f);
        CHECK_EQ(r.id(), int(kTesting));
        r.guiCloses();
        CHECK_EQ(r.calls("wifi.rollback"), 1);
        CHECK(!r.sys().testing);
    }
    {
        WifiRig r;
        r.sys().needsCountry = true;
        r.pick(1);
        r.keys("a");
        r.wait(3.0f);
        r.keys("a");
        CHECK_EQ(r.id(), int(kCountry));
        r.guiCloses();
        CHECK_EQ(r.calls("wifi.rollback"), 1);
    }
    {   // at the review too
        WifiRig r;
        r.pick(1);
        r.keys("a");
        r.wait(3.0f);
        CHECK_EQ(r.id(), int(kReview));
        r.guiCloses();
        CHECK_EQ(r.calls("wifi.rollback"), 1);
    }
}

TEST(wifi_begin_fails) {
    WifiRig r;
    r.sys().beginFails = true;
    r.pick(1);
    r.keys("a");
    CHECK_EQ(r.id(), int(kFailed));
    r.keys("a");
    CHECK_EQ(r.id(), int(kPrompt));                     // open network: Try again goes back to the prompt
    r.keys("b");
    CHECK(!r.active());
    r.finish();
}

TEST(wifi_unsupported_row_does_nothing) {
    WifiRig r;
    r.start();
    r.wait(3.0f);
    r.keys("down,down,a");
    CHECK_EQ(r.id(), int(kNetworks));
    r.keys("b");
    r.finish();
}

TEST(wifi_timeout_stall) {
    WifiRig r;
    r.sys().testMode = SampleSystemService::TestMode::Stall;
    r.pick(1);
    r.keys("a");
    r.wait(31.0f);
    CHECK_EQ(r.id(), int(kFailed));
    CHECK_EQ(r.calls("wifi.rollback"), 0);              // the service ended the test itself (Wifi::pollTest)
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("wifi.rollback"), 0);
    r.finish();
}

// The invariant as a property: at every moment of the testing page the service holds exactly one test, and every way out
// (B, GUI closes) ends it exactly once.
TEST(wifi_every_exit_during_a_test_ends_it_once) {
    for (int exit = 0; exit < 3; ++exit) {
        for (float at : {0.2f, 0.7f, 1.2f, 1.9f}) {
            WifiRig r;
            r.pick(1);
            r.keys("a");
            r.wait(at);
            CHECK(r.sys().testing);
            if (exit == 0) r.keys("b");
            else if (exit == 1) r.guiCloses();
            else { r.keys("bhold"); r.keys("b"); }
            CHECK(!r.active() || exit == 2);
            CHECK_MSG(r.calls("wifi.rollback") == 1, "exit " + std::to_string(exit) + " at " + std::to_string(at) + " s: rollback called " + std::to_string(r.calls("wifi.rollback")) + " times");
            CHECK(!r.sys().testing);
        }
    }
}

}  // namespace
}  // namespace tt
