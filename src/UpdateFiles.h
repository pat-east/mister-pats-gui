#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "ReleaseInfo.h"

namespace UpdateFiles {

constexpr const char *kRoot = "/media/fat/mister-pat";

struct LibrarySelection {
    std::string selectedName;
    Version selectedVersion;
    std::string newestName;
    Version newestVersion;
    bool pinned = false;
};

bool readLimited(const std::string &path, size_t limit, std::string &out);
bool sha256(const std::string &path, std::string &hash, std::string &error,
            const std::atomic<bool> *cancel = nullptr);
bool isElf(const std::string &path);
bool regularFile(const std::string &path);
bool librarySelection(LibrarySelection &selection, std::string &error);
bool writeExclusive(const std::string &path, const std::string &data, unsigned mode,
                    std::string &error);
bool copyFile(const std::string &source, const std::string &destination, unsigned mode,
              std::string &error);
bool sameHash(const std::string &path, const std::string &expected, std::string &error,
              const std::atomic<bool> *cancel = nullptr);
std::string bootId();

} // namespace UpdateFiles
