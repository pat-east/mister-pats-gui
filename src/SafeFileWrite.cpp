#include "SafeFileWrite.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

// mkdir -p, one path segment at a time — none of this project's target directories are more
// than a couple of levels deep, so simplicity wins over a general-purpose implementation.
void makeDirs(const std::string &path) {
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        current += path[i];
        if (path[i] == '/' || i + 1 == path.size()) {
            if (current.size() > 1) mkdir(current.c_str(), 0755);
        }
    }
}

std::string dirnameOf(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

bool readWholeFile(const std::string &path, std::string &out) {
    out.clear();
    const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        info.st_size > 1024 * 1024) {
        close(fd);
        return false;
    }
    out.reserve(static_cast<size_t>(info.st_size));
    char buffer[4096];
    for (;;) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 || out.size() + static_cast<size_t>(count) > 1024 * 1024) {
            close(fd);
            out.clear();
            return false;
        }
        if (count == 0) break;
        out.append(buffer, static_cast<size_t>(count));
    }
    return close(fd) == 0;
}

} // namespace

namespace SafeFileWrite {

bool atomicWrite(const std::string &path, const std::string &content) {
    makeDirs(dirnameOf(path));

    static std::atomic<unsigned long long> sequence{0};
    std::string tmpPath;
    int fd = -1;
    for (int attempt = 0; attempt < 8 && fd < 0; ++attempt) {
        tmpPath = path + ".tmp-" + std::to_string(getpid()) + "-" +
                  std::to_string(sequence.fetch_add(1));
        fd = open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
    }
    if (fd < 0) {
        std::printf("safe-write: cannot create temporary file for %s: %s\n",
                    path.c_str(), std::strerror(errno));
        return false;
    }
    size_t written = 0;
    while (written < content.size()) {
        const ssize_t count = write(fd, content.data() + written, content.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        written += static_cast<size_t>(count);
    }
    const bool synced = written == content.size() && fsync(fd) == 0;
    const bool closed = close(fd) == 0;
    if (!synced || !closed) {
        std::printf("safe-write: cannot write temporary file for %s\n", path.c_str());
        unlink(tmpPath.c_str());
        return false;
    }
    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        std::printf("safe-write: rename %s -> %s failed: %s\n", tmpPath.c_str(), path.c_str(),
                    std::strerror(errno));
        unlink(tmpPath.c_str());
        return false;
    }
    const std::string parent = dirnameOf(path);
    const int directory = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory >= 0) { fsync(directory); close(directory); }
    return true;
}

bool backupBeforeWrite(const std::string &sourcePath, const std::string &backupDir,
                       int maxBackups) {
    std::string content;
    if (!readWholeFile(sourcePath, content)) {
        std::printf("safe-write: cannot read %s, refusing to write without a backup\n",
                    sourcePath.c_str());
        return false;
    }

    makeDirs(backupDir);

    const size_t sourceSlash = sourcePath.find_last_of('/');
    const std::string base =
        sourceSlash == std::string::npos ? sourcePath : sourcePath.substr(sourceSlash + 1);

    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    tm local{};
    localtime_r(&ts.tv_sec, &local);

    char stamp[64];
    // Milliseconds included: several backups can legitimately happen within one second (a
    // deadzone tweaked repeatedly while testing), and the name has to keep sorting correctly
    // when that happens rather than colliding or landing out of order.
    std::snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d-%03ld",
                 local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
                 local.tm_min, local.tm_sec, long(ts.tv_nsec / 1000000));

    const std::string backupPath = backupDir + "/" + stamp + "-" + base;
    if (!atomicWrite(backupPath, content)) return false;

    // Rotate: keep at most `maxBackups`, oldest first out. The timestamp prefix means plain
    // lexicographic order is chronological order.
    std::vector<std::string> names;
    if (DIR *dir = opendir(backupDir.c_str())) {
        while (dirent *entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            names.push_back(name);
        }
        closedir(dir);
    }
    std::sort(names.begin(), names.end());

    while (int(names.size()) > maxBackups) {
        const std::string victim = backupDir + "/" + names.front();
        std::remove(victim.c_str());
        names.erase(names.begin());
    }

    return true;
}

} // namespace SafeFileWrite
