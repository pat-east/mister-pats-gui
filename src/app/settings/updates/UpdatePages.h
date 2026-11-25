#pragma once

// The two dialogs of the update flow and the error dialog of its two failures. Classic draws them as modals of
// SettingsScreen that are a FUNCTION OF THE SNAPSHOT, not of local state (SettingsScreen::render, handle(), hints()):
//   phase == Confirming                              the release-notes confirmation   (renderUpdateConfirmation, SS:2520)
//   phase == RestartRequired && successPromptVisible the restart prompt               (renderUpdateSuccess, SS:2641)
// Both pages here therefore close themselves when the snapshot no longer says so (unless `forced`, the host runner's
// `--page`), and the category opens them from the snapshot (UpdatesSettings::syncPages).

#include <functional>
#include <memory>
#include <string>

#include "app/settings/SettingsPage.h"
#include "app/settings/UpdatesService.h"

namespace patsgui {

struct UpdatePageEnv {
    UpdatesService *service = nullptr;
    std::function<const UpdateInfo &()> info;   // the category's cached snapshot (updated once per frame)
    std::function<void()> closed;               // the page left the stack (any way): the category forgets it
    bool forced = false;                        // keep the page whatever the snapshot says (host runner)
};

// Opens the update flow (one Medium wizard frame) on `pages`. Install/restart pages follow the snapshot and close themselves
// when it stops saying so, unless env.forced (the host runner's --page). `error` is the raw service text of the failure
// for the error starts (shown only as Details).
enum class UpdateFlowStart { Confirm, Busy, Restart, RestartError, PinError };
void openUpdateFlow(PageStack &pages, const UpdatePageEnv &env, UpdateFlowStart start, const std::string &error = std::string());

// The texts of the pages.
void registerUpdatePageStrings(ui::Catalog &c);

}  // namespace patsgui
