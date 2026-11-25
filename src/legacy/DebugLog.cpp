#include "DebugLog.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

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
std::mutex g_logMutex;

bool ensureDirectory() {
    struct stat st {};
    if (lstat(kDirectory, &st) == 0)
        return S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode);
    return errno == ENOENT && mkdir(kDirectory, 0755) == 0;
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
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!ensureDirectory()) return;

    struct stat st {};
    if (lstat(kPath, &st) == 0) {
        if (!S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) return;
        if (st.st_size > kMaxBytes && rename(kPath, kOldPath) != 0) return;
    } else if (errno != ENOENT) return;

    const int fd = open(kPath, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC | O_NOFOLLOW |
                        O_NONBLOCK, 0644);
    if (fd < 0) return;
    struct stat logInfo {};
    if (fstat(fd, &logInfo) != 0 || !S_ISREG(logInfo.st_mode)) {
        close(fd);
        return;
    }
    FILE *log = fdopen(fd, "a");
    if (!log) { close(fd); return; }

    char stamp[20] = "";
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    if (localtime_r(&now, &local))
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);

    const size_t length = std::min<size_t>(line.size(), 2048);
    std::fprintf(log, "%s [%s] ", stamp, tagFor(level));
    for (size_t i = 0; i < length; ++i) {
        const char ch = line[i];
        std::fputc(ch == '\n' || ch == '\r' ? ' ' : ch, log);
    }
    if (length < line.size()) std::fputs(" [truncated]", log);
    std::fputc('\n', log);
    std::fflush(log);
    std::fclose(log);
}

} // namespace DebugLog
