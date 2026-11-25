#pragma once

// Updates -> Read full changelog: Classic's viewer (SettingsScreen::renderChangelog / prepareChangelogLines and the
// changelog fields of UpdateSnapshot). An InfoPage viewer whose content follows UpdatesService::snapshot() every frame:
// Loading, Error (A tries again), the text (Markdown-lite, scrolls), or the empty line.

#include <memory>

#include "app/settings/SettingsPage.h"
#include "app/settings/UpdatesService.h"

namespace patsgui {

// `service.startChangelog()` must have returned true before the page is opened (Classic opens the viewer only then).
std::unique_ptr<SettingsPage> makeChangelogPage(UpdatesService &service);

}  // namespace patsgui
