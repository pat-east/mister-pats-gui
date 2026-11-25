#pragma once

#include "Paths.h"

#include <string>
#include <vector>

// Which systems the Systems tab should skip, chosen by the user rather than derived from
// anything on disk. Plain text, one system name per line — the same names shown in the tab
// and in the settings list that manages this, so no separate identifier scheme is needed.
class HiddenSystems {
public:
    bool load(const std::string &file = kDefaultFile);
    bool save() const;

    bool contains(const std::string &systemName) const;
    void toggle(const std::string &systemName);

    size_t size() const { return names_.size(); }

    static constexpr const char *kDefaultFile = MISTER_PAT_ROOT "/hidden-systems.txt";

private:
    std::vector<std::string> names_;
    std::string file_ = kDefaultFile;
};
