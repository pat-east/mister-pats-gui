#include "Favorites.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

bool Favorites::load(const std::string &file) {
    file_ = file;
    entries_.clear();

    std::ifstream in(file_);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        const size_t first = line.find('\t');
        if (first == std::string::npos) continue;
        const size_t second = line.find('\t', first + 1);

        FavoriteEntry entry;
        entry.system = line.substr(0, first);
        if (second == std::string::npos) {
            entry.path = line.substr(first + 1);
        } else {
            entry.path = line.substr(first + 1, second - first - 1);
            entry.name = line.substr(second + 1);
        }

        if (entry.name.empty()) {
            const size_t slash = entry.path.find_last_of('/');
            entry.name = (slash == std::string::npos) ? entry.path : entry.path.substr(slash + 1);
        }

        entries_.push_back(std::move(entry));
    }

    return true;
}

bool Favorites::save() const {
    std::ofstream out(file_, std::ios::trunc);
    if (!out) {
        std::printf("favorites: cannot write %s\n", file_.c_str());
        return false;
    }

    for (const FavoriteEntry &entry : entries_)
        out << entry.system << '\t' << entry.path << '\t' << entry.name << '\n';

    return true;
}

bool Favorites::contains(const std::string &gamePath) const {
    return std::any_of(entries_.begin(), entries_.end(),
                       [&](const FavoriteEntry &e) { return e.path == gamePath; });
}

void Favorites::toggle(const std::string &system, const std::string &gamePath,
                       const std::string &name) {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const FavoriteEntry &e) { return e.path == gamePath; });

    if (it != entries_.end()) entries_.erase(it);
    else entries_.push_back({system, gamePath, name});

    save();
}
