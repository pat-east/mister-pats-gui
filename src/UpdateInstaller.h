#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ReleaseInfo.h"
#include "VerifiedDownloader.h"

enum class InstallStep { Downloading, Verifying, Installing };

struct InstallResult {
    enum class Code { Installed, Cancelled, RetryableError, RepairRequired };
    Code code = Code::RetryableError;
    std::string message;
    std::vector<std::string> paths;
};

class UpdateInstaller {
public:
    using Progress = std::function<void(InstallStep, uint64_t)>;
    InstallResult install(const ReleaseCandidate &candidate, const VerifiedDownloader &downloader,
                          std::atomic<bool> &cancel, const Progress &progress);
    bool removeVersionPin(const Version &expectedSelected, const Version &expectedNewest,
                          std::string &error);
};
