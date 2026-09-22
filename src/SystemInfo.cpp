#include "SystemInfo.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/statvfs.h>

#include "GameDatabase.h"

namespace {

std::string trim(const std::string &s) {
    const size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

uint64_t readMemoryValue(const std::string &line) {
    const size_t colon = line.find(':');
    if (colon == std::string::npos) return 0;
    return uint64_t(std::strtoull(line.c_str() + colon + 1, nullptr, 10)) * 1024;   // kB
}

} // namespace

std::string SystemInfo::formatBytes(uint64_t bytes) {
    char buffer[32];

    if (bytes >= 1024ull * 1024 * 1024) {
        std::snprintf(buffer, sizeof(buffer), "%.1f GB", double(bytes) / (1024.0 * 1024 * 1024));
        return buffer;
    }
    if (bytes >= 1024ull * 1024) {
        std::snprintf(buffer, sizeof(buffer), "%llu MB", (unsigned long long)(bytes / (1024 * 1024)));
        return buffer;
    }
    std::snprintf(buffer, sizeof(buffer), "%llu KB", (unsigned long long)(bytes / 1024));
    return buffer;
}

std::string SystemInfo::formatDuration(long seconds) {
    char buffer[48];

    const long days = seconds / 86400;
    const long hours = (seconds % 86400) / 3600;
    const long minutes = (seconds % 3600) / 60;

    if (days) std::snprintf(buffer, sizeof(buffer), "%ldd %ldh", days, hours);
    else if (hours) std::snprintf(buffer, sizeof(buffer), "%ldh %ldmin", hours, minutes);
    else std::snprintf(buffer, sizeof(buffer), "%ldmin", minutes);
    return buffer;
}

void SystemInfo::sampleCpu() {
    std::ifstream in("/proc/stat");
    std::string line;
    if (!std::getline(in, line) || line.compare(0, 4, "cpu ") != 0) return;

    unsigned long long value = 0, total = 0, idle = 0;
    const char *p = line.c_str() + 4;
    for (int field = 0; field < 10; ++field) {
        char *end = nullptr;
        value = std::strtoull(p, &end, 10);
        if (end == p) break;
        p = end;

        total += value;
        // Fields 3 and 4 are idle and iowait — time the processors were not working.
        if (field == 3 || field == 4) idle += value;
    }
    if (!total) return;

    const unsigned long long busy = total - idle;
    if (lastTotal_ && total > lastTotal_) {
        const double span = double(total - lastTotal_);
        cpuBusy_ = float(double(busy - lastBusy_) / span);
    }
    lastBusy_ = busy;
    lastTotal_ = total;
}

void SystemInfo::readOnce() {
    if (cores_ == 0) {
        std::ifstream in("/proc/cpuinfo");
        std::string line;
        while (std::getline(in, line)) {
            if (line.compare(0, 10, "processor\t") == 0 || line.compare(0, 9, "processor") == 0)
                ++cores_;
            if (cpuModel_ == "unknown" && line.compare(0, 10, "model name") == 0) {
                const size_t colon = line.find(':');
                if (colon != std::string::npos) cpuModel_ = trim(line.substr(colon + 1));
            }
        }
    }

    {
        std::ifstream in("/proc/meminfo");
        std::string line;
        while (std::getline(in, line)) {
            if (line.compare(0, 9, "MemTotal:") == 0) memTotal_ = readMemoryValue(line);
            else if (line.compare(0, 13, "MemAvailable:") == 0) memAvailable_ = readMemoryValue(line);
        }
    }

    {
        std::ifstream in("/proc/loadavg");
        in >> load_;
    }

    {
        std::ifstream in("/proc/uptime");
        double seconds = 0;
        in >> seconds;
        uptime_ = long(seconds);
    }

    volumes_.clear();
    for (const std::string &mount : GameDatabase::mountPoints()) {
        struct statvfs st {};
        if (statvfs(mount.c_str(), &st) != 0) continue;

        Volume volume;
        volume.path = mount;
        volume.totalBytes = uint64_t(st.f_blocks) * st.f_frsize;
        volume.freeBytes = uint64_t(st.f_bavail) * st.f_frsize;
        if (volume.totalBytes) volumes_.push_back(volume);
    }

    sampleCpu();
}

void SystemInfo::update(float deltaSeconds, float intervalSeconds) {
    timer_ -= deltaSeconds;
    if (!first_ && timer_ > 0.0f) return;

    first_ = false;
    timer_ = intervalSeconds;
    readOnce();
}
