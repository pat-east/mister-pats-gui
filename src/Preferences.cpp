#include "Preferences.h"

#include <cstdio>
#include <fstream>

bool Preferences::load(const std::string &file) {
    file_ = file;
    showGamesTab_ = true;
    showArcadeTab_ = true;
    showBoxArt_ = true;
    defaultView_ = "grid";
    checkForUpdates_ = false;

    std::ifstream in(file_);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        // Trim a trailing carriage return, in case the file was ever touched from Windows.
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line == "showGamesTab=0") showGamesTab_ = false;
        else if (line == "showArcadeTab=0") showArcadeTab_ = false;
        else if (line == "showBoxArt=0") showBoxArt_ = false;
        else if (line == "checkForUpdates=1") checkForUpdates_ = true;
        else if (line.compare(0, 12, "defaultView=") == 0) defaultView_ = line.substr(12);
    }

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

void Preferences::setDefaultView(const std::string &view) {
    defaultView_ = view;
    save();
}

void Preferences::setCheckForUpdates(bool check) {
    checkForUpdates_ = check;
    save();
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
    if (defaultView_ != "grid") out << "defaultView=" << defaultView_ << "\n";
    if (checkForUpdates_) out << "checkForUpdates=1\n";
    return true;
}
