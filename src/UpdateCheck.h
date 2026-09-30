#pragma once

#include <string>

// A one-shot, opt-in check against GitHub for a release newer than this build.
//
// The only network access in this project that is not something the user explicitly asked
// for in the moment (the box art scraper is a Settings button; this would run on its own),
// so it stays off unless turned on in Settings, and runs at most once per session.
//
// There is no background thread anywhere in this codebase, so run() blocks — bounded to a
// few seconds by Downloader's own timeout, meant to be called once from a quiet moment
// rather than from the frame loop itself.
class UpdateCheck {
public:
    void run(const std::string &currentVersion);

    bool checked() const { return checked_; }
    bool available() const { return isNewer(latestVersion_, currentVersion_); }

    // Whether `latest` is a later release than `current`, compared number by number ("0.10.0"
    // is later than "0.9.0"). Anything unparseable counts as not newer: a banner announcing an
    // "update" to an older or unreadable version is worse than no banner at all.
    static bool isNewer(const std::string &latest, const std::string &current);
    const std::string &latestVersion() const { return latestVersion_; }

private:
    bool checked_ = false;
    std::string currentVersion_;
    std::string latestVersion_;
};
