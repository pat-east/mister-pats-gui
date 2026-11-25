// Install/update Disc cores: Classic's row handler (SettingsScreen A on the row), confirmation (renderDiscCoreInstallConfirmation)
// and DiscCoreInstaller run as a Wizard: Confirm -> Working -> Result, one Medium frame. Classic showed a toast and put the
// progress and "{A} cancel" into the row's value; the work now has its own page (B cancels, as A on the row did).
//   A on the row while the installer works: the flow opens on Working (Classic: cancel it, or "finishing safely" while it installs).
//   A on the row with recovery blocked: the flow opens on the Result "blocked" (Classic: a 7 s toast of the installer's message).
//   Otherwise: Confirm -A-> Working -> Result (installed | failed | cancelled), A on a failed or cancelled one asks again.

#include "app/settings/SystemService.h"
#include "app/settings/system/SystemFlows.h"

namespace patsgui {
namespace {

enum { kConfirm = 0, kWork, kResult };

struct Flow {
    SystemService *svc = nullptr;
    enum class Outcome { Installed, Failed, Cancelled, Blocked } outcome = Outcome::Installed;
    bool followed = false;      // the page follows a run of this visit (started here, or already running when opened)
    bool stopping = false;      // B was pressed: the installer winds down
    bool refused = false;       // startDiscInstall() said no
    std::string message;        // the installer's own text (Details)
};
using FlowPtr = std::shared_ptr<Flow>;

ui::Text key(const char *k) { return ui::Text::key(k); }
ui::Text title() { return key("settings.system.disccores.title"); }

void addConfirm(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, title()));
    page->onDraw = [f](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Drive;
        c.headline = ui::Text::data(ui::format("settings.system.disccores.confirm.title", {f->svc->disc().packageVersion}));
        c.lines = {key("settings.system.disccores.confirm.line1"), key("settings.system.disccores.confirm.line2"),
                   key("settings.system.disccores.confirm.line3")};
        c.firstPrimary = true;
        ui::drawWizardCentered(p, t, body, c);
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "settings.system.hint.install", true), ui::hint(ui::HintKey::B, "hint.cancel", true)};
    };
    page->onEvent = [f](ui::WizardEvent e, ui::Wizard &w) {
        if (e != ui::WizardEvent::Confirm) return;
        f->followed = false;
        f->stopping = false;
        f->refused = false;
        w.go(kWork);
    };
    page->onBack = [] { return ui::kClose; };
    flow.add(kConfirm, std::move(page));
}

void addWork(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(title()));
    page->centered = true;
    ui::BusyPage *raw = page.get();
    auto fill = [f, raw]() {
        const DiscInstallInfo inst = f->svc->discInstall();
        const bool installing = inst.state == DiscInstallInfo::State::Installing;
        raw->cancelable = !installing && !f->stopping;
        raw->fraction = -1.0f;
        if (f->stopping) { raw->status = key("settings.system.disccores.working.stopping"); raw->detail = ui::Text(); }
        else if (installing) {
            raw->status = key("settings.system.disccores.working.installing");
            raw->detail = key("settings.system.disccores.working.finishing");
            raw->waitNote = key("settings.system.disccores.working.wait.installing");
        } else if (inst.state == DiscInstallInfo::State::Verifying) {
            raw->status = key("settings.system.disccores.working.verifying");
            raw->detail = ui::Text();
            raw->waitNote = key("settings.system.disccores.working.wait.verifying");
        } else {
            raw->status = key("settings.system.disccores.working.downloading");
            raw->waitNote = key("settings.system.disccores.working.wait.downloading");
            raw->setDetail(inst.bytes ? ui::Text::data(ui::format("settings.system.disccores.working.bytes", {std::to_string(inst.bytes / 1024)}))
                                      : ui::Text());
        }
    };
    page->onEnter = [f, fill](ui::Wizard &) {
        f->message.clear();
        f->stopping = false;
        if (f->svc->discInstall().busy) f->followed = true;   // opened while it works: follow that run
        else if (f->svc->startDiscInstall()) f->followed = true;
        else { f->refused = true; f->message = f->svc->discInstall().message; }
        fill();
    };
    page->onUpdate = [f, fill](float, ui::Wizard &w) {
        if (f->refused) {   // Classic: a toast of the installer's message; now a Result
            f->outcome = f->svc->discInstall().recoveryBlocked ? Flow::Outcome::Blocked : Flow::Outcome::Failed;
            w.go(kResult);
            return true;
        }
        const DiscInstallInfo inst = f->svc->discInstall();
        if (f->followed && !inst.busy) {
            f->message = inst.message;
            f->outcome = inst.state == DiscInstallInfo::State::Ready ? Flow::Outcome::Installed
                         : inst.state == DiscInstallInfo::State::Cancelled ? Flow::Outcome::Cancelled
                         : inst.recoveryBlocked ? Flow::Outcome::Blocked : Flow::Outcome::Failed;
            w.go(kResult);
            return true;
        }
        fill();
        return true;
    };
    page->onCancel = [f, fill](ui::Wizard &) {   // Classic: A on the row again while it downloads
        f->stopping = true;
        f->svc->cancelDiscInstall();
        fill();
    };
    flow.add(kWork, std::move(page));
}

void addResult(WizardFlowPage &flow, const FlowPtr &f) {
    flow.add(kResult, sysflow::resultPage(title(), [f](ui::WizardResult &r) {
        const std::string version = f->svc->disc().packageVersion;
        switch (f->outcome) {
            case Flow::Outcome::Installed:
                r.tone = ui::Tone::Done;
                r.headline = key("settings.system.disccores.done.headline");
                r.changed = ui::Text::data(ui::format("settings.system.disccores.done.changed", {version}));
                r.next = key("settings.system.disccores.done.next");
                break;
            case Flow::Outcome::Cancelled:
                r.tone = ui::Tone::Warning;
                r.headline = key("settings.system.disccores.cancelled.headline");
                r.changed = key("settings.system.disccores.cancelled.changed");
                r.next = key("settings.system.disccores.cancelled.next");
                r.retry = kConfirm;
                break;
            case Flow::Outcome::Blocked:
                r.tone = ui::Tone::Error;
                r.headline = key("settings.system.disccores.blocked.headline");
                r.changed = key("settings.system.disccores.blocked.changed");
                r.next = key("settings.system.disccores.blocked.next");
                r.details = ui::Text::data(f->message);
                break;
            case Flow::Outcome::Failed:
                r.tone = ui::Tone::Error;
                r.headline = key("settings.system.disccores.failed.headline");
                r.changed = key("settings.system.disccores.failed.changed");
                r.next = key("settings.system.disccores.failed.next");
                r.details = ui::Text::data(f->message);
                r.retry = kConfirm;
                break;
        }
    }));
}

std::unique_ptr<WizardFlowPage> makeDiscFlow(const FlowPtr &f) {
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    addConfirm(*flow, f);
    addWork(*flow, f);
    addResult(*flow, f);
    return flow;
}

FlowPtr newFlow(SystemService &service) {
    FlowPtr f = std::make_shared<Flow>();
    f->svc = &service;
    return f;
}

}  // namespace

void openDiscInstallFlow(SystemService &service, PageStack &pages) {
    FlowPtr f = newFlow(service);
    const DiscInstallInfo inst = service.discInstall();
    ui::PageId first = kConfirm;
    if (inst.busy) first = kWork;
    else if (inst.recoveryBlocked) { f->outcome = Flow::Outcome::Blocked; f->message = inst.message; first = kResult; }
    WizardFlowPage::launch(pages, makeDiscFlow(f), first);
}

bool openDiscInstallStep(SystemService &service, PageStack &pages, const std::string &step) {
    FlowPtr f = newFlow(service);
    f->message = service.discInstall().message;
    ui::PageId first = kConfirm;
    if (step == "working") first = kWork;
    else if (step == "done") { f->outcome = Flow::Outcome::Installed; first = kResult; }
    else if (step == "failed") { f->outcome = Flow::Outcome::Failed; f->message = "downloaded package verification failed"; first = kResult; }
    else if (step == "cancelled") { f->outcome = Flow::Outcome::Cancelled; first = kResult; }
    else if (step == "blocked") { f->outcome = Flow::Outcome::Blocked; first = kResult; }
    else if (!step.empty()) return false;
    WizardFlowPage::launch(pages, makeDiscFlow(f), first);
    return true;
}

void registerDiscInstallStrings(ui::Catalog &c) {
    c.set("settings.system.disccores.title", "Install Disc cores");
    c.set("settings.system.disccores.confirm.title", "Install USB CDROM Loader v{1}?");
    c.set("settings.system.disccores.confirm.line1", "The official disc-drive package is downloaded and checked first.");
    c.set("settings.system.disccores.confirm.line2", "Your MiSTer.ini and the files it replaces are backed up. Only the [CD-*] settings change.");
    c.set("settings.system.disccores.confirm.line3", "Existing PlayStation and Saturn launch files at the managed paths are replaced.");
    c.set("settings.system.disccores.working.downloading", "Downloading the package");
    c.set("settings.system.disccores.working.verifying", "Checking the package");
    c.set("settings.system.disccores.working.installing", "Installing the package");
    c.set("settings.system.disccores.working.finishing", "Finishing safely. Please wait.");
    c.set("settings.system.disccores.working.wait.downloading", "The files are downloaded from the Internet. A slow connection takes longer. Nothing is stuck.");
    c.set("settings.system.disccores.working.wait.verifying", "The download is checked before anything is installed. Nothing is stuck.");
    c.set("settings.system.disccores.working.wait.installing", "The files are being put in place on the SD card. Nothing is stuck; do not switch the MiSTer off.");
    c.set("settings.system.disccores.working.stopping", "Stopping the download");
    c.set("settings.system.disccores.working.bytes", "{1} KB downloaded");
    c.set("settings.system.disccores.done.headline", "Disc cores installed");
    c.set("settings.system.disccores.done.changed", "The Disc package v{1} is installed and checked.");
    c.set("settings.system.disccores.done.next", "Open Disc drive status to see the PlayStation and Saturn files.");
    c.set("settings.system.disccores.cancelled.headline", "Download stopped");
    c.set("settings.system.disccores.cancelled.changed", "Nothing was installed. Your files are unchanged.");
    c.set("settings.system.disccores.cancelled.next", "Start again when you are ready.");
    c.set("settings.system.disccores.failed.headline", "Could not install the Disc cores");
    c.set("settings.system.disccores.failed.changed", "The package was not installed.");
    c.set("settings.system.disccores.failed.next", "Check the Internet connection, then try again.");
    c.set("settings.system.disccores.blocked.headline", "Disc package actions are turned off");
    c.set("settings.system.disccores.blocked.changed", "A recovery step failed earlier, so nothing can be installed now.");
    c.set("settings.system.disccores.blocked.next", "Fix the problem named in the details, then restart the GUI.");
}

}  // namespace patsgui
