#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

struct DownloadResult {
    enum class Code { Ok, Cancelled, Network, Tls, Timeout, Http, TooLarge, Tool, File };
    Code code = Code::File;
    std::string error;
    uint64_t bytes = 0;
    bool ok() const { return code == Code::Ok; }
};

class VerifiedDownloader {
public:
    using Progress = std::function<void(uint64_t)>;
    DownloadResult fetch(const std::string &url, const std::string &destination,
                         uint64_t maxBytes, int timeoutSeconds,
                         const std::atomic<bool> &cancel, const Progress &progress) const;
};
