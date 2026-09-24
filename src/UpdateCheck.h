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
    bool available() const { return !latestVersion_.empty() && latestVersion_ != currentVersion_; }
    const std::string &latestVersion() const { return latestVersion_; }

private:
    bool checked_ = false;
    std::string currentVersion_;
    std::string latestVersion_;
};
