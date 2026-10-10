#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>

struct Version {
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t patch = 0;
};

struct LatestRelease {
    std::string tag;
    std::string notes;
    std::set<std::string> assets;
};

struct ReleaseInfo {
    Version version;
    uint16_t gamesDbFormat = 0;
};

struct ReleaseCandidate {
    Version version;
    std::string tag;
    std::string notes;
    uint16_t gamesDbFormat = 0;
    std::map<std::string, std::string> hashes;
    std::string manifestHash;
};

bool parseVersion(const std::string &text, Version &out);
int compareVersions(const Version &left, const Version &right);
std::string versionText(const Version &version);
std::string libraryName(const Version &version);
std::string releaseAssetUrl(const std::string &tag, const std::string &name);
bool parseLibraryName(const std::string &name, Version &out);

bool parseLatestReleaseJson(const std::string &json, LatestRelease &out, std::string &error);
bool releaseHasAssets(const LatestRelease &release, const Version &version, std::string &error);
bool parseReleaseInfo(const std::string &text, ReleaseInfo &out, std::string &error);
bool parseSha256Sums(const std::string &text, const Version &version,
                     std::map<std::string, std::string> &out, std::string &error);
