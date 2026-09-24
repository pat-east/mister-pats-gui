#include "UpdateCheck.h"

#include <fstream>
#include <iterator>
#include <unistd.h>

#include "DebugLog.h"
#include "Downloader.h"

namespace {

constexpr const char *kUrl =
    "https://api.github.com/repos/pat-east/mister-pats-gui/releases/latest";
constexpr const char *kTempFile = "/tmp/mister-pat-update-check.json";

// Just the one field, out of everything GitHub's release API returns — not worth a JSON
// parser for. Looks for "tag_name":"..." however the surrounding whitespace happens to fall.
std::string extractTag(const std::string &json) {
    const std::string key = "\"tag_name\"";
    const size_t keyPos = json.find(key);
    if (keyPos == std::string::npos) return {};

    const size_t colon = json.find(':', keyPos + key.size());
    if (colon == std::string::npos) return {};

    const size_t firstQuote = json.find('"', colon + 1);
    if (firstQuote == std::string::npos) return {};
    const size_t secondQuote = json.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) return {};

    return json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

} // namespace

void UpdateCheck::run(const std::string &currentVersion) {
    currentVersion_ = currentVersion;
    checked_ = true;

    Downloader downloader;
    if (!downloader.fetch(kUrl, kTempFile, 5)) {
        DebugLog::info("update check: could not reach GitHub");
        unlink(kTempFile);
        return;
    }

    std::ifstream in(kTempFile);
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    unlink(kTempFile);

    std::string tag = extractTag(json);
    if (!tag.empty() && tag[0] == 'v') tag = tag.substr(1);   // "v0.1.5" -> "0.1.5"

    if (tag.empty()) {
        DebugLog::info("update check: could not read the release tag");
        return;
    }

    latestVersion_ = tag;
    if (available())
        DebugLog::info("update check: v" + tag + " is available (running v" + currentVersion_ + ")");
    else
        DebugLog::info("update check: already on the latest version");
}
