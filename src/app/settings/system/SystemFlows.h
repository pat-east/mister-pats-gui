#pragma once

// The small System sequences as wizard flows (WizardFlowPage, one Medium frame each; Classic's state machines and messages stay,
// only the presentation changed; src/app/PARITY-settings.md, System):
//   Time zone            TimeZoneFlow.cpp   Region -> City (letter jump) ; Failed
//   Back up GUI settings BackupFlow.cpp     Working -> Done | Failed
//   Download icons       IconsFlow.cpp      Working -> Done | Failed
//   Install Disc cores   DiscInstallFlow.cpp Confirm -> Working -> Result (done, failed, cancelled, blocked)
//   Reboot               RebootFlow.cpp     Confirm ; Failed
// Info, Disc drive status and the changelog stay pages (reading): InfoPage in StandardPages.

#include <memory>
#include <string>

#include "app/settings/SettingsCategory.h"
#include "app/settings/WizardFlowPage.h"

namespace patsgui {

class SystemService;

// A on the row. Time zone: with no region list the flow opens on its Result page (Classic toasted).
void openTimeZoneFlow(SystemService &service, PageStack &pages);
// `reporting` is true while the flow owns the backup's result: SystemSettings must not report it again.
void openBackupFlow(SystemService &service, PageStack &pages, const std::shared_ptr<bool> &reporting);
void openIconsFlow(SystemService &service, PageStack &pages);
void openDiscInstallFlow(SystemService &service, PageStack &pages);
void openRebootFlow(SystemService &service, PageStack &pages);

// The host runner opens one step (`--page system/<id>`): timezone, timezone.city, timezone.failed, backup, backup.failed,
// backup.done, icons, icons.failed, icons.done, discinstall, discinstall.working, discinstall.failed, discinstall.cancelled,
// discinstall.blocked, discinstall.done, reboot, reboot.failed. False for an unknown id.
bool openSystemFlowStepForTest(SystemService &service, PageStack &pages, const std::string &id,
                               const std::shared_ptr<bool> &backupReporting);

void registerSystemFlowStrings(ui::Catalog &c);

// One step of one flow (each in its flow's file); "" is the first page. False for an unknown step.
bool openTimeZoneStep(SystemService &service, PageStack &pages, const std::string &step);
bool openBackupStep(SystemService &service, PageStack &pages, const std::string &step, const std::shared_ptr<bool> &reporting);
bool openIconsStep(SystemService &service, PageStack &pages, const std::string &step);
bool openDiscInstallStep(SystemService &service, PageStack &pages, const std::string &step);
bool openRebootStep(SystemService &service, PageStack &pages, const std::string &step);
void registerTimeZoneStrings(ui::Catalog &c);
void registerBackupStrings(ui::Catalog &c);
void registerIconsStrings(ui::Catalog &c);
void registerDiscInstallStrings(ui::Catalog &c);
void registerRebootStrings(ui::Catalog &c);

// ---- Shared by the flows (FlowParts)
namespace sysflow {

// One line under a list: icon + word (never colour alone) and text.
struct Line {
    ui::Tone tone = ui::Tone::Neutral;
    ui::Text word, text;
};

// A choose page: the headline (at most two lines in the text column), the list under it, and the status line at the bottom.
void drawHeadlineChoose(ui::Painter &p, const ui::Theme &t, const ui::Rect &body, const ui::Text &headline, ui::ChooseList &list,
                        const Line &line);

// A Result page whose outcome `fill` builds when the page is entered.
std::unique_ptr<ui::ResultPage> resultPage(const ui::Text &title, std::function<void(ui::WizardResult &)> fill,
                                           std::function<void()> onFinished = nullptr);

}  // namespace sysflow
}  // namespace patsgui
