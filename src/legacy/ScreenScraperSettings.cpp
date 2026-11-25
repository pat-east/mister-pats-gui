#include "ScreenScraperSettings.h"

#include <fstream>
#include <sstream>
#include <vector>

#include "Paths.h"
#include "SafeFileWrite.h"
#include "ScreenScraperDevLogin.h"

namespace ScreenScraperSettings {
namespace {

const char *const kKeys[] = {"SCREENSCRAPER_DEVID", "SCREENSCRAPER_DEVPASSWORD",
                             "SCREENSCRAPER_SOFTNAME", "SCREENSCRAPER_SSID",
                             "SCREENSCRAPER_SSPASSWORD"};

bool knownKey(const std::string &key) {
    for (const char *k : kKeys) if (key == k) return true;
    return false;
}

std::string escape(const std::string &value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

}  // namespace

std::string defaultPath() { return MISTER_PAT_ROOT "/screenscraper-fr-settings.ini"; }

bool parseLine(const std::string &rawLine, std::string &key, std::string &value) {
    std::string line = rawLine;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
    if (start >= line.size() || line[start] == '#') return false;
    const size_t equals = line.find('=', start);
    if (equals == std::string::npos) return false;
    key = line.substr(start, equals - start);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
    std::string rest = line.substr(equals + 1);
    size_t a = 0;
    while (a < rest.size() && (rest[a] == ' ' || rest[a] == '\t')) ++a;
    rest = rest.substr(a);
    value.clear();
    if (!rest.empty() && rest[0] == '"') {
        for (size_t i = 1; i < rest.size(); ++i) {
            if (rest[i] == '\\' && i + 1 < rest.size()) { value += rest[++i]; continue; }
            if (rest[i] == '"') return true;
            value += rest[i];
        }
        return true;   // an unterminated quote keeps what was read
    }
    while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t')) rest.pop_back();
    value = rest;
    return true;
}

ScreenScraperCredentials load(const std::string &path) {
    ScreenScraperCredentials c;
    std::ifstream in(path);
    std::string line, key, value;
    while (std::getline(in, line)) {
        if (!parseLine(line, key, value)) continue;
        if (key == "SCREENSCRAPER_DEVID") c.devId = value;
        else if (key == "SCREENSCRAPER_DEVPASSWORD") c.devPassword = value;
        else if (key == "SCREENSCRAPER_SOFTNAME") c.softName = value;
        else if (key == "SCREENSCRAPER_SSID") c.userId = value;
        else if (key == "SCREENSCRAPER_SSPASSWORD") c.userPassword = value;
    }
    if (c.softName.empty()) c.softName = kSoftName;
    // A build that carries the developer login uses that one; the file's values are only the
    // way to supply it to a build that does not.
    std::string builtId, builtPassword;
    if (ScreenScraperDevLogin::get(builtId, builtPassword)) {
        c.devId = builtId;
        c.devPassword = builtPassword;
    }
    return c;
}

bool save(const std::string &key, const std::string &value, const std::string &path) {
    if (!knownKey(key)) return false;
    if (value.find_first_of("\r\n") != std::string::npos) return false;

    std::vector<std::string> lines;
    {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
    }
    const std::string replacement = key + "=\"" + escape(value) + "\"";
    bool replaced = false;
    for (std::string &line : lines) {
        std::string k, v;
        if (parseLine(line, k, v) && k == key) { line = replacement; replaced = true; }
    }
    if (lines.empty()) {
        lines.push_back("# ScreenScraper.fr logins. Not part of the settings backup; readable by anyone");
        lines.push_back("# who can reach this MiSTer over SSH or FTP, so use a password you use nowhere else.");
        for (const char *k : kKeys) {
            const std::string initial = std::string(k) == "SCREENSCRAPER_SOFTNAME" ? std::string(kSoftName) : std::string();
            lines.push_back(std::string(k) + "=\"" + (key == k ? escape(value) : initial) + "\"");
            if (key == k) replaced = true;
        }
    }
    if (!replaced) lines.push_back(replacement);

    std::ostringstream out;
    for (const std::string &line : lines) out << line << "\n";
    return SafeFileWrite::atomicWrite(path, out.str());
}

}  // namespace ScreenScraperSettings
