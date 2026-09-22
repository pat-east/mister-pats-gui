#pragma once

#include "Paths.h"

#include <string>
#include <vector>

struct FavoriteEntry {
    std::string system;
    std::string path;
    std::string name;
};

// Favourites live in the app's own writable directory, one tab-separated line per entry.
class Favorites {
public:
    bool load(const std::string &file = kDefaultFile);
    bool save() const;

    bool contains(const std::string &gamePath) const;
    void toggle(const std::string &system, const std::string &gamePath, const std::string &name);

    const std::vector<FavoriteEntry> &entries() const { return entries_; }
    size_t size() const { return entries_.size(); }

    static constexpr const char *kDefaultFile = MISTER_PAT_ROOT "/favorites.txt";

private:
    std::vector<FavoriteEntry> entries_;
    std::string file_ = kDefaultFile;
};
