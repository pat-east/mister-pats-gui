#pragma once

#include <string>

// The ScreenScraper logins, kept in one small text file next to the other settings:
//
//   SCREENSCRAPER_DEVID="..."          the developer login of this software; only needed by a build
//                                      that does not carry it (see ScreenScraperDevLogin.h)
//   SCREENSCRAPER_DEVPASSWORD="..."
//   SCREENSCRAPER_SOFTNAME="..."
//   SCREENSCRAPER_SSID="..."           the user's own ScreenScraper account (set in Settings)
//   SCREENSCRAPER_SSPASSWORD="..."
//
// The file is not part of the settings backup, and its values are never logged or shown (the
// password is masked). Its permissions do not protect it on the FAT/exFAT card: anyone with SSH
// or FTP access to the MiSTer can read it, so a password used nowhere else is advised.
struct ScreenScraperCredentials {
    std::string devId, devPassword, softName, userId, userPassword;
    bool hasDeveloper() const { return !devId.empty() && !devPassword.empty() && !softName.empty(); }
    bool hasUser() const { return !userId.empty() && !userPassword.empty(); }
};

namespace ScreenScraperSettings {

// The name this software gives ScreenScraper. It is not a setting: an empty or missing
// SCREENSCRAPER_SOFTNAME in the file means this name, and a new file is written with it.
constexpr const char *kSoftName = "MiSTerPatsGUI";

std::string defaultPath();

// Reads the file; missing keys and a missing file give empty values.
ScreenScraperCredentials load(const std::string &path = defaultPath());

// Sets one key (for example "SCREENSCRAPER_SSID"), keeping every other line as it is, and
// writes the file atomically. Creates the file with all five keys when it does not exist.
// Only the five known keys are accepted. Quotes and backslashes in a value are escaped.
bool save(const std::string &key, const std::string &value,
          const std::string &path = defaultPath());

// Exposed for tests: one `KEY="value"` line, with the escapes undone.
bool parseLine(const std::string &line, std::string &key, std::string &value);

}  // namespace ScreenScraperSettings
