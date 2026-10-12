#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "DiscDriveService.h"

// Inspects the scoped USB CDROM Loader installation. It never reads or writes the global
// MiSTer main= setting.
class DiscCoreManager {
public:
    struct Status {
        bool alternateMainInstalled = false;
        bool alternateMainVerified = false;
        bool psxMglInstalled = false;
        bool saturnMglInstalled = false;
        bool psxMglCompatible = false;
        bool saturnMglCompatible = false;
        bool psxMglVerified = false;
        bool saturnMglVerified = false;
        bool psxCoreInstalled = false;
        bool saturnCoreInstalled = false;
        bool scopedMainConfigured = false;
        std::string psxMgl;
        std::string saturnMgl;

        bool packageReady() const {
            return alternateMainVerified && psxMglCompatible && saturnMglCompatible &&
                   psxMglVerified && saturnMglVerified && scopedMainConfigured;
        }
        bool ready() const {
            return packageReady() && psxCoreInstalled && saturnCoreInstalled;
        }
        bool readyFor(DiscDriveService::System system) const {
            return alternateMainVerified && scopedMainConfigured &&
                   (system == DiscDriveService::System::PlayStation
                        ? psxMglCompatible && psxMglVerified && psxCoreInstalled
                        : system == DiscDriveService::System::Saturn
                            ? saturnMglCompatible && saturnMglVerified && saturnCoreInstalled : false);
        }
        bool hasAnyCore() const { return psxMglInstalled || saturnMglInstalled; }
    };

    DiscCoreManager();
    ~DiscCoreManager();
    DiscCoreManager(const DiscCoreManager &) = delete;
    DiscCoreManager &operator=(const DiscCoreManager &) = delete;

    // Latest filesystem inspection, refreshed by the manager worker.
    Status snapshot() const;
    static const char *projectUrl();

private:
    struct HashCacheEntry {
        uint64_t device = 0;
        uint64_t inode = 0;
        uint64_t size = 0;
        int64_t mtimeSeconds = 0;
        int64_t mtimeNanoseconds = 0;
        int64_t ctimeSeconds = 0;
        int64_t ctimeNanoseconds = 0;
        int64_t lastCheckTicks = 0;
        bool matches = false;
    };

    void workerLoop();
    Status inspect();
    bool matchesPinnedHash(const std::string &path, const char *expectedHash);

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    Status snapshot_;
    std::unordered_map<std::string, HashCacheEntry> hashCache_;
    bool stopping_ = false;
    std::thread worker_;
};
