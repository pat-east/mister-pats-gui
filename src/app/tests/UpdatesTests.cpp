// Updates: the update flow (confirm, download, restart prompt, failures). The category opens it from the snapshot's phase, so
// the tests set the sample phase and activate the same rows the first level has.
// Page ids: src/app/settings/updates/UpdatePages.cpp.

#include "app/tests/Generic.h"
#include "app/tests/Test.h"

namespace tt {
namespace {

enum { kConfirm = 0, kBusy = 1, kRestart = 2, kRestartError = 3, kPinError = 4, kPinTry = 5, kFailed = 6 };
using K = ui::HintKey;

void expectTone(Rig &r, ui::Tone tone, const char *what) {
    ui::WizardResult *res = r.result();
    CHECK_MSG(res != nullptr, std::string(what) + ": not a Result page (page " + std::to_string(r.id()) + ")");
    if (res) CHECK_MSG(res->tone == tone, std::string(what) + ": wrong tone");
}

void toConfirm(Rig &r) {
    r.set("updates.phase", "available");
    r.open("updates", "updates.install");
}

// Install and wait until the prompt shows (every sample phase: downloading 0.5 s steps, verifying, installing, restart).
void toRestartPrompt(Rig &r) {
    toConfirm(r);
    r.keys("a");
    r.wait(10.0f);
}

TEST(updates_confirm_page) {
    Rig r;
    toConfirm(r);
    CHECK_EQ(r.id(), int(kConfirm));
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::Confirming);
    CHECK_EQ(r.hintWord(K::A), std::string("Install"));
    CHECK_EQ(r.hintWord(K::B), std::string("Cancel"));
    CHECK(r.hasHint(K::UpDown));                    // the release notes scroll
    r.keys("down,down,lnext,lprev");
    CHECK_EQ(r.id(), int(kConfirm));
    CHECK_EQ(r.upd().installs, 0);
}

TEST(updates_cancel_at_confirm) {
    Rig r;
    toConfirm(r);
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.upd().installs, 0);
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::Available);   // cancelBeforeCommit: back to "available"
    r.wait(1.0f);
    CHECK(!r.active());                             // and the flow does not come back by itself
}

TEST(updates_start_acts_as_a_on_the_confirm) {
    Rig r;
    toConfirm(r);
    r.keys("start");
    CHECK_EQ(r.upd().installs, 1);
    CHECK_EQ(r.id(), int(kBusy));
}

TEST(updates_every_phase_of_an_install) {
    Rig r;
    toConfirm(r);
    r.keys("a");
    CHECK_EQ(r.id(), int(kBusy));
    CHECK_EQ(r.upd().installs, 1);
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::Downloading);
    CHECK(r.cancellableBusy());
    CHECK_EQ(r.hintWord(K::B), std::string("Cancel"));
    r.wait(3.0f);                                   // 5 x 8 MB steps of 0.5 s reach the 40 MB goal
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::Verifying);
    CHECK(r.cancellableBusy());                     // still stoppable before the commit
    r.wait(1.6f);
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::Installing);
    CHECK_EQ(r.id(), int(kBusy));
    CHECK(!r.cancellableBusy());                    // the commit: no cancel, no hint
    CHECK(!r.hasHint(K::B));
    r.keys("b");
    CHECK_EQ(r.id(), int(kBusy));
    CHECK_EQ(r.upd().cancels, 0);
    r.wait(1.6f);
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::RestartRequired);
    CHECK_EQ(r.id(), int(kRestart));                // the prompt follows the snapshot
    CHECK_EQ(r.hintWord(K::A), std::string("Choose"));
    CHECK_EQ(r.hintWord(K::B), std::string("Later"));
}

TEST(updates_cancel_during_the_download) {
    Rig r;
    toConfirm(r);
    r.keys("a,wait1.0");
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::Downloading);
    r.keys("b");
    CHECK_EQ(r.upd().cancels, 1);
    r.wait(0.5f);
    CHECK(!r.active());                             // the snapshot left the busy phases: the flow closed itself
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::Available);
    CHECK_EQ(r.upd().restarts, 0);
}

TEST(updates_cancel_while_verifying) {
    Rig r;
    toConfirm(r);
    r.keys("a,wait3.2");
    CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::Verifying);
    r.keys("b");
    CHECK_EQ(r.upd().cancels, 1);
    r.wait(0.5f);
    CHECK(!r.active());
}

TEST(updates_restart_prompt_choices) {
    {   // Restart now
        Rig r;
        toRestartPrompt(r);
        CHECK_EQ(r.id(), int(kRestart));
        r.keys("a");
        CHECK_EQ(r.upd().restarts, 1);
        CHECK(!r.upd().lastRestartRemovedPin);
    }
    {   // Later (the second row)
        Rig r;
        toRestartPrompt(r);
        r.keys("down,a");
        CHECK(!r.active());
        CHECK_EQ(r.upd().restarts, 0);
        CHECK(!r.upd().snapshot().successPromptVisible);   // dismissInstallSuccess
        CHECK_EQ(r.upd().snapshot().phase, UpdatePhase::RestartRequired);
    }
    {   // B is Later too
        Rig r;
        toRestartPrompt(r);
        r.keys("b");
        CHECK(!r.active());
        CHECK_EQ(r.upd().restarts, 0);
        CHECK(!r.upd().snapshot().successPromptVisible);
    }
    {   // Start restarts like A on the first row
        Rig r;
        toRestartPrompt(r);
        r.keys("start");
        CHECK_EQ(r.upd().restarts, 1);
    }
}

TEST(updates_restart_prompt_when_pinned) {
    {   // Use newest version and restart (the third row)
        Rig r;
        r.set("updates.pinned", "1");
        r.set("updates.phase", "restart");
        r.open("updates", "updates.restart");
        CHECK_EQ(r.id(), int(kRestart));
        r.keys("down,down,a");
        CHECK_EQ(r.upd().restarts, 1);
        CHECK(r.upd().lastRestartRemovedPin);
    }
    {   // X is the shortcut, only when pinned
        Rig r;
        r.set("updates.pinned", "1");
        r.set("updates.phase", "restart");
        r.open("updates", "updates.restart");
        r.keys("x");
        CHECK(r.upd().lastRestartRemovedPin);
        CHECK_EQ(r.upd().restarts, 1);
    }
    {
        Rig r;
        r.set("updates.phase", "restart");
        r.open("updates", "updates.restart");
        r.keys("x");
        CHECK_EQ(r.upd().restarts, 0);              // not pinned: X does nothing
        r.keys("down,down,a");                      // and there is no third row: the cursor stays on Later
        CHECK(!r.upd().lastRestartRemovedPin);
    }
}

TEST(updates_restart_fails_then_try_again) {
    Rig r;
    toRestartPrompt(r);
    r.set("updates.fail", "restart");
    r.keys("a");
    CHECK_EQ(r.id(), int(kRestartError));
    expectTone(r, ui::Tone::Error, "restart failed");
    CHECK(r.result() && !r.result()->details.empty());
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    CHECK_EQ(r.upd().restarts, 0);
    r.keys("a");
    CHECK_EQ(r.id(), int(kRestart));                // Try again: the prompt
    r.keys("a");
    CHECK_EQ(r.upd().restarts, 1);                  // the second attempt works
    // B on the failure page closes it; the prompt is a function of the snapshot (Classic), so it shows again until Later
    Rig q;
    toRestartPrompt(q);
    q.set("updates.fail", "restart");
    q.keys("a,b");
    CHECK_EQ(q.id(), int(kRestart));
    q.keys("b");
    CHECK(!q.active());
    CHECK(!q.upd().snapshot().successPromptVisible);
}

TEST(updates_pin_removal_fails_then_try_again) {
    Rig r;
    r.set("updates.pinned", "1");
    r.set("updates.phase", "current");
    r.set("updates.fail", "pin");
    r.open("updates", "updates.usenewest");
    CHECK_EQ(r.id(), int(kPinError));
    expectTone(r, ui::Tone::Error, "pin removal failed");
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    CHECK_EQ(r.upd().pinRemovals, 0);
    r.keys("a");                                    // the second try works
    r.wait(0.5f);
    CHECK_EQ(r.upd().pinRemovals, 1);
    CHECK_EQ(r.id(), int(kRestart));                // "pin removed": the restart prompt
    r.keys("b");
    CHECK(!r.active());
}

TEST(updates_error_phase_ends_in_a_result) {
    Rig r;
    toConfirm(r);
    r.keys("a,wait0.6");
    CHECK_EQ(r.id(), int(kBusy));
    r.set("updates.phase", "error");
    r.wait(0.5f);
    CHECK_EQ(r.id(), int(kFailed));
    expectTone(r, ui::Tone::Error, "update failed");
    CHECK_EQ(r.hintWord(K::AB), std::string("Done"));
    r.keys("a");
    CHECK(!r.active());
}

TEST(updates_gui_closes_during_the_download_does_not_cancel) {
    Rig r;
    toConfirm(r);
    r.keys("a,wait1.0");
    r.guiCloses();
    CHECK(r.stack().empty());
    CHECK_EQ(r.upd().cancels, 0);                   // Classic: the download continues behind the GUI
}

}  // namespace
}  // namespace tt
