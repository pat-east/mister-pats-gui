#include "CoreIndex.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>

namespace {

const char *kCoreDirs[] = {
    "_Console", "_Computer", "_Handheld",   "_Arcade",
    "_Other",   "_Utility",  "_ExtraCores", "_Ports",
};

// The directory a core lives in already states what kind of machine it is.
struct GroupName {
    const char *dir;
    const char *label;
};

const GroupName kGroupNames[] = {
    {"_Console", "Consoles"},   {"_Computer", "Computers"}, {"_Handheld", "Handhelds"},
    {"_Arcade", "Arcade"},      {"_Other", "Other"},        {"_Utility", "Utility"},
    {"_Ports", "Ports"},        {"_ExtraCores", "Other"},
};

bool hasSuffix(const std::string &s, const char *suffix) {
    const size_t n = std::strlen(suffix);
    return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, suffix) == 0;
}


// "SNES_20260823.rbf" -> "SNES";  "Atari 2600.mgl" -> "Atari 2600"
std::string coreNameOf(const std::string &filename) {
    const size_t dot = filename.find_last_of('.');
    std::string base = (dot == std::string::npos) ? filename : filename.substr(0, dot);

    // Cores carry a release date: eight digits after a final underscore.
    const size_t underscore = base.find_last_of('_');
    if (underscore != std::string::npos && base.size() - underscore == 9) {
        bool allDigits = true;
        for (size_t i = underscore + 1; i < base.size(); ++i)
            if (base[i] < '0' || base[i] > '9') allDigits = false;
        if (allDigits) base = base.substr(0, underscore);
    }

    return base;
}

} // namespace

std::string CoreIndex::normalise(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c >= 'a' && c <= 'z') out.push_back(char(c - 32));
        else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out.push_back(c);
    }
    return out;
}

std::string CoreIndex::groupOf(const std::string &relPath) {
    const size_t slash = relPath.find('/');
    if (slash == std::string::npos) return std::string();

    const std::string dir = relPath.substr(0, slash);
    for (const GroupName &group : kGroupNames)
        if (dir == group.dir) return group.label;

    return std::string();
}

void CoreIndex::scan(const std::vector<std::string> &roots) {
    cores_.clear();

    for (const std::string &root : roots) {
        for (const char *name : kCoreDirs) {
            const std::string dir = root + "/" + name;
            DIR *d = opendir(dir.c_str());
            if (!d) continue;

            while (dirent *entry = readdir(d)) {
                const std::string filename = entry->d_name;
                if (filename.empty() || filename[0] == '.') continue;
                if (!hasSuffix(filename, ".rbf") && !hasSuffix(filename, ".mgl")) continue;

                Entry core;
                core.key = normalise(coreNameOf(filename));
                core.relPath = std::string(name) + "/" + coreNameOf(filename);
                if (!core.key.empty()) cores_.push_back(core);
            }
            closedir(d);
        }
    }

    std::printf("cores: %zu indexed on %zu volume(s)\n", cores_.size(), roots.size());
}

std::string CoreIndex::find(const std::vector<std::string> &names) const {
    for (const std::string &name : names) {
        const std::string key = normalise(name);
        if (key.empty()) continue;
        for (const Entry &core : cores_)
            if (core.key == key) return core.relPath;
    }

    // Only then loosen up. Doing this in a second pass matters: an exact match anywhere in
    // the list must beat a prefix match on an earlier name.
    for (const std::string &name : names) {
        const std::string key = normalise(name);
        if (key.size() < 3) continue;
        for (const Entry &core : cores_) {
            if (core.key.empty()) continue;
            if (core.key.compare(0, key.size(), key) == 0) return core.relPath;
            if (key.compare(0, core.key.size(), core.key) == 0) return core.relPath;
        }
    }

    return std::string();
}
