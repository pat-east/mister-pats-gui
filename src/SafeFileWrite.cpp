#include "SafeFileWrite.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
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
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}

} // namespace

namespace SafeFileWrite {

bool atomicWrite(const std::string &path, const std::string &content) {
    makeDirs(dirnameOf(path));

    const std::string tmpPath = path + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::printf("safe-write: cannot open %s\n", tmpPath.c_str());
            return false;
        }
        out.write(content.data(), std::streamsize(content.size()));
        if (!out.good()) {
            std::printf("safe-write: short write to %s\n", tmpPath.c_str());
            return false;
        }
    }

    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        std::printf("safe-write: rename %s -> %s failed: %s\n", tmpPath.c_str(), path.c_str(),
                    std::strerror(errno));
        return false;
    }
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
