#include "Preferences.h"

#include <cstdio>
#include <fstream>

bool Preferences::load(const std::string &file) {
    file_ = file;
    showGamesTab_ = true;

    std::ifstream in(file_);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        // Trim a trailing carriage return, in case the file was ever touched from Windows.
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line == "showGamesTab=0") showGamesTab_ = false;
    }

    return true;
}

void Preferences::setShowGamesTab(bool show) {
    showGamesTab_ = show;
    save();
}

bool Preferences::save() const {
    std::ofstream out(file_, std::ios::trunc);
    if (!out) {
        std::printf("preferences: cannot write %s\n", file_.c_str());
        return false;
    }

    if (!showGamesTab_) out << "showGamesTab=0\n";
    return true;
}
