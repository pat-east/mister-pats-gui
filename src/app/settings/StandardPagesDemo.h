#pragma once

// The standard pages with sample content, for the host runner only (`--tab settings --page standard/choice`):
// every helper page of StandardPages.h in each of its states, so the pages can be looked at and driven with
// --keys without a category. The texts are plain data, not catalog keys: nothing here ships to users.

#include <string>
#include <vector>

#include "app/settings/SettingsPage.h"

namespace patsgui {

// "choice", "confirm", "result.done", "result.error", "progress", "text", "text.masked", "facts", "viewer",
// "viewer.loading", "viewer.error".
std::vector<std::string> standardPageDemoIds();
// Opens the demo page `id` on the stack; false when there is no such page.
bool openStandardPageDemo(const std::string &id, PageStack &pages);

}  // namespace patsgui
