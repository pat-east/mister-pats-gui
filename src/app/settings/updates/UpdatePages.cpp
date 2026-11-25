// The GUI update as a Wizard flow (ui::Wizard through WizardFlowPage), one Medium frame for all pages. Classic's state
// machine (src/legacy/SettingsScreen.cpp: renderUpdateConfirmation 2520, renderUpdateSuccess 2641, handle() 1131-1151,
// hints() 2813-2824; App.cpp: restartMisterAfterUpdate, removePinnedVersion) is unchanged: the pages are a FUNCTION OF THE
// SNAPSHOT (the service's phase), and close themselves when it stops saying so.
//
//   Confirming ---A (startInstall)---> Downloading / Verifying / Installing (one Busy page) --RestartRequired--> Restart prompt
//        \-B (cancelBeforeCommit)-> close      B while Downloading/Verifying (cancelBeforeCommit) -> "Stopping" -> close
//   Restart prompt: Restart now (A), Later (B = dismissInstallSuccess), Use newest version and restart (pinned only).
//   Failures: restart fails -> Result (Try again = the prompt); pin removal fails -> Result (Try again = removes again);
//             the update ends in phase Error -> Result.

#include "app/settings/updates/UpdatePages.h"

#include "app/settings/StandardPages.h"
#include "app/settings/WizardFlowPage.h"
#include "ui-framework/kit/Primitives.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

void registerUpdatePageStrings(ui::Catalog &c) {
    c.set("settings.updates.flow.title", "GUI update");
    // Confirm (Classic: renderUpdateConfirmation)
    c.set("settings.updates.confirm.title", "Install v{1}?");
    c.set("settings.updates.confirm.pinned", "The version pin stays active after download.");
    c.set("settings.updates.confirm.restart", "Restart MiSTer after installation to use the update.");
    c.set("settings.updates.confirm.rebuild", "The game list needs rebuilding after restart.");
    c.set("settings.updates.confirm.notes", "Release notes");
    c.set("settings.updates.hint.install", "Install");
    // Busy
    c.set("settings.updates.busy.download", "Downloading the update");
    c.set("settings.updates.busy.verify", "Checking the downloaded files");
    c.set("settings.updates.busy.install", "Installing the update");
    c.set("settings.updates.busy.stopping", "Stopping the download");
    c.set("settings.updates.busy.power", "Keep MiSTer powered on.");
    c.set("settings.updates.busy.wait.download", "The update is downloaded from the Internet. A slow connection takes longer. Nothing is stuck.");
    c.set("settings.updates.busy.wait.verify", "The download is checked against its fingerprint before anything is installed. Nothing is stuck.");
    c.set("settings.updates.busy.wait.install", "The files are being replaced on the SD card. Nothing is stuck; do not switch the MiSTer off.");
    c.set("settings.updates.busy.pin", "Removing the version pin");
    // Restart prompt (Classic: renderUpdateSuccess)
    c.set("settings.updates.prompt.word", "Done");
    c.set("settings.updates.prompt.title", "v{1} installed");
    c.set("settings.updates.prompt.titleRemoved", "Version pin removed");
    c.set("settings.updates.prompt.text", "The verified GUI library is ready. Choose what happens next.");
    c.set("settings.updates.prompt.textRemoved", "The newest installed GUI is now selected for the next start.");
    c.set("settings.updates.prompt.restart", "Restart now");
    c.set("settings.updates.prompt.starts", "Starts v{1}.");
    c.set("settings.updates.prompt.startsPinned", "Starts v{1} (version pin stays active).");
    c.set("settings.updates.prompt.later", "Later");
    c.set("settings.updates.prompt.continues", "Continue running v{1}.");
    c.set("settings.updates.prompt.removePin", "Use newest version and restart");
    c.set("settings.updates.prompt.startsNewest", "Starts the newest installed library, v{1}.");
    // Failures (Classic: showError "Cannot restart MiSTer", "Cannot remove version pin", the Error phase)
    c.set("settings.updates.err.restart", "MiSTer could not restart");
    c.set("settings.updates.err.restart.changed", "Nothing was changed. MiSTer was not restarted and keeps running v{1}.");
    c.set("settings.updates.err.restart.next", "Try again, or choose Later and restart from Settings \xE2\x86\x92 Updates when you are ready.");
    c.set("settings.updates.err.pin", "The version pin was not removed");
    c.set("settings.updates.err.pin.changed", "Nothing was changed. The version pin is still active.");
    c.set("settings.updates.err.pin.next", "Try again, or leave the pin as it is.");
    c.set("settings.updates.err.update", "The update could not be installed");
    c.set("settings.updates.err.update.changed", "Nothing was changed. The installed version stays in place.");
    c.set("settings.updates.err.update.next", "Check your Internet connection, then check for updates again.");
    // The footer note (Classic: the bottom bar's "v<running>  ·  v<target> available", App.cpp:1281-1288)
    c.set("settings.updates.footer.available", "v{1} available");
}

namespace {

enum { kConfirm = 0, kBusy = 1, kRestart = 2, kRestartError = 3, kPinError = 4, kPinTry = 5, kFailed = 6 };

bool busyPhase(UpdatePhase p) { return p == UpdatePhase::Downloading || p == UpdatePhase::Verifying || p == UpdatePhase::Installing; }
bool promptShown(const UpdateInfo &i) { return i.phase == UpdatePhase::RestartRequired && i.successPromptVisible; }

// The flow's data: the snapshot as of this frame (read from the service itself, so a command a page just gave is seen at once),
// the raw text of the last failure.
struct State {
    UpdatePageEnv env;
    UpdateInfo info;
    std::string error;
    bool stopping = false;
    void refresh() { info = env.service ? env.service->snapshot() : env.info(); }
};
using StatePtr = std::shared_ptr<State>;

// The release notes as paragraphs (Classic: lines starting with # are headings, ** toggles bold; here a heading is a bold
// paragraph, other consecutive lines are one paragraph, "- " becomes a bullet).
std::vector<ui::WizardParagraph> notesParagraphs(const std::string &notes) {
    std::vector<ui::WizardParagraph> out;
    std::string block;
    auto flush = [&] {
        if (!block.empty()) out.push_back({ui::Text::data(block), ui::TextTier::Primary, false});
        block.clear();
    };
    size_t start = 0;
    while (start <= notes.size()) {
        const size_t end = notes.find('\n', start);
        std::string line = notes.substr(start, end == std::string::npos ? end : end - start);
        start = end == std::string::npos ? notes.size() + 1 : end + 1;
        for (size_t at; (at = line.find("**")) != std::string::npos;) line.erase(at, 2);
        size_t hashes = 0;
        while (hashes < line.size() && line[hashes] == '#') ++hashes;
        const bool heading = hashes > 0 && hashes < line.size() && line[hashes] == ' ';
        if (heading) line = line.substr(hashes + 1);
        if (line.empty()) { flush(); continue; }
        if (heading) { flush(); out.push_back({ui::Text::data(line), ui::TextTier::Primary, true}); continue; }
        if (line.compare(0, 2, "- ") == 0) line = "\xE2\x80\xA2 " + line.substr(2);
        block += (block.empty() ? "" : "\n") + line;
    }
    flush();
    return out;
}

// ---- Confirm: only when the snapshot says Confirming (Classic: SettingsScreen::render 2726). Capture page: B is the page's
// own (cancelBeforeCommit), so closing the GUI does not cancel anything.
std::unique_ptr<ui::FnPage> makeConfirm(const StatePtr &st) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Capture, ui::Text::key("settings.updates.flow.title")));
    auto notes = std::make_shared<ui::WizardScrollText>();
    page->onEnter = [st, notes](ui::Wizard &) { st->refresh(); notes->set(notesParagraphs(st->info.releaseNotes)); };
    page->onDraw = [st, notes](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        const UpdateInfo &i = st->info;
        std::vector<ui::WizardParagraph> head = {{ui::Text::data(ui::format("settings.updates.confirm.title", {i.targetVersion})),
                                                  ui::TextTier::Primary, true},
                                                 {ui::Text::key(i.pinned ? "settings.updates.confirm.pinned" : "settings.updates.confirm.restart"),
                                                  ui::TextTier::Muted, false}};
        if (i.databaseRebuildHint) head.push_back({ui::Text::key("settings.updates.confirm.rebuild"), ui::TextTier::Muted, false});
        int y = body.y + ui::drawWizardParagraphs(p, t, body, head);
        if (i.releaseNotes.empty() || y >= body.bottom()) return;
        y += t.px(14);
        const ui::Rect rest{body.x, y, body.w, body.bottom() - y};
        y += ui::drawWizardParagraphs(p, t, rest, {{ui::Text::key("settings.updates.confirm.notes"), ui::TextTier::Muted, true}}) + t.px(6);
        if (y < body.bottom()) notes->draw(p, t, {body.x, y, body.w, body.bottom() - y});
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::UpDown, "hint.scroll"), ui::hint(ui::HintKey::A, "settings.updates.hint.install", true),
                                     ui::hint(ui::HintKey::B, "hint.cancel", true)};
    };
    page->onEvent = [st, notes](ui::WizardEvent e, ui::Wizard &w) {
        UpdatesService *svc = st->env.service;
        switch (e) {
            case ui::WizardEvent::Confirm:
            case ui::WizardEvent::Start:   // App::dispatch: Start acts as A
                if (svc && svc->startInstall()) w.go(kBusy);   // onInstallUpdate_
                break;
            case ui::WizardEvent::Back:    // onCancelUpdate_
                if (svc) svc->cancelBeforeCommit();
                w.close();
                break;
            case ui::WizardEvent::Up:
            case ui::WizardEvent::Down:
            case ui::WizardEvent::TriggerPrev:
            case ui::WizardEvent::TriggerNext:
                notes->handle(e);
                break;
            default: break;   // tabs locked
        }
    };
    page->onUpdate = [st](float, ui::Wizard &w) {
        if (st->env.forced) return false;
        st->refresh();
        const UpdateInfo &i = st->info;
        if (i.phase == UpdatePhase::Confirming) return false;
        if (busyPhase(i.phase)) w.go(kBusy);
        else if (promptShown(i)) w.go(kRestart);
        else w.close();
        return true;
    };
    return page;
}

// ---- Downloading, verifying, installing: one centred Busy page, its words follow the phase.
std::unique_ptr<ui::BusyPage> makeBusy(const StatePtr &st) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(ui::Text::key("settings.updates.flow.title")));
    page->centered = true;
    ui::BusyPage *raw = page.get();
    auto fill = [st, raw] {
        const UpdateInfo &i = st->info;
        const bool installing = i.phase == UpdatePhase::Installing;
        raw->status = ui::Text::key(st->stopping ? "settings.updates.busy.stopping"
                                    : i.phase == UpdatePhase::Verifying ? "settings.updates.busy.verify"
                                    : installing ? "settings.updates.busy.install" : "settings.updates.busy.download");
        raw->waitNote = ui::Text::key(installing ? "settings.updates.busy.wait.install"
                                      : i.phase == UpdatePhase::Verifying ? "settings.updates.busy.wait.verify"
                                      : "settings.updates.busy.wait.download");
        raw->cancelable = !st->stopping && !installing;
        raw->fraction = i.phase == UpdatePhase::Downloading && i.bytesTotal ? float(i.bytesDone) / float(i.bytesTotal) : -1.0f;
        if (st->stopping) { raw->detail = ui::Text(); return; }
        if (installing) { raw->detail = ui::Text::key("settings.updates.busy.power"); return; }
        if (i.phase != UpdatePhase::Downloading) { raw->detail = ui::Text(); return; }
        raw->setDetail(ui::Text::data(i.bytesTotal ? ui::format("settings.updates.progress.percent", {std::to_string(i.bytesDone * 100 / i.bytesTotal)})
                                                   : ui::format("settings.updates.progress.downloaded", {i.bytesDoneText})));
    };
    page->onEnter = [st, raw, fill](ui::Wizard &) {
        st->refresh();
        st->stopping = false;
        raw->detail = ui::Text();
        fill();
    };
    // B while Downloading/Verifying: cancelBeforeCommit(); the page ends when the snapshot leaves the busy phases.
    page->onCancel = [st, raw](ui::Wizard &) {
        if (st->env.service) st->env.service->cancelBeforeCommit();
        st->stopping = true;
        raw->cancelable = false;
    };
    page->onUpdate = [st, fill](float, ui::Wizard &w) {
        st->refresh();
        fill();
        if (st->env.forced) return true;
        const UpdateInfo &i = st->info;
        if (busyPhase(i.phase) || i.phase == UpdatePhase::Confirming) return true;   // Confirming: the service has not started yet
        if (promptShown(i)) w.go(kRestart);
        else if (i.phase == UpdatePhase::Error && !st->stopping) w.go(kFailed);
        else w.close();
        return true;
    };
    return page;
}

// The headline and the sentence of the restart prompt, top aligned, then the choices.
void drawHead(ui::Painter &p, const ui::Theme &t, const ui::Rect &col, int &y, const ui::Text &headline, const ui::Text &line) {
    const int headH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Heading));
    const int bodyH = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body));
    const int hl = std::max(1, std::min<int>(2, int(ui::wrapText(p, t, headline.str(), ui::TextRole::Heading, true, col.w).size())));
    const int ll = std::max(1, std::min<int>(3, int(ui::wrapText(p, t, line.str(), ui::TextRole::Body, false, col.w).size())));
    const int hb = hl * (headH + t.px(ui::Space::lineExtra)), lb = ll * (bodyH + t.px(ui::Space::lineExtra));
    ui::drawTextBlock(p, t, {col.x, y, col.w, hb}, headline.str(), ui::TextRole::Heading, ui::TextTier::Primary, true, 2);
    y += hb + t.px(8);
    ui::drawTextBlock(p, t, {col.x, y, col.w, lb}, line.str(), ui::TextRole::Body, ui::TextTier::Muted, false, 3);
    y += lb + t.px(20);
}

// ---- Restart prompt: phase RestartRequired and successPromptVisible (SettingsScreen::render 2728). The choices are rows;
// the words of the rows and of the hints are the same (A Choose, B Later). Capture page: Later (B) is the page's own.
std::unique_ptr<ui::FnPage> makeRestart(const StatePtr &st) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Capture, ui::Text::key("settings.updates.flow.title")));
    page->tone_ = ui::Tone::Done;
    struct Data {
        ui::ChooseList list{ui::ChooseMode::Single};
        std::string signature;
    };
    auto data = std::make_shared<Data>();
    auto signature = [](const UpdateInfo &i) {
        return i.targetVersion + "|" + i.selectedVersion + "|" + i.runningVersion + "|" + i.newestInstalledVersion + "|" +
               (i.pinned ? "1" : "0") + (i.pinRemoved ? "1" : "0");
    };
    auto rebuild = [st, data, signature] {
        const UpdateInfo &i = st->info;
        data->signature = signature(i);
        std::vector<ui::ChooseItem> items(2);
        items[0].title = ui::Text::key("settings.updates.prompt.restart");
        items[0].detail = ui::Text::data(ui::format(i.pinned ? "settings.updates.prompt.startsPinned" : "settings.updates.prompt.starts", {i.selectedVersion}));
        items[1].title = ui::Text::key("settings.updates.prompt.later");
        items[1].detail = ui::Text::data(ui::format("settings.updates.prompt.continues", {i.runningVersion}));
        if (i.pinned) {
            ui::ChooseItem it;
            it.title = ui::Text::key("settings.updates.prompt.removePin");
            it.detail = ui::Text::data(ui::format("settings.updates.prompt.startsNewest", {i.newestInstalledVersion}));
            items.push_back(it);
        }
        const int keep = data->list.cursor();
        data->list.setItems(items);
        data->list.setCursor(std::min<int>(keep, int(items.size()) - 1));
    };
    // App::restartMisterAfterUpdate: prepareRestart + sync + reboot, or the error page.
    auto restart = [st](bool removePin, ui::Wizard &w) {
        std::string error;
        if (!st->env.service || !st->env.service->restartAfterUpdate(removePin, error)) {
            st->error = error;
            w.go(kRestartError);
        }
    };
    auto later = [st](ui::Wizard &w) {   // onDismissInstallSuccess_
        if (st->env.service) st->env.service->dismissInstallSuccess();
        w.close();
    };
    page->onEnter = [st, data, rebuild](ui::Wizard &) { st->refresh(); data->list.setCursor(0); rebuild(); };
    page->onDraw = [st, data](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        const UpdateInfo &i = st->info;
        const ui::Rect col = ui::wizardTextColumn(t, body);
        int y = body.y + t.px(4);
        const int sh = ui::wizardStatusHeight(p, t);
        ui::drawWizardStatus(p, t, {col.x, y, col.w, sh}, ui::Tone::Done, ui::Text::key("settings.updates.prompt.word"), ui::Text());
        y += sh + t.px(10);
        drawHead(p, t, col, y,
                 i.pinRemoved ? ui::Text::key("settings.updates.prompt.titleRemoved")
                              : ui::Text::data(ui::format("settings.updates.prompt.title", {i.targetVersion})),
                 ui::Text::key(i.pinRemoved ? "settings.updates.prompt.textRemoved" : "settings.updates.prompt.text"));
        data->list.draw(p, t, {body.x, y, body.w, std::max(0, body.bottom() - y)});
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::UpDown, "hint.move"), ui::hint(ui::HintKey::A, "hint.choose", true),
                                     ui::hint(ui::HintKey::B, "hint.later", true)};
    };
    page->onEvent = [st, data, restart, later](ui::WizardEvent e, ui::Wizard &w) {
        switch (e) {
            case ui::WizardEvent::Start: restart(false, w); break;                       // App::dispatch turns Start into Confirm
            case ui::WizardEvent::Back: later(w); break;                                  // B
            case ui::WizardEvent::X: if (st->info.pinned) restart(true, w); break;        // FaceXPress: only when pinned
            default:
                if (data->list.handle(e) == ui::ChooseOutcome::Chosen) {
                    const int row = data->list.cursor();
                    if (row == 0) restart(false, w);
                    else if (row == 1) later(w);
                    else restart(true, w);
                }
        }
    };
    page->onUpdate = [st, data, rebuild, signature](float, ui::Wizard &w) {
        if (st->env.forced) return false;
        st->refresh();
        if (!promptShown(st->info)) { w.close(); return true; }
        if (signature(st->info) != data->signature) { rebuild(); return true; }   // e.g. "startup selection changed" refreshed the versions
        return false;
    };
    return page;
}

// ---- Failure pages: plain words, what changed, the next step; the service text is only the Details.
std::unique_ptr<ui::ResultPage> makeRestartError(const StatePtr &st) {
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(ui::Text::key("settings.updates.flow.title")));
    ui::ResultPage *raw = page.get();
    page->onEnter = [st, raw](ui::Wizard &) {
        st->refresh();
        ui::WizardResult r;
        r.tone = ui::Tone::Error;
        r.headline = ui::Text::key("settings.updates.err.restart");
        r.changed = ui::Text::data(ui::format("settings.updates.err.restart.changed", {st->info.runningVersion}));
        r.next = ui::Text::key("settings.updates.err.restart.next");
        r.details = ui::Text::data(st->error);
        r.retry = kRestart;
        raw->set(r);
    };
    return page;
}

std::unique_ptr<ui::ResultPage> makePinError(const StatePtr &st) {
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(ui::Text::key("settings.updates.flow.title")));
    ui::ResultPage *raw = page.get();
    page->onEnter = [st, raw](ui::Wizard &) {
        ui::WizardResult r;
        r.tone = ui::Tone::Error;
        r.headline = ui::Text::key("settings.updates.err.pin");
        r.changed = ui::Text::key("settings.updates.err.pin.changed");
        r.next = ui::Text::key("settings.updates.err.pin.next");
        r.details = ui::Text::data(st->error);
        r.retry = kPinTry;
        raw->set(r);
    };
    return page;
}

// Try again after a failed pin removal: App::removePinnedVersion once more (a Busy page that does it on its first frame).
std::unique_ptr<ui::BusyPage> makePinTry(const StatePtr &st) {
    std::unique_ptr<ui::BusyPage> page(new ui::BusyPage(ui::Text::key("settings.updates.flow.title")));
    page->centered = true;
    page->cancelable = false;
    page->status = ui::Text::key("settings.updates.busy.pin");
    page->showElapsed = false;   // done within one frame
    page->onUpdate = [st](float, ui::Wizard &w) {
        std::string error;
        if (st->env.service && st->env.service->removePinnedVersion(error)) {
            st->refresh();
            if (promptShown(st->info)) w.go(kRestart); else w.close();
        } else {
            st->error = error;
            w.go(kPinError);
        }
        return true;
    };
    return page;
}

std::unique_ptr<ui::ResultPage> makeFailed(const StatePtr &st) {
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(ui::Text::key("settings.updates.flow.title")));
    ui::ResultPage *raw = page.get();
    page->onEnter = [st, raw](ui::Wizard &) {
        st->refresh();
        ui::WizardResult r;
        r.tone = ui::Tone::Error;
        r.headline = ui::Text::key("settings.updates.err.update");
        r.changed = ui::Text::key("settings.updates.err.update.changed");
        r.next = ui::Text::key("settings.updates.err.update.next");
        r.details = ui::Text::data(st->info.errorText.empty() ? st->info.statusText : st->info.errorText);
        raw->set(r);
    };
    return page;
}

// The flow page: reads the snapshot afresh every frame (before the pages' update) and tells the category when it is gone.
class UpdateFlow : public WizardFlowPage {
public:
    UpdateFlow(const StatePtr &st) : WizardFlowPage(ui::WizardFrame::Medium), st_(st) {}
    bool update(float dt, PageStack &stack) override {
        st_->refresh();
        return WizardFlowPage::update(dt, stack);
    }
    void onClose() override {
        WizardFlowPage::onClose();
        if (st_->env.closed) st_->env.closed();
    }

private:
    StatePtr st_;
};

}  // namespace

void openUpdateFlow(PageStack &pages, const UpdatePageEnv &env, UpdateFlowStart start, const std::string &error) {
    StatePtr st(new State);
    st->env = env;
    st->error = error;
    st->refresh();
    std::unique_ptr<UpdateFlow> flow(new UpdateFlow(st));
    flow->add(kConfirm, makeConfirm(st));
    flow->add(kBusy, makeBusy(st));
    flow->add(kRestart, makeRestart(st));
    flow->add(kRestartError, makeRestartError(st));
    flow->add(kPinError, makePinError(st));
    flow->add(kPinTry, makePinTry(st));
    flow->add(kFailed, makeFailed(st));
    const ui::PageId first = start == UpdateFlowStart::Confirm ? kConfirm : start == UpdateFlowStart::Busy ? kBusy
                           : start == UpdateFlowStart::Restart ? kRestart : start == UpdateFlowStart::RestartError ? kRestartError : kPinError;
    WizardFlowPage::launch(pages, std::move(flow), first);
}

}  // namespace patsgui
