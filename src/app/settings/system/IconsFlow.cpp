// Download missing icons: Classic's App::startAssetDownload() (the row; its notices: started, already running, all ready, failed to
// start) and the notice when the download ends (App::run) shown as a Wizard: Working -> Done | Failed, one Medium frame. The
// download cannot be stopped in Classic, so the Working page offers no Cancel.

#include "app/settings/SystemService.h"
#include "app/settings/system/SystemFlows.h"

namespace patsgui {
namespace {

enum { kWork = 0, kDone, kFailed };

struct Flow {
    SystemService *svc = nullptr;
    enum class Start { Running, AllReady, Failed } start = Start::Running;
    std::string error;
};
using FlowPtr = std::shared_ptr<Flow>;

ui::Text key(const char *k) { return ui::Text::key(k); }
ui::Text title() { return key("settings.system.icons.title"); }

std::unique_ptr<WizardFlowPage> makeIconsFlow(const FlowPtr &f) {
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    std::unique_ptr<ui::BusyPage> work(new ui::BusyPage(title()));
    work->centered = true;
    work->cancelable = false;
    work->status = key("settings.system.icons.working");
    work->waitNote = key("settings.system.icons.wait");
    work->onEnter = [f](ui::Wizard &) {
        const IconStart r = f->svc->startIconDownload();   // Classic: App::startAssetDownload()
        switch (r.kind) {
            case IconStart::Kind::Started: case IconStart::Kind::AlreadyRunning: f->start = Flow::Start::Running; break;
            case IconStart::Kind::AllReady: f->start = Flow::Start::AllReady; break;
            case IconStart::Kind::Failed: f->start = Flow::Start::Failed; f->error = r.error; break;
        }
    };
    work->onUpdate = [f](float, ui::Wizard &w) {
        if (f->start == Flow::Start::Failed) { w.go(kFailed); return true; }
        if (f->start == Flow::Start::AllReady) { w.go(kDone); return true; }
        const IconDownloadInfo i = f->svc->icons();
        if (i.running) return false;
        if (i.failed) { f->error = i.error; w.go(kFailed); }
        else w.go(kDone);
        return true;
    };
    flow->add(kWork, std::move(work));
    flow->add(kDone, sysflow::resultPage(title(), [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Done;
        const bool nothing = f->start == Flow::Start::AllReady;
        r.headline = key(nothing ? "settings.system.icons.ready.headline" : "settings.system.icons.done.headline");
        r.changed = key(nothing ? "settings.system.icons.ready.changed" : "settings.system.icons.done.changed");
    }));
    flow->add(kFailed, sysflow::resultPage(title(), [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Error;
        r.headline = key("settings.system.icons.failed.headline");
        r.changed = key("settings.system.icons.failed.changed");
        r.next = key("settings.system.icons.failed.next");
        r.details = ui::Text::data(f->error);
        r.retry = kWork;
    }));
    return flow;
}

FlowPtr newFlow(SystemService &service) {
    FlowPtr f = std::make_shared<Flow>();
    f->svc = &service;
    return f;
}

}  // namespace

void openIconsFlow(SystemService &service, PageStack &pages) {
    WizardFlowPage::launch(pages, makeIconsFlow(newFlow(service)), kWork);
}

bool openIconsStep(SystemService &service, PageStack &pages, const std::string &step) {
    FlowPtr f = newFlow(service);
    ui::PageId first = kWork;
    if (step == "done") first = kDone;
    else if (step == "ready") { f->start = Flow::Start::AllReady; first = kDone; }
    else if (step == "failed") { f->error = "failed to download nes: Could not resolve host"; first = kFailed; }
    else if (!step.empty()) return false;
    WizardFlowPage::launch(pages, makeIconsFlow(f), first);
    return true;
}

void registerIconsStrings(ui::Catalog &c) {
    c.set("settings.system.icons.title", "Download missing icons");
    c.set("settings.system.icons.working", "Downloading icons");
    c.set("settings.system.icons.wait", "The icons are downloaded one by one. A slow Internet connection takes longer. Nothing is stuck.");
    c.set("settings.system.icons.done.headline", "Icons downloaded");
    c.set("settings.system.icons.done.changed", "The console and navigation icons are ready. Existing icons were left alone.");
    c.set("settings.system.icons.ready.headline", "All icons are ready");
    c.set("settings.system.icons.ready.changed", "Nothing needed to be downloaded.");
    c.set("settings.system.icons.failed.headline", "Could not download the icons");
    c.set("settings.system.icons.failed.changed", "Icons that were already there are unchanged.");
    c.set("settings.system.icons.failed.next", "Check the Internet connection and the SD card, then try again.");
}

}  // namespace patsgui
