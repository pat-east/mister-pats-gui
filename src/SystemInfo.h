#pragma once

#include <cstdint>
#include <string>
#include <vector>

// What the machine is doing right now, read straight from /proc and the mounted volumes.
//
// Only what the device actually reports: this board exposes no thermal zone and no cpufreq,
// so there is no temperature and no clock speed here. Showing a made-up figure would be
// worse than showing none.
class SystemInfo {
public:
    struct Volume {
        std::string path;
        uint64_t totalBytes = 0;
        uint64_t freeBytes = 0;
    };

    // Cheap enough to call every frame; it only does the work every `intervalSeconds`.
    void update(float deltaSeconds, float intervalSeconds = 2.0f);

    const std::string &cpuModel() const { return cpuModel_; }
    int cores() const { return cores_; }

    // Share of the last interval the processors were busy, 0..1. Negative before the first
    // two samples, because a rate needs two of them.
    float cpuBusy() const { return cpuBusy_; }
    float loadAverage() const { return load_; }

    uint64_t memoryTotalBytes() const { return memTotal_; }
    uint64_t memoryUsedBytes() const { return memTotal_ - memAvailable_; }

    long uptimeSeconds() const { return uptime_; }
    const std::vector<Volume> &volumes() const { return volumes_; }

    // "1.4 GB", "93 MB" — one decimal only where it carries information.
    static std::string formatBytes(uint64_t bytes);
    static std::string formatDuration(long seconds);

private:
    void readOnce();
    void sampleCpu();

    float timer_ = 0.0f;
    bool first_ = true;

    std::string cpuModel_ = "unknown";
    int cores_ = 0;
    float cpuBusy_ = -1.0f;
    float load_ = 0.0f;
    uint64_t memTotal_ = 0;
    uint64_t memAvailable_ = 0;
    long uptime_ = 0;
    std::vector<Volume> volumes_;

    unsigned long long lastBusy_ = 0;
    unsigned long long lastTotal_ = 0;
};
