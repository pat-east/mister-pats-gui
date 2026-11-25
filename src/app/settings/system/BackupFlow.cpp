// Back up GUI settings: Classic's BackupTask run (SettingsScreen row -> startBackup(); App::update() reports Complete as a notice
// and Failed as an error dialog) shown as a Wizard: Working -> Done | Failed, one Medium frame. Classic asks nothing before it
// starts and the task cannot be stopped, so the first page is the work and it offers no Cancel. The result is reported here,
// once (acknowledgeBackup), and SystemSettings does not report it again while `reporting` is set.

#include "app/settings/SystemService.h"
#include "app/settings/system/SystemFlows.h"

namespace patsgui {
namespace {

enum { kWork = 0, kDone, kFailed };

struct Flow {
    SystemService *svc = nullptr;
    std::shared_ptr<bool> reporting;
    std::string folder;   // where the archive went (Complete)
};
using FlowPtr = std::shared_ptr<Flow>;

ui::Text key(const char *k) { return ui::Text::key(k); }
ui::Text title() { return key("settings.system.backup.title"); }

void addWorkPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(title()));
    page->centered = true;
    page->cancelable = false;
    page->status = key("settings.system.backup.working");
    page->waitNote = key("settings.system.backup.wait");
    page->onEnter = [f](ui::Wizard &) {
        *f->reporting = true;
        f->svc->startBackup();   // false: it already runs (Classic is silent then); the page just follows it
    };
    page->onUpdate = [f](float, ui::Wizard &w) {
        const BackupInfo b = f->svc->backup();
        if (b.state == BackupInfo::State::Running || b.state == BackupInfo::State::Idle) return false;
        f->svc->acknowledgeBackup();   // the result is shown here
        const size_t slash = b.path.find_last_of('/');
        f->folder = slash == std::string::npos ? ui::Catalog::get().tr("settings.system.backup.usbFolder") : b.path.substr(0, slash);
        w.go(b.state == BackupInfo::State::Complete ? kDone : kFailed);
        return true;
    };
    page->onLeave = [f](bool cancelled) { if (cancelled) *f->reporting = false; };   // the GUI closed
    flow.add(kWork, std::move(page));
}

}  // namespace

static std::unique_ptr<WizardFlowPage> makeBackupFlow(const std::shared_ptr<Flow> &f) {
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    addWorkPage(*flow, f);
    auto finished = [f] { *f->reporting = false; };
    flow->add(kDone, sysflow::resultPage(title(), [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Done;
        r.headline = key("settings.system.backup.done.headline");
        r.changed = ui::Text::data(ui::format("settings.system.backup.done.changed", {f->folder}));
        r.next = key("settings.system.backup.done.next");
    }, finished));
    flow->add(kFailed, sysflow::resultPage(title(), [](ui::WizardResult &r) {
        r.tone = ui::Tone::Error;
        r.headline = key("settings.system.backup.failed.headline");
        r.changed = key("settings.system.backup.failed.changed");
        r.next = key("settings.system.backup.failed.next");
        r.retry = kWork;
    }, finished));
    return flow;
}

void openBackupFlow(SystemService &service, PageStack &pages, const std::shared_ptr<bool> &reporting) {
    auto f = std::make_shared<Flow>();
    f->svc = &service;
    f->reporting = reporting;
    WizardFlowPage::launch(pages, makeBackupFlow(f), kWork);
}

bool openBackupStep(SystemService &service, PageStack &pages, const std::string &step, const std::shared_ptr<bool> &reporting) {
    auto f = std::make_shared<Flow>();
    f->svc = &service;
    f->reporting = reporting;
    ui::PageId first = kWork;
    if (step == "done") { f->folder = "/media/usb0/mister-pat-backups"; first = kDone; }
    else if (step == "failed") first = kFailed;
    else if (!step.empty()) return false;
    WizardFlowPage::launch(pages, makeBackupFlow(f), first);
    return true;
}

void registerBackupStrings(ui::Catalog &c) {
    c.set("settings.system.backup.title", "Back up GUI settings");
    c.set("settings.system.backup.working", "Saving your settings");
    c.set("settings.system.backup.wait", "Your settings and lists are copied to the backup drive. A slow USB drive takes longer. Nothing is stuck.");
    c.set("settings.system.backup.usbFolder", "the USB backup folder");
    c.set("settings.system.backup.done.headline", "Backup saved");
    c.set("settings.system.backup.done.changed", "Your GUI settings were saved to {1}.");
    c.set("settings.system.backup.done.next", "Restore a backup from the console.");
    c.set("settings.system.backup.failed.headline", "Could not back up your settings");
    c.set("settings.system.backup.failed.changed", "No settings were changed and no backup was created.");
    c.set("settings.system.backup.failed.next", "Connect one writable USB drive, then try again.");
}

}  // namespace patsgui
