#pragma once

// The value rules of the Library rows and pages, as plain functions (no framework, no service) so a test can
// compile them alone and compare them with Classic's lambdas (src/legacy/SettingsScreen.cpp:662-735, ScanScreen.cpp,
// SettingsScreen.cpp:1456-1568). Header-only.

#include <cstdint>
#include <cstdio>
#include <string>

#include "app/settings/LibraryService.h"

namespace patsgui {
namespace library {

// ---- ScreenScraper cycles: the option lists of the table, in Classic's cycle order.
inline const int *volumes(int &count) { static const int v[] = {0, 100, 75, 50, 25, 10}; count = 6; return v; }
inline const int *storageLimits(int &count) { static const int v[] = {256, 512, 1024, 2048, 4096, 8192, 0}; count = 7; return v; }

// The option index of a stored value. Classic searches `while (index + 1 < count && values[index] != current) ++index`,
// so a value that is not in the list lands on the LAST element and the next press wraps to the first.
inline int indexIn(const int *values, int count, int current) {
    int index = 0;
    while (index + 1 < count && values[index] != current) ++index;
    return index;
}
inline int volumeIndex(int percent) { int n; const int *v = volumes(n); return indexIn(v, n, percent); }
inline int volumeAt(int index) { int n; const int *v = volumes(n); return v[((index % n) + n) % n]; }
inline int storageIndex(int megabytes) { int n; const int *v = storageLimits(n); return indexIn(v, n, megabytes); }
inline int storageAt(int index) { int n; const int *v = storageLimits(n); return v[((index % n) + n) % n]; }
// Classic: region == "eu" ? "us" : region == "us" ? "jp" : "eu"; the displayed text maps anything else to EU.
inline int regionIndex(const std::string &region) { return region == "us" ? 1 : region == "jp" ? 2 : 0; }
inline const char *regionAt(int index) { return index == 1 ? "us" : index == 2 ? "jp" : "eu"; }
// Classic: delay + 5, above 30 back to 5. Options are 5..30, index 0..5.
inline int delayIndex(int seconds) { int i = seconds / 5 - 1; return i < 0 ? 0 : i > 5 ? 5 : i; }
inline int delayAt(int index) { return (((index % 6) + 6) % 6 + 1) * 5; }

// ---- Row texts (SettingsScreen.cpp:663-726); the option words come from the row tables (LibrarySettings.cpp)
// Account row (SettingsScreen.cpp:676-681).
inline std::string accountValueText(const ScreenScraperAccount &account) {
    switch (account.state) {
    case ScreenScraperAccount::State::Unknown: return "Not checked";
    case ScreenScraperAccount::State::Checking: return "Checking...";
    default: return account.detail.empty() ? std::string("Not available") : account.detail;
    }
}
inline std::string userText(const std::string &user) { return user.empty() ? std::string("Not set") : user; }
inline std::string passwordText(bool set) { return set ? "********" : "Not set"; }

// ---- Core updater texts (SettingsScreen.cpp:1368-1568)
// "1.5 MB" from 1 MiB up, else "<n> KB" rounded up.
inline std::string sizeText(uint64_t bytes) {
    char buffer[32];
    if (bytes >= 1024ull * 1024) std::snprintf(buffer, sizeof(buffer), "%.1f MB", double(bytes) / (1024.0 * 1024.0));
    else std::snprintf(buffer, sizeof(buffer), "%llu KB", (unsigned long long)((bytes + 1023) / 1024));
    return buffer;
}
// "CPS2_20260501.rbf" -> "CPS2", "1944.mra" -> "1944"
inline std::string friendlyName(const std::string &file) {
    std::string name = file;
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name.erase(dot);
    const size_t us = name.find_last_of('_');
    if (us != std::string::npos && name.size() - us == 9 && name.find_first_not_of("0123456789", us + 1) == std::string::npos)
        name.erase(us);
    return name;
}
// Which entries a filter shows: 0 needs update (Missing, UpdateAvailable, Modified, Error), 1 all, 2 up to date, 3 changed.
inline bool filterShows(int filter, CoreMraFile::Status status) {
    using S = CoreMraFile::Status;
    if (filter == 0) return status == S::Missing || status == S::UpdateAvailable || status == S::Modified || status == S::Error;
    if (filter == 2) return status == S::UpToDate;
    if (filter == 3) return status == S::Modified;
    return true;
}
// A file can be picked when it is neither up to date nor unchecked (no checkbox otherwise).
inline bool selectable(CoreMraFile::Status status) {
    return status != CoreMraFile::Status::UpToDate && status != CoreMraFile::Status::Error;
}

}  // namespace library
}  // namespace patsgui
