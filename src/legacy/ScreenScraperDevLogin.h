#pragma once

#include <string>

// The developer login (devid / devpassword) ScreenScraper requires from this software.
//
// It is not in the repository. A build that finds the git-ignored `_build/screenscraper.env` in the
// project folder bakes the two values into the binary, scrambled (tools/embed-dev-login.py), so
// `strings` does not show them; a build without that file has none. At run time they are
// unscrambled into ordinary variables in memory and sent with every request. They are never
// logged or written anywhere. Anyone willing to spend the time can still recover them from the
// binary, as with every scraper that ships its developer login.
namespace ScreenScraperDevLogin {

// True when this build carries a developer login; fills both values.
bool get(std::string &devId, std::string &devPassword);

}  // namespace ScreenScraperDevLogin
