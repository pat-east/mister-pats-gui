// Reboot MiSTer: Classic's confirmation (App::rebootMister(); "Reboot MiSTer?", "The system will restart now.", A restart, B
// cancel) and its error ("Cannot restart MiSTer" + the OS text) as a Wizard: Confirm -> (the system restarts) | Failed.

#include "app/settings/SystemService.h"
#include "app/settings/system/SystemFlows.h"

namespace patsgui {
namespace {

enum { kConfirm = 0, kFailed };

struct Flow {
    SystemService *svc = nullptr;
    std::string error;
};
using FlowPtr = std::shared_ptr<Flow>;

ui::Text key(const char *k) { return ui::Text::key(k); }
ui::Text title() { return key("settings.system.reboot.title"); }

std::unique_ptr<WizardFlowPage> makeRebootFlow(const FlowPtr &f) {
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    std::unique_ptr<ui::FnPage> ask(new ui::FnPage(ui::WizardPage::Kind::Step, title()));
    ask->onDraw = [](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        ui::WizardCentered c;
        c.icon = ui::IconId::Alert;
        c.headline = key("settings.system.reboot.headline");
        c.lines = {key("settings.system.reboot.body"), key("settings.system.reboot.body2")};
        c.firstPrimary = true;
        ui::drawWizardCentered(p, t, body, c);
    };
    ask->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "settings.system.hint.restart", true), ui::hint(ui::HintKey::B, "hint.cancel", true)};
    };
    ask->onEvent = [f](ui::WizardEvent e, ui::Wizard &w) {
        if (e != ui::WizardEvent::Confirm) return;
        if (f->svc->reboot(f->error)) w.close();   // Classic: App::rebootMister(); the system goes down
        else w.go(kFailed);                        // Classic: Context::showError("Cannot restart MiSTer", strerror(errno))
    };
    ask->onBack = [] { return ui::kClose; };
    flow->add(kConfirm, std::move(ask));
    flow->add(kFailed, sysflow::resultPage(title(), [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Error;
        r.headline = key("settings.system.reboot.failed.headline");
        r.changed = key("settings.system.reboot.failed.changed");
        r.next = key("settings.system.reboot.failed.next");
        r.details = ui::Text::data(f->error);
        r.retry = kConfirm;
    }));
    return flow;
}

FlowPtr newFlow(SystemService &service) {
    FlowPtr f = std::make_shared<Flow>();
    f->svc = &service;
    return f;
}

}  // namespace

void openRebootFlow(SystemService &service, PageStack &pages) {
    WizardFlowPage::launch(pages, makeRebootFlow(newFlow(service)), kConfirm);
}

bool openRebootStep(SystemService &service, PageStack &pages, const std::string &step) {
    FlowPtr f = newFlow(service);
    ui::PageId first = kConfirm;
    if (step == "failed") { f->error = "Operation not permitted"; first = kFailed; }
    else if (!step.empty()) return false;
    WizardFlowPage::launch(pages, makeRebootFlow(f), first);
    return true;
}

void registerRebootStrings(ui::Catalog &c) {
    c.set("settings.system.reboot.title", "Restart MiSTer");
    c.set("settings.system.reboot.headline", "Restart MiSTer now?");
    c.set("settings.system.reboot.body", "The system will restart now.");
    c.set("settings.system.reboot.body2", "You will leave this GUI until it starts again.");
    c.set("settings.system.reboot.failed.headline", "Could not restart MiSTer");
    c.set("settings.system.reboot.failed.changed", "MiSTer is still running. Nothing was changed.");
    c.set("settings.system.reboot.failed.next", "Try again, or restart MiSTer from its own menu.");
}

}  // namespace patsgui
