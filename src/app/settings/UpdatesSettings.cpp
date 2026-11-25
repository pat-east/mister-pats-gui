#include "app/settings/UpdatesSettings.h"

#include "app/settings/StandardPages.h"
#include "app/settings/updates/ChangelogPage.h"

namespace patsgui {
namespace {

using namespace menu;

// The Classic rows of this category, in Classic's order (SettingsScreen::buildRows() 625-628 + buildUpdateRows() 833-891).
const MenuCategory &table() {
    static const MenuCategory category = [] {
        MenuCategory updates{"updates", "Updates", Cond::Always, {}};
        updates.rows = {
            toggle("updates.auto", "Check for updates automatically",
                   "When on, check GitHub for a newer GUI after startup once Internet is available.", false),
            info("updates.running", "Version running now", "The version currently open on MiSTer."),
            info("updates.next", "Version used on next start", "The version MiSTer will open after the next restart."),
            shown(info("updates.newest", "Newest installed version", "The newest GUI version already stored on this MiSTer."),
                  Cond::PinnedNewer),
            shown(because(enabledWhen(action("updates.usenewest", "Use newest installed version",
                                     "Remove the version pin so MiSTer uses the newest installed GUI next time.",
                                     "Classic: removes the pin at once, then the restart prompt"),
                              Cond::UseNewestAllowed), "An update is being checked, downloaded or installed."),
                  Cond::Pinned),
            shown(dynamic(info("updates.statuswait", nullptr, nullptr), Dyn::StatusText), Cond::PhaseWaiting),
            shown(info("updates.progress", "Update progress", "Shows how much of the update has downloaded or been installed."),
                  Cond::PhaseBusy),
            // Classic draws this row disabled, without a value and without an action (SS:869); B cancels the download.
            shown(because(enabledWhen(info("updates.cancel", "Cancel update download",
                                     "Stop downloading this update. The currently installed version stays in place."),
                              Cond::Never), "Press {B} to cancel the download."),
                  Cond::PhaseCancelable),
            shown(dynamic(info("updates.statusrestart", nullptr, nullptr), Dyn::StatusText), Cond::PhaseRestart),
            shown(action("updates.restart", "Restart options",
                         "Choose whether to restart now or keep using the current version for now.",
                         "Classic: the restart prompt (A restart, B later, X use newest and restart)"),
                  Cond::PhaseRestartNoPrompt),
            shown(action("updates.check", "Check for updates now", "Ask GitHub whether a newer GUI version is available.",
                         "Classic: starts the check; the result appears as rows below"),
                  Cond::PhaseCheckable),
            shown(dynamic(action("updates.install", nullptr,
                                 "Download and verify this version. You can choose when to restart afterward.",
                                 "Classic: release-notes confirmation (A install, B cancel, Up/Down read), then the download"),
                          Dyn::InstallTarget),
                  Cond::PhaseAvailInstall),
            shown(dynamic(info("updates.statusmisc", nullptr, nullptr), Dyn::StatusText), Cond::PhaseStatusLine),
            shown(dynamic(info("updates.repair", nullptr, nullptr), Dyn::RepairPaths), Cond::PhaseBlocked),
            shown(dynamic(info("updates.statuscurrent", nullptr, nullptr), Dyn::StatusText), Cond::PhaseCurrent),
            // Moved here from System (owner, 2026-11-24): the change history belongs to the updates.
            action("updates.changelog", "Read full changelog",
                   "Read the complete project change history from GitHub. An Internet connection is required.",
                   "Classic: System row; scrolling changelog viewer with load and retry"),
        };
        return updates;
    }();
    return category;
}

void strings(ui::Catalog &c) {
    registerTable(c, table());
    c.set("settings.hint.cancelDownload", "Cancel download");   // Classic: "B Cancel download" while an update downloads
    c.set("settings.updates.installTarget", "Install v{1} update");
    c.set("settings.updates.changelog.title", "Changelog");
    c.set("settings.updates.changelog.subtitle", "Full project changelog from GitHub");
    c.set("settings.updates.changelog.loading", "Loading the changelog");
    c.set("settings.updates.changelog.error", "Could not load the changelog");
    c.set("settings.updates.changelog.busy", "Another online task is running. Try again when it finishes.");
    c.set("settings.value.running", "v{1}");
    c.set("settings.value.pinned", "Pinned v{1}");
    c.set("settings.value.newestV", "Newest v{1}");
    c.set("settings.updates.progress.verifying", "Checking downloaded files\xE2\x80\xA6");   // Classic: buildUpdateRows
    c.set("settings.updates.progress.installing", "Installing update\xE2\x80\xA6");
    c.set("settings.updates.progress.percent", "{1}%");
    c.set("settings.updates.progress.downloaded", "Downloaded {1}");
    registerUpdatePageStrings(c);
}

bool downloading(const MenuState &s) { return s.phase == UpdatePhase::Downloading || s.phase == UpdatePhase::Verifying; }

}  // namespace

std::vector<VisibleRow> UpdatesSettings::rows(const MenuState &state) const { return visibleRows(table(), state); }

// The live texts of the info rows (Classic: buildUpdateRows value lambdas, SS:834-867).
ui::Text UpdatesSettings::valueOf(const VisibleRow &r, const MenuState &state, int current) const {
    const std::string id = r.row->id;
    if (id == "updates.running") return ui::Text::data(ui::format("settings.value.running", {state.running}));
    if (id == "updates.next")
        return ui::Text::data(ui::format(state.pinned ? "settings.value.pinned" : "settings.value.newestV", {state.selected}));
    if (id == "updates.newest") return ui::Text::data(ui::format("settings.value.running", {state.newest}));
    if (id == "updates.progress") return ui::Text::data(state.progress);
    return SettingsCategory::valueOf(r, state, current);
}

// "Check for updates automatically" shows Preferences::checkForUpdates() (SS:625-627).
int UpdatesSettings::current(const std::string &rowId) const {
    UpdatesService *s = service();
    if (s && rowId == "updates.auto") return s->checkAutomatically() ? 1 : 0;
    return kNoValue;
}

// App.cpp:275-279: setCheckForUpdates(!enabled); turning it off also cancels a running automatic check.
void UpdatesSettings::changed(const std::string &rowId, int value) {
    if (UpdatesService *s = service())
        if (rowId == "updates.auto") s->setCheckAutomatically(value != 0);
}

// Classic (App::dispatch 990-994): while an update downloads or verifies, B cancels it wherever the cursor is.
std::string UpdatesSettings::backActivates() const {
    return attached() && downloading(env().state) ? "updates.cancel" : std::string();
}

UpdatePageEnv UpdatesSettings::pageEnv(bool forced, bool *openFlag) {
    UpdatePageEnv e;
    e.service = service();
    e.info = [this]() -> const UpdateInfo & { return info_; };
    if (openFlag) e.closed = [openFlag]() { *openFlag = false; };
    e.forced = forced;
    return e;
}

// The dialogs are a function of the snapshot (SettingsScreen::render/handle): opened when it says Confirming, or
// RestartRequired with the prompt visible. They close themselves when it stops saying so.
bool UpdatesSettings::syncPages(PageStack &pages) {
    bool opened = false;
    if (info_.phase == UpdatePhase::Confirming && !confirmOpen_) {
        confirmOpen_ = true;
        openUpdateFlow(pages, pageEnv(false, &confirmOpen_), UpdateFlowStart::Confirm);
        opened = true;
    }
    const bool busy = info_.phase == UpdatePhase::Downloading || info_.phase == UpdatePhase::Verifying || info_.phase == UpdatePhase::Installing;
    if (busy && !confirmOpen_) {
        confirmOpen_ = true;
        openUpdateFlow(pages, pageEnv(false, &confirmOpen_), UpdateFlowStart::Busy);
        opened = true;
    }
    if (info_.phase == UpdatePhase::RestartRequired && info_.successPromptVisible && !confirmOpen_) {
        confirmOpen_ = true;
        openUpdateFlow(pages, pageEnv(false, &confirmOpen_), UpdateFlowStart::Restart);
        opened = true;
    }
    return opened;
}

bool UpdatesSettings::activate(const std::string &rowId, PageStack &pages) {
    UpdatesService *s = service();
    if (!s) return false;
    if (rowId == "updates.changelog") {      // SettingsScreen onShowChangelog_(): false while another online task runs
        if (!s->startChangelog()) {
            pages.toast(ui::Text::key("settings.updates.changelog.busy"));
            return true;
        }
        pages.push(makeChangelogPage(*s));
        return true;
    }
    if (rowId == "updates.check") {          // App.cpp:280: autoCheckDecisionMade_ = true; startCheck(false)
        s->startCheck();
        refresh(0.0f);
        return true;
    }
    if (rowId == "updates.install") {        // App.cpp:281: beginConfirmation(); the dialog is the snapshot's Confirming phase
        s->beginConfirmation();
        refresh(0.0f);
        syncPages(pages);
        return true;
    }
    if (rowId == "updates.usenewest") {      // App::removePinnedVersion (App.cpp:629-633)
        std::string error;
        if (!s->removePinnedVersion(error)) openUpdateFlow(pages, pageEnv(false, nullptr), UpdateFlowStart::PinError, error);
        refresh(0.0f);
        syncPages(pages);
        return true;
    }
    if (rowId == "updates.restart") {        // App.cpp:285: showInstallSuccess()
        s->showInstallSuccess();
        refresh(0.0f);
        syncPages(pages);
        return true;
    }
    if (rowId == "updates.cancel") {         // B while downloading (App.cpp:990-994): updateService_.cancelBeforeCommit()
        s->cancelBeforeCommit();
        refresh(0.0f);
        return true;
    }
    return false;
}

// Classic's App::update hands the UpdateSnapshot to SettingsScreen::setUpdateSnapshot() every frame; here the category
// copies it into the shared MenuState that the row conditions read. Nothing is read while the host has no service.
bool UpdatesSettings::refresh(float deltaSeconds) {
    UpdatesService *s = service();
    if (!s || !attached()) return false;
    s->update(deltaSeconds);
    const UpdateInfo info = s->snapshot();
    MenuState &st = env().state;
    std::string progress;   // SS:861-866
    if (info.phase == UpdatePhase::Verifying) progress = ui::Catalog::get().tr("settings.updates.progress.verifying");
    else if (info.phase == UpdatePhase::Installing) progress = ui::Catalog::get().tr("settings.updates.progress.installing");
    else if (info.bytesTotal)
        progress = ui::format("settings.updates.progress.percent", {std::to_string(info.bytesDone * 100 / info.bytesTotal)});
    else progress = ui::format("settings.updates.progress.downloaded", {info.bytesDoneText});
    const bool changed = st.phase != info.phase || st.pinned != info.pinned || st.canInstall != info.canInstall ||
                         st.restartPromptVisible != info.successPromptVisible || st.running != info.runningVersion ||
                         st.selected != info.selectedVersion || st.newest != info.newestInstalledVersion ||
                         st.target != info.targetVersion || st.statusText != info.statusText ||
                         st.repairPaths != info.repairPaths || st.progress != progress ||
                         info_.pinRemoved != info.pinRemoved || info_.databaseRebuildHint != info.databaseRebuildHint;
    st.phase = info.phase;
    st.pinned = info.pinned;
    st.canInstall = info.canInstall;
    st.restartPromptVisible = info.successPromptVisible;
    st.running = info.runningVersion;
    st.selected = info.selectedVersion;
    st.newest = info.newestInstalledVersion;
    st.target = info.targetVersion;
    st.statusText = info.statusText;
    st.repairPaths = info.repairPaths;
    st.progress = progress;
    info_ = info;
    return changed;
}

bool UpdatesSettings::update(float deltaSeconds) {
    const bool changed = refresh(deltaSeconds);
    if (!attached() || !service()) return changed;
    return syncPages(env().pages) || changed;
}

// Not shown: the snapshot is still polled (Classic polls it on every tab: the bottom bar, the restart prompt).
bool UpdatesSettings::background(float deltaSeconds) { return refresh(deltaSeconds); }

void UpdatesSettings::onShow() { update(0.0f); }

// Classic (App.cpp:1490-1494): the restart prompt switches to Settings wherever the user is.
bool UpdatesSettings::wantsAttention() const {
    return service() && info_.phase == UpdatePhase::RestartRequired && info_.successPromptVisible;
}

// Classic (App.cpp:1281-1288): "v<version>  ·  v<target> available" in the bottom bar while an update can be installed.
ui::Text UpdatesSettings::footerNote() const {
    if (!service() || info_.phase != UpdatePhase::Available || !info_.canInstall) return ui::Text();
    return ui::Text::data(ui::format("settings.updates.footer.available", {info_.targetVersion}));
}

// Classic (SettingsScreen::hints, SS:2821-2823): "B Cancel download   ←/→ Change column   LB/RB Change tab"; the tab
// takes the B hint of this hook and keeps the rest of its own line.
bool UpdatesSettings::hintsHook(std::vector<ui::Hint> &hints) const {
    if (!attached() || !downloading(env().state)) return false;
    hints.push_back(ui::hint(ui::HintKey::B, "settings.hint.cancelDownload", true));
    return true;
}

// The host runner's `--page updates/<id>`: the same pages a row opens, kept whatever the (sample) snapshot says.
bool UpdatesSettings::openPage(const std::string &pageId, PageStack &pages) {
    if (!attached()) return false;
    if (pageId == "confirm" || pageId == "restart" || pageId == "busy") {
        if (confirmOpen_) return true;
        confirmOpen_ = true;
        openUpdateFlow(pages, pageEnv(true, &confirmOpen_),
                       pageId == "confirm" ? UpdateFlowStart::Confirm : pageId == "busy" ? UpdateFlowStart::Busy : UpdateFlowStart::Restart);
        return true;
    }
    if (pageId == "changelog") {
        UpdatesService *s = service();
        if (!s || !s->startChangelog()) return false;
        pages.push(makeChangelogPage(*s));
        return true;
    }
    if (pageId == "error.restart") {
        openUpdateFlow(pages, pageEnv(true, nullptr), UpdateFlowStart::RestartError, "startup selection changed; review the restart destination");
        return true;
    }
    if (pageId == "error.pin") {
        openUpdateFlow(pages, pageEnv(true, nullptr), UpdateFlowStart::PinError, "version pin changed; review the startup selection");
        return true;
    }
    return false;
}

PATSGUI_REGISTER_SETTINGS_CATEGORY(UpdatesSettings, strings);

}  // namespace patsgui
