#include "UpdateFiles.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

extern char **environ;

namespace UpdateFiles {

bool regularFile(const std::string &path) {
    struct stat st {};
    return lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool readLimited(const std::string &path, size_t limit, std::string &out) {
    out.clear();
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return false;
    char buffer[4096];
    while (true) {
        ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 || out.size() + size_t(count) > limit) {
            close(fd);
            out.clear();
            return false;
        }
        if (count == 0) break;
        out.append(buffer, size_t(count));
    }
    close(fd);
    return true;
}

bool isElf(const std::string &path) {
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return false;
    unsigned char magic[4] = {};
    ssize_t count = read(fd, magic, sizeof(magic));
    close(fd);
    return count == 4 && magic[0] == 0x7f && magic[1] == 'E' &&
           magic[2] == 'L' && magic[3] == 'F';
}

bool librarySelection(LibrarySelection &selection, std::string &error) {
    selection = {};
    DIR *directory = opendir(kRoot);
    if (!directory) { error = "cannot list GUI libraries"; return false; }
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        Version version;
        const std::string name(entry->d_name);
        if (!parseLibraryName(name, version) || !regularFile(std::string(kRoot) + "/" + name))
            continue;
        if (selection.newestName.empty() || compareVersions(version, selection.newestVersion) > 0) {
            selection.newestName = name;
            selection.newestVersion = version;
        }
    }
    closedir(directory);
    if (selection.newestName.empty()) { error = "no versioned GUI library installed"; return false; }
    const std::string link = std::string(kRoot) + "/mister-pats-gui.so";
    struct stat st {};
    if (lstat(link.c_str(), &st) != 0) {
        if (errno != ENOENT) { error = "cannot inspect GUI version pin: " + link; return false; }
        selection.selectedName = selection.newestName;
        selection.selectedVersion = selection.newestVersion;
        return true;
    }
    if (!S_ISLNK(st.st_mode)) {
        error = "GUI version pin is not a symlink: " + link;
        return false;
    }
    char target[256];
    ssize_t length = readlink(link.c_str(), target, sizeof(target) - 1);
    if (length < 0 || length >= (ssize_t)sizeof(target) - 1) {
        error = "cannot read active GUI link: " + link;
        return false;
    }
    target[length] = '\0';
    selection.selectedName.assign(target);
    if (!parseLibraryName(selection.selectedName, selection.selectedVersion) ||
        !regularFile(std::string(kRoot) + "/" + selection.selectedName)) {
        error = "invalid GUI version pin: " + selection.selectedName;
        return false;
    }
    selection.pinned = true;
    return true;
}

bool writeExclusive(const std::string &path, const std::string &data, unsigned mode,
                    std::string &error) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, mode);
    if (fd < 0) { error = "cannot create " + path + ": " + std::strerror(errno); return false; }
    size_t position = 0;
    while (position < data.size()) {
        ssize_t count = write(fd, data.data() + position, data.size() - position);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { error = "cannot write " + path; break; }
        position += size_t(count);
    }
    if (position == data.size() && fsync(fd) != 0) error = "cannot sync " + path;
    if (close(fd) != 0 && error.empty()) error = "cannot close " + path;
    if (!error.empty()) { unlink(path.c_str()); return false; }
    return true;
}

bool copyFile(const std::string &source, const std::string &destination, unsigned mode,
              std::string &error) {
    if (!regularFile(source)) { error = "not a regular file: " + source; return false; }
    int in = open(source.c_str(), O_RDONLY | O_NOFOLLOW);
    if (in < 0) { error = "cannot read " + source; return false; }
    int out = open(destination.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, mode);
    if (out < 0) {
        error = "cannot create " + destination + ": " + std::strerror(errno);
        close(in);
        return false;
    }
    char buffer[32768];
    while (true) {
        ssize_t count = read(in, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { error = "cannot read " + source; break; }
        if (count == 0) break;
        size_t position = 0;
        while (position < size_t(count)) {
            ssize_t written = write(out, buffer + position, size_t(count) - position);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) { error = "cannot write " + destination; break; }
            position += size_t(written);
        }
        if (!error.empty()) break;
    }
    if (error.empty() && fchmod(out, mode) != 0) error = "cannot set mode " + destination;
    if (error.empty() && fsync(out) != 0) error = "cannot sync " + destination;
    close(in);
    if (close(out) != 0 && error.empty()) error = "cannot close " + destination;
    if (!error.empty()) { unlink(destination.c_str()); return false; }
    return true;
}

std::string bootId() {
    std::string id;
    if (!readLimited("/proc/sys/kernel/random/boot_id", 100, id)) return {};
    while (!id.empty() && (id.back() == '\n' || id.back() == '\r')) id.pop_back();
    return id;
}

bool sha256(const std::string &path, std::string &hash, std::string &error,
            const std::atomic<bool> *cancel) {
    hash.clear();
    if (!regularFile(path)) { error = "not a regular file: " + path; return false; }
    int outputPipe[2];
    if (pipe(outputPipe) != 0) { error = "cannot create hash pipe"; return false; }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, outputPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, outputPipe[0]);
    posix_spawn_file_actions_addclose(&actions, outputPipe[1]);
    char program[] = "sha256sum";
    char *args[] = {program, const_cast<char *>(path.c_str()), nullptr};
    pid_t child = -1;
    int spawned = posix_spawnp(&child, program, &actions, nullptr, args, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(outputPipe[1]);
    if (spawned != 0) {
        close(outputPipe[0]);
        error = "cannot start sha256sum: " + std::string(std::strerror(spawned));
        return false;
    }
    std::string line;
    bool interrupted = false;
    bool terminated = false;
    while (true) {
        if (cancel && cancel->load()) {
            interrupted = true;
            kill(child, SIGTERM);
            terminated = true;
            break;
        }
        struct pollfd pfd {outputPipe[0], POLLIN | POLLHUP, 0};
        int ready = poll(&pfd, 1, 200);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) { error = "cannot read sha256sum"; kill(child, SIGTERM); terminated = true; break; }
        if (ready == 0) continue;
        char buffer[128];
        ssize_t count = read(outputPipe[0], buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { error = "cannot read sha256sum"; kill(child, SIGTERM); terminated = true; break; }
        if (count == 0) break;
        if (line.size() + size_t(count) > 512) {
            error = "invalid sha256sum output";
            kill(child, SIGTERM);
            terminated = true;
            break;
        }
        line.append(buffer, size_t(count));
    }
    close(outputPipe[0]);
    int status = 0;
    if (terminated) {
        bool reaped = false;
        for (int attempt = 0; attempt < 20; ++attempt) {
            const pid_t done = waitpid(child, &status, WNOHANG);
            if (done == child || (done < 0 && errno == ECHILD)) { reaped = true; break; }
            usleep(100000);
        }
        if (!reaped) kill(child, SIGKILL);
    }
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    if (interrupted) { error = "cancelled"; return false; }
    if (!error.empty()) return false;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        line.size() < 66 || line[64] != ' ') {
        error = "sha256sum failed for " + path;
        return false;
    }
    for (size_t i = 0; i < 64; ++i) {
        if (!((line[i] >= '0' && line[i] <= '9') ||
              (line[i] >= 'a' && line[i] <= 'f'))) {
            error = "invalid sha256sum output";
            return false;
        }
    }
    hash = line.substr(0, 64);
    return true;
}

bool sameHash(const std::string &path, const std::string &expected, std::string &error,
              const std::atomic<bool> *cancel) {
    std::string actual;
    if (!sha256(path, actual, error, cancel)) return false;
    if (actual != expected) {
        error = "SHA-256 mismatch: " + path;
        return false;
    }
    return true;
}

} // namespace UpdateFiles
