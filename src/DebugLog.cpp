#include "DebugLog.h"

#include <cstdio>
#include <ctime>
#include <sys/stat.h>

#include "Paths.h"

namespace DebugLog {

namespace {

constexpr const char *kDirectory = MISTER_PAT_ROOT "/logs";
constexpr const char *kPath = MISTER_PAT_ROOT "/logs/debug.log";
constexpr const char *kOldPath = MISTER_PAT_ROOT "/logs/debug.log.old";
// One rotation kept alongside the live file, so a report can always include both without
// needing to catch it mid-write. On /media/fat rather than /tmp so a log survives the reboot
// that a boot-time bug (the reason this exists at all) is likely to be followed by.
constexpr long kMaxBytes = 512 * 1024;

bool ensureDirectory() {
    struct stat st {};
    if (stat(kDirectory, &st) == 0) return true;
    return mkdir(kDirectory, 0777) == 0;
}

const char *tagFor(Level level) {
    switch (level) {
    case Level::Warning: return "WARN ";
    case Level::Error:   return "ERROR";
    case Level::Info:
    default:             return "INFO ";
    }
}

} // namespace

void write(Level level, const std::string &line) {
    if (!ensureDirectory()) return;

    struct stat st {};
    if (stat(kPath, &st) == 0 && st.st_size > kMaxBytes) rename(kPath, kOldPath);

    FILE *log = std::fopen(kPath, "a");
    if (!log) return;

    char stamp[20] = "";
    const std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::fprintf(log, "%s [%s] %s\n", stamp, tagFor(level), line.c_str());
    std::fclose(log);
}

} // namespace DebugLog
