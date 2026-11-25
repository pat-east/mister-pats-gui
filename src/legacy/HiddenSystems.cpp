#include "HiddenSystems.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

bool HiddenSystems::load(const std::string &file) {
    file_ = file;
    names_.clear();

    std::ifstream in(file_);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        // Trim a trailing carriage return, in case the file was ever touched from Windows.
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (!line.empty()) names_.push_back(line);
    }

    return true;
}

bool HiddenSystems::save() const {
    std::ofstream out(file_, std::ios::trunc);
    if (!out) {
        std::printf("hidden-systems: cannot write %s\n", file_.c_str());
        return false;
    }

    for (const std::string &name : names_) out << name << '\n';
    return true;
}

bool HiddenSystems::contains(const std::string &systemName) const {
    return std::find(names_.begin(), names_.end(), systemName) != names_.end();
}

void HiddenSystems::toggle(const std::string &systemName) {
    const auto it = std::find(names_.begin(), names_.end(), systemName);

    if (it != names_.end()) names_.erase(it);
    else names_.push_back(systemName);

    save();
}
