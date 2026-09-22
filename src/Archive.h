#pragma once

#include <string>
#include <vector>

// Reads entry names from a zip's central directory. MiSTer addresses an archived ROM as
// "<archive>.zip/<inner path>"; a path pointing at the bare archive loads nothing.
class Archive {
public:
    static std::vector<std::string> entries(const std::string &path);

    // First entry matching one of the extensions, empty when none fits.
    static std::string findRom(const std::string &path, const std::vector<std::string> &extensions);
};
