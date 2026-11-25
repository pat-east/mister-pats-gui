// Time zone, Backup, Icons, Disc install, Reboot (page ids: src/app/settings/system/*Flow.cpp).

#include "app/tests/Generic.h"
#include "app/tests/Test.h"

namespace tt {
namespace {

using K = ui::HintKey;

void expectTone(Rig &r, ui::Tone tone, const char *what) {
    ui::WizardResult *res = r.result();
    CHECK_MSG(res != nullptr, std::string(what) + ": not a Result page (page " + std::to_string(r.id()) + ")");
    if (res) CHECK_MSG(res->tone == tone, std::string(what) + ": wrong tone");
}

// ================================================================================================ Time zone
enum { kRegion = 0, kCity = 1, kZoneFailed = 2 };

void toEurope(Rig &r) { for (int i = 0; i < 11; ++i) r.press(Event::Down); }   // Europe is the 12th region

TEST(timezone_success) {
    Rig r;
    r.open("system", "system.timezone");
    CHECK_EQ(r.id(), int(kRegion));
    CHECK_EQ(r.hintWord(K::A), std::string("Open"));
    toEurope(r);
    r.keys("a");
    CHECK_EQ(r.id(), int(kCity));
    CHECK_EQ(r.hintWord(K::A), std::string("Choose"));
    r.keys("down,down,a");                         // Berlin
    CHECK(!r.active());
    CHECK_EQ(r.calls("zone.set"), 1);
    CHECK_EQ(r.sys().zone, std::string("Europe/Berlin"));
}

TEST(timezone_back_keeps_the_region_row) {
    Rig r;
    r.open("system", "system.timezone");
    toEurope(r);
    r.keys("a,b");
    CHECK_EQ(r.id(), int(kRegion));
    r.keys("a");                                   // the cursor is still on Europe: its cities again
    CHECK_EQ(r.id(), int(kCity));
    r.keys("b,b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("zone.set"), 0);
}

TEST(timezone_save_fails_then_retry) {
    Rig r;
    r.set("system.zonesave", "fail");
    r.open("system", "system.timezone");
    toEurope(r);
    r.keys("a,down,down,a");
    CHECK_EQ(r.id(), int(kZoneFailed));
    expectTone(r, ui::Tone::Error, "zone save failed");
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.set("system.zonesave", "ok");
    r.keys("a");
    CHECK_EQ(r.id(), int(kCity));
    r.keys("a");
    CHECK(!r.active());
    CHECK_EQ(r.calls("zone.set"), 2);
    CHECK_EQ(r.sys().zone, std::string("Europe/Amsterdam"));
}

TEST(timezone_no_list) {
    Rig r;
    r.set("system.zones", "empty");
    r.open("system", "system.timezone");
    CHECK_EQ(r.id(), int(kZoneFailed));
    expectTone(r, ui::Tone::Error, "no zone list");
    CHECK_EQ(r.hintWord(K::AB), std::string("Done"));     // nothing to try again
    r.keys("a");
    CHECK(!r.active());
}

TEST(timezone_empty_region_stays) {
    Rig r;
    r.set("system.zones", "regionempty");
    r.open("system", "system.timezone");
    r.keys("down,down,a");                          // Antarctica
    CHECK_EQ(r.id(), int(kRegion));
    CHECK_EQ(r.stack().size(), size_t(1));
}

TEST(timezone_long_list_offers_the_jump) {
    Rig r;
    r.open("system", "system.timezone");
    r.keys("down,a");                               // America: 46 cities
    CHECK_EQ(r.id(), int(kCity));
    CHECK(r.hasHint(K::Triggers));
    r.keys("lnext,a");
    CHECK(!r.active());
    CHECK(r.sys().zone != "America/Anchorage");     // the jump moved the cursor to the next letter
}

// ================================================================================================ Backup
enum { kBackupWork = 0, kBackupDone = 1, kBackupFailed = 2 };

TEST(backup_success) {
    Rig r;
    r.open("system", "system.backup");
    CHECK_EQ(r.id(), int(kBackupWork));
    CHECK_EQ(r.kind(), ui::WizardPage::Kind::Busy);
    CHECK(!r.hasHint(K::B));                        // it cannot be stopped: no hint
    r.keys("b");                                    // swallowed
    CHECK_EQ(r.id(), int(kBackupWork));
    r.wait(1.5f);
    CHECK_EQ(r.id(), int(kBackupDone));
    expectTone(r, ui::Tone::Done, "backup done");
    CHECK_EQ(r.calls("backup.start"), 1);
    CHECK_EQ(r.calls("backup.ack"), 1);             // the result is acknowledged here, not reported again by the category
    r.keys("a");
    CHECK(!r.active());
    r.wait(1.0f);
    CHECK_EQ(r.calls("backup.ack"), 1);
    CHECK_EQ(r.calls("backup.start"), 1);
}

TEST(backup_failure_and_retry) {
    Rig r;
    r.set("system.backupresult", "fail");
    r.open("system", "system.backup");
    r.wait(1.5f);
    CHECK_EQ(r.id(), int(kBackupFailed));
    expectTone(r, ui::Tone::Error, "backup failed");
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.set("system.backupresult", "ok");
    r.keys("a");
    CHECK_EQ(r.id(), int(kBackupWork));
    r.wait(1.5f);
    CHECK_EQ(r.id(), int(kBackupDone));
    CHECK_EQ(r.calls("backup.start"), 2);
    r.keys("b");                                    // B closes a result too
    CHECK(!r.active());
}

TEST(backup_gui_closes_while_running) {
    Rig r;
    r.open("system", "system.backup");
    r.wait(0.3f);
    r.guiCloses();
    CHECK(r.stack().empty());
    CHECK_EQ(r.calls("backup.start"), 1);
}

// ================================================================================================ Icons
enum { kIconsWork = 0, kIconsDone = 1, kIconsFailed = 2 };

TEST(icons_download_success) {
    Rig r;
    r.open("system", "system.icons");
    CHECK_EQ(r.id(), int(kIconsWork));
    CHECK(!r.hasHint(K::B));
    r.wait(1.5f);
    CHECK_EQ(r.id(), int(kIconsDone));
    expectTone(r, ui::Tone::Done, "icons done");
    CHECK_EQ(r.calls("icons.start"), 1);
    r.keys("a");
    CHECK(!r.active());
}

TEST(icons_all_ready) {
    Rig r;
    r.set("system.icons", "ready");
    r.open("system", "system.icons");
    r.wait(0.3f);
    CHECK_EQ(r.id(), int(kIconsDone));
    expectTone(r, ui::Tone::Done, "icons ready");
    r.keys("b");
    CHECK(!r.active());
}

TEST(icons_failure_and_retry) {
    {
        Rig r;
        r.set("system.iconsresult", "fail");
        r.open("system", "system.icons");
        r.wait(1.5f);
        CHECK_EQ(r.id(), int(kIconsFailed));
        expectTone(r, ui::Tone::Error, "icons failed");
        CHECK(r.result() && !r.result()->details.empty());
        CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
        r.set("system.iconsresult", "ok");
        r.keys("a");
        CHECK_EQ(r.id(), int(kIconsWork));
        r.wait(1.5f);
        CHECK_EQ(r.id(), int(kIconsDone));
        CHECK_EQ(r.calls("icons.start"), 2);
    }
    {   // the download cannot even start
        Rig r;
        r.set("system.icons", "startfails");
        r.open("system", "system.icons");
        r.wait(0.3f);
        CHECK_EQ(r.id(), int(kIconsFailed));
        r.keys("b");
        CHECK(!r.active());
    }
}

// ================================================================================================ Disc install
enum { kDiscConfirm = 0, kDiscWork = 1, kDiscResult = 2 };

TEST(discinstall_success) {
    Rig r;
    r.set("system.discpackage", "none");
    r.open("system", "system.disccores");
    CHECK_EQ(r.id(), int(kDiscConfirm));
    CHECK_EQ(r.hintWord(K::A), std::string("Install"));
    CHECK_EQ(r.hintWord(K::B), std::string("Cancel"));
    CHECK_EQ(r.calls("disc.start"), 0);
    r.keys("a");
    CHECK_EQ(r.id(), int(kDiscWork));
    CHECK_EQ(r.calls("disc.start"), 1);
    CHECK(r.cancellableBusy());
    r.wait(8.0f);
    CHECK_EQ(r.id(), int(kDiscResult));
    expectTone(r, ui::Tone::Done, "disc installed");
    r.keys("a");
    CHECK(!r.active());
    CHECK(r.sys().disc().packageReady);
}

TEST(discinstall_cancel_at_confirm) {
    Rig r;
    r.open("system", "system.disccores");
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("disc.start"), 0);
    CHECK_EQ(r.calls("disc.cancel"), 0);
}

TEST(discinstall_cancel_while_downloading) {
    Rig r;
    r.open("system", "system.disccores");
    r.keys("a,wait0.4");
    CHECK(r.cancellableBusy());
    r.keys("b");
    CHECK_EQ(r.calls("disc.cancel"), 1);
    CHECK_EQ(r.id(), int(kDiscWork));               // "Stopping": the installer winds down first
    CHECK(!r.hasHint(K::B));
    r.keys("b");
    CHECK_EQ(r.calls("disc.cancel"), 1);            // a second B does not cancel again
    r.wait(3.0f);
    CHECK_EQ(r.id(), int(kDiscResult));
    expectTone(r, ui::Tone::Warning, "disc cancelled");
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.keys("a");
    CHECK_EQ(r.id(), int(kDiscConfirm));            // Try again: the confirmation
    r.keys("b");
    CHECK(!r.active());
}

TEST(discinstall_cannot_be_stopped_while_installing) {
    Rig r;
    r.open("system", "system.disccores");
    r.keys("a,wait4.6");                            // the sample installs from its 4th second on
    CHECK_EQ(r.sys().discInstall().state, DiscInstallInfo::State::Installing);
    CHECK_EQ(r.id(), int(kDiscWork));
    CHECK(!r.cancellableBusy());
    CHECK(!r.hasHint(K::B));
    r.keys("b");
    CHECK_EQ(r.calls("disc.cancel"), 0);
    r.wait(4.0f);
    CHECK_EQ(r.id(), int(kDiscResult));
}

TEST(discinstall_failure_and_blocked) {
    {
        Rig r;
        r.set("system.installend", "failed");
        r.open("system", "system.disccores");
        r.keys("a,wait8");
        CHECK_EQ(r.id(), int(kDiscResult));
        expectTone(r, ui::Tone::Error, "disc failed");
        CHECK(r.result() && !r.result()->details.empty());
        CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
        r.set("system.installend", "ready");
        r.keys("a,a,wait8");
        CHECK_EQ(r.id(), int(kDiscResult));
        expectTone(r, ui::Tone::Done, "disc retried");
        CHECK_EQ(r.calls("disc.start"), 2);
    }
    {   // the installer needs attention: opens on its result; nothing to try again
        Rig r;
        r.set("system.discinstall", "blocked");
        r.open("system", "system.disccores");
        CHECK_EQ(r.id(), int(kDiscResult));
        expectTone(r, ui::Tone::Error, "disc blocked");
        CHECK_EQ(r.hintWord(K::AB), std::string("Done"));
        r.keys("a");
        CHECK(!r.active());
        CHECK_EQ(r.calls("disc.start"), 0);
    }
    {   // a run that ends cancelled without B
        Rig r;
        r.set("system.installend", "cancelled");
        r.open("system", "system.disccores");
        r.keys("a,wait8");
        expectTone(r, ui::Tone::Warning, "disc cancelled by the service");
    }
}

TEST(discinstall_follows_a_running_install) {
    Rig r;
    r.set("system.discinstall", "downloading");
    r.open("system", "system.disccores");
    CHECK_EQ(r.id(), int(kDiscWork));
    CHECK_EQ(r.calls("disc.start"), 0);             // opened while it works: follows that run, starts nothing
}

// ================================================================================================ Reboot
enum { kRebootConfirm = 0, kRebootFailed = 1 };

TEST(reboot_success) {
    Rig r;
    r.open("system", "system.reboot");
    CHECK_EQ(r.id(), int(kRebootConfirm));
    CHECK_EQ(r.hintWord(K::B), std::string("Cancel"));
    r.keys("a");
    CHECK_EQ(r.calls("reboot"), 1);
    CHECK(r.sys().rebooted);
    CHECK(!r.active());
}

TEST(reboot_cancel) {
    Rig r;
    r.open("system", "system.reboot");
    r.keys("b");
    CHECK(!r.active());
    CHECK_EQ(r.calls("reboot"), 0);
    // Start does not restart: only A confirms
    r.open("system", "system.reboot");
    r.keys("start");
    CHECK_EQ(r.calls("reboot"), 0);
    CHECK(r.active());
}

TEST(reboot_failure) {
    Rig r;
    r.set("system.reboot", "fails");
    r.open("system", "system.reboot");
    r.keys("a");
    CHECK_EQ(r.id(), int(kRebootFailed));
    expectTone(r, ui::Tone::Error, "reboot failed");
    CHECK(r.result() && !r.result()->details.empty());
    CHECK_EQ(r.hintWord(K::A), std::string("Try again"));
    r.keys("a");
    CHECK_EQ(r.id(), int(kRebootConfirm));
    r.set("system.reboot", "ok");
    r.keys("a");
    CHECK(!r.active());
    CHECK_EQ(r.calls("reboot"), 2);
    CHECK(r.sys().rebooted);
}

}  // namespace
}  // namespace tt
