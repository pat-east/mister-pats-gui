#include "Preferences.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <sys/stat.h>

#include <time.h>

#include "SafeFileWrite.h"

namespace {

constexpr const char *kZoneInfo = "/usr/share/zoneinfo/posix";
constexpr const char *kLinuxTimezone = "/media/fat/linux/timezone";

void listDirectories(const std::string &path, std::vector<std::string> &out) {
    DIR *dir = opendir(path.c_str());
    if (!dir) return;
    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        struct stat info{};
        const std::string child = path + "/" + name;
        if (lstat(child.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) out.push_back(name);
    }
    closedir(dir);
}

void listZoneFiles(const std::string &root, const std::string &relative,
                   std::vector<std::string> &out) {
    const std::string path = relative.empty() ? root : root + "/" + relative;
    DIR *dir = opendir(path.c_str());
    if (!dir) return;
    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        const std::string childRelative = relative.empty() ? name : relative + "/" + name;
        const std::string child = root + "/" + childRelative;
        struct stat linkInfo{};
        if (lstat(child.c_str(), &linkInfo) != 0) continue;
        if (S_ISDIR(linkInfo.st_mode)) {
            listZoneFiles(root, childRelative, out);
            continue;
        }
        struct stat targetInfo{};
        if (stat(child.c_str(), &targetInfo) == 0 && S_ISREG(targetInfo.st_mode))
            out.push_back(childRelative);
    }
    closedir(dir);
}

} // namespace

std::vector<std::string> Preferences::timeZoneRegions() {
    std::vector<std::string> regions;
    listDirectories(kZoneInfo, regions);
    std::sort(regions.begin(), regions.end());
    return regions;
}

std::vector<std::string> Preferences::timeZonesIn(const std::string &region) {
    if (region.empty() || region.find('/') != std::string::npos || region == "." ||
        region == "..") return {};
    const std::string root = std::string(kZoneInfo) + "/" + region;
    std::vector<std::string> zones;
    listZoneFiles(root, "", zones);
    std::sort(zones.begin(), zones.end());
    return zones;
}

bool Preferences::load(const std::string &file) {
    file_ = file;
    showGamesTab_ = true;
    showArcadeTab_ = true;
    showBoxArt_ = true;
    startSoundEnabled_ = true;
    menuSoundsEnabled_ = true;
    useSpaceGrotesk_ = true;
    defaultView_ = "grid";
    checkForUpdates_ = false;
    use24HourClock_ = true;
    timeZone_.clear();

    std::ifstream in(file_);
    if (!in) {
        applyTimeZone();
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        // Trim a trailing carriage return, in case the file was ever touched from Windows.
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line == "showGamesTab=0") showGamesTab_ = false;
        else if (line == "showArcadeTab=0") showArcadeTab_ = false;
        else if (line == "showBoxArt=0") showBoxArt_ = false;
        else if (line == "startSound=0") startSoundEnabled_ = false;
        else if (line == "menuSounds=0") menuSoundsEnabled_ = false;
        else if (line == "useSpaceGrotesk=0") useSpaceGrotesk_ = false;
        else if (line == "checkForUpdates=1") checkForUpdates_ = true;
        else if (line == "use24HourClock=0") use24HourClock_ = false;
        else if (line.compare(0, 9, "timeZone=") == 0) {
            const std::string zone = line.substr(9);
            const size_t slash = zone.find('/');
            if (slash != std::string::npos) {
                const std::string region = zone.substr(0, slash);
                const std::string city = zone.substr(slash + 1);
                for (const std::string &known : timeZonesIn(region))
                    if (known == city) timeZone_ = zone;
            }
        }
        else if (line.compare(0, 12, "defaultView=") == 0) defaultView_ = line.substr(12);
    }

    applyTimeZone();
    return true;
}

void Preferences::setShowGamesTab(bool show) {
    showGamesTab_ = show;
    save();
}

void Preferences::setShowArcadeTab(bool show) {
    showArcadeTab_ = show;
    save();
}

void Preferences::setShowBoxArt(bool show) {
    showBoxArt_ = show;
    save();
}

void Preferences::setStartSoundEnabled(bool enabled) {
    startSoundEnabled_ = enabled;
    save();
}

void Preferences::setMenuSoundsEnabled(bool enabled) {
    menuSoundsEnabled_ = enabled;
    save();
}

void Preferences::setUseSpaceGrotesk(bool use) {
    useSpaceGrotesk_ = use;
    save();
}

void Preferences::setDefaultView(const std::string &view) {
    defaultView_ = view;
    save();
}

void Preferences::setCheckForUpdates(bool check) {
    checkForUpdates_ = check;
    save();
}

void Preferences::setUse24HourClock(bool value) {
    use24HourClock_ = value;
    save();
}

bool Preferences::setTimeZone(const std::string &zone) {
    const size_t slash = zone.find('/');
    if (slash == std::string::npos) return false;
    const std::string region = zone.substr(0, slash);
    const std::string city = zone.substr(slash + 1);
    bool valid = false;
    for (const std::string &known : timeZonesIn(region))
        if (known == city) valid = true;
    if (!valid) return false;

    std::ifstream in(std::string(kZoneInfo) + "/" + zone, std::ios::binary);
    if (!in) return false;
    const std::string content((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    if (content.empty() || !SafeFileWrite::atomicWrite(kLinuxTimezone, content)) return false;

    timeZone_ = zone;
    applyTimeZone();
    save();
    return true;
}

void Preferences::applyTimeZone() const {
    if (timeZone_.empty()) unsetenv("TZ");
    else {
        const std::string value = ":/usr/share/zoneinfo/posix/" + timeZone_;
        setenv("TZ", value.c_str(), 1);
    }
    tzset();
}

bool Preferences::save() const {
    std::ofstream out(file_, std::ios::trunc);
    if (!out) {
        std::printf("preferences: cannot write %s\n", file_.c_str());
        return false;
    }

    if (!showGamesTab_) out << "showGamesTab=0\n";
    if (!showArcadeTab_) out << "showArcadeTab=0\n";
    if (!showBoxArt_) out << "showBoxArt=0\n";
    if (!startSoundEnabled_) out << "startSound=0\n";
    if (!menuSoundsEnabled_) out << "menuSounds=0\n";
    if (!useSpaceGrotesk_) out << "useSpaceGrotesk=0\n";
    if (defaultView_ != "grid") out << "defaultView=" << defaultView_ << "\n";
    if (checkForUpdates_) out << "checkForUpdates=1\n";
    if (!use24HourClock_) out << "use24HourClock=0\n";
    if (!timeZone_.empty()) out << "timeZone=" << timeZone_ << "\n";
    return true;
}
