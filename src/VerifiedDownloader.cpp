#include "VerifiedDownloader.h"

#include <cerrno>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <vector>

#include "Paths.h"

extern char **environ;

namespace {

const char *certificatePath() {
    const char *paths[] = {
        "/media/fat/Scripts/.config/downloader/cacert.pem",
        MISTER_PAT_ROOT "/cacert.pem",
        "/etc/ssl/certs/cacert.pem",
        "/etc/ssl/cert.pem",
    };
    struct stat st {};
    for (const char *path : paths)
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return path;
    return nullptr; // curl may have a compiled-in verified system CA store.
}

bool safeHttps(const std::string &url) {
    if (url.compare(0, 8, "https://") != 0) return false;
    for (unsigned char c : url)
        if (c <= 0x20 || c >= 0x7f || c == '\\' || c == '\'' || c == '"') return false;
    return true;
}

void stopChild(pid_t child) {
    kill(child, SIGTERM);
    for (int i = 0; i < 20; ++i) {
        int status = 0;
        if (waitpid(child, &status, WNOHANG) == child) return;
        usleep(100000);
    }
    kill(child, SIGKILL);
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
}

DownloadResult failed(DownloadResult::Code code, const std::string &message,
                      const std::string &) {
    DownloadResult result;
    result.code = code;
    result.error = message;
    return result;
}

} // namespace

DownloadResult VerifiedDownloader::fetch(const std::string &url, const std::string &destination,
                                         uint64_t maxBytes, int timeoutSeconds,
                                         const std::atomic<bool> &cancel,
                                         const Progress &progress) const {
    if (!safeHttps(url)) return failed(DownloadResult::Code::Tool, "non-HTTPS URL refused", destination);
    if (cancel.load()) return failed(DownloadResult::Code::Cancelled, "cancelled", destination);

    int output = open(destination.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (output < 0)
        return failed(DownloadResult::Code::File,
                      "cannot create " + destination + ": " + std::strerror(errno), destination);

    int bodyPipe[2];
    int errorPipe[2];
    if (pipe(bodyPipe) != 0) {
        close(output);
        return failed(DownloadResult::Code::File, "cannot create download pipe", destination);
    }
    if (pipe(errorPipe) != 0) {
        close(bodyPipe[0]); close(bodyPipe[1]); close(output);
        return failed(DownloadResult::Code::File, "cannot create error pipe", destination);
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, bodyPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, errorPipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, bodyPipe[0]);
    posix_spawn_file_actions_addclose(&actions, errorPipe[0]);
    posix_spawn_file_actions_addclose(&actions, bodyPipe[1]);
    posix_spawn_file_actions_addclose(&actions, errorPipe[1]);

    std::string time = std::to_string(timeoutSeconds);
    std::string connectTime = std::to_string(std::min(12, timeoutSeconds));
    std::vector<std::string> arguments = {
        "curl", "--proto", "=https", "--proto-redir", "=https", "--fail",
        "--location", "--max-redirs", "5", "--silent", "--show-error",
        "--connect-timeout", connectTime, "--max-time", time, "--output", "-"
    };
    if (const char *ca = certificatePath()) {
        arguments.push_back("--cacert");
        arguments.push_back(ca);
    }
    arguments.push_back("--url");
    arguments.push_back(url);
    std::vector<char *> argv;
    for (std::string &argument : arguments) argv.push_back(&argument[0]);
    argv.push_back(nullptr);
    pid_t child = -1;
    int spawnError = posix_spawnp(&child, "curl", &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(bodyPipe[1]);
    close(errorPipe[1]);
    if (spawnError != 0) {
        close(bodyPipe[0]); close(errorPipe[0]); close(output);
        return failed(DownloadResult::Code::Tool,
                      "cannot start curl: " + std::string(std::strerror(spawnError)), destination);
    }

    DownloadResult result;
    result.code = DownloadResult::Code::Ok;
    std::string stderrText;
    int bodyFd = bodyPipe[0];
    int errorFd = errorPipe[0];
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeoutSeconds + 2);
    while (bodyFd >= 0 || errorFd >= 0) {
        if (cancel.load()) {
            result.code = DownloadResult::Code::Cancelled;
            result.error = "cancelled";
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            result.code = DownloadResult::Code::Timeout;
            result.error = "download timed out";
            break;
        }
        struct pollfd fds[2] = {{bodyFd, POLLIN | POLLHUP, 0},
                                {errorFd, POLLIN | POLLHUP, 0}};
        int ready = poll(fds, 2, 200);
        if (ready < 0) {
            if (errno == EINTR) continue;
            result.code = DownloadResult::Code::File;
            result.error = "download pipe failed";
            break;
        }
        if (ready == 0) continue;
        if (bodyFd >= 0 && (fds[0].revents & (POLLIN | POLLHUP))) {
            char buffer[8192];
            ssize_t count = read(bodyFd, buffer, sizeof(buffer));
            if (count == 0) { close(bodyFd); bodyFd = -1; }
            else if (count < 0) {
                if (errno != EINTR) {
                    result.code = DownloadResult::Code::File;
                    result.error = "cannot read downloaded bytes";
                    break;
                }
            } else {
                if (uint64_t(count) > maxBytes - result.bytes) {
                    result.code = DownloadResult::Code::TooLarge;
                    result.error = "download exceeds size limit";
                    break;
                }
                size_t position = 0;
                while (position < size_t(count)) {
                    ssize_t written = write(output, buffer + position, size_t(count) - position);
                    if (written < 0 && errno == EINTR) continue;
                    if (written <= 0) {
                        result.code = DownloadResult::Code::File;
                        result.error = "cannot write download";
                        break;
                    }
                    position += size_t(written);
                }
                if (!result.ok()) break;
                result.bytes += uint64_t(count);
                if (progress) progress(result.bytes);
            }
        }
        if (errorFd >= 0 && (fds[1].revents & (POLLIN | POLLHUP))) {
            char buffer[512];
            ssize_t count = read(errorFd, buffer, sizeof(buffer));
            if (count == 0) { close(errorFd); errorFd = -1; }
            else if (count > 0 && stderrText.size() < 2048)
                stderrText.append(buffer, size_t(count) < 2048 - stderrText.size()
                                             ? size_t(count) : 2048 - stderrText.size());
        }
    }
    if (bodyFd >= 0) close(bodyFd);
    if (errorFd >= 0) close(errorFd);
    if (close(output) != 0 && result.ok()) {
        result.code = DownloadResult::Code::File;
        result.error = "cannot close downloaded file";
    }
    if (!result.ok()) {
        stopChild(child);
        unlink(destination.c_str());
        return result;
    }

    int status = 0;
    for (;;) {
        const pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) break;
        if (waited < 0 && errno == EINTR) continue;
        if (waited < 0) {
            result.code = DownloadResult::Code::File;
            result.error = "cannot wait for curl";
            unlink(destination.c_str());
            return result;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            stopChild(child);
            result.code = DownloadResult::Code::Timeout;
            result.error = "download timed out";
            unlink(destination.c_str());
            return result;
        }
        usleep(100000);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        result.code = exitCode == 60 || exitCode == 77 || exitCode == 35
                          ? DownloadResult::Code::Tls
                          : exitCode == 28 ? DownloadResult::Code::Timeout
                          : exitCode == 22 ? DownloadResult::Code::Http
                          : exitCode == 63 ? DownloadResult::Code::TooLarge
                          : exitCode == 1 || exitCode == 2 ? DownloadResult::Code::Tool
                                                           : DownloadResult::Code::Network;
        result.error = stderrText.empty() ? "curl failed (" + std::to_string(exitCode) + ")"
                                          : stderrText;
        unlink(destination.c_str());
    } else if (result.bytes == 0) {
        result.code = DownloadResult::Code::Network;
        result.error = "empty download";
        unlink(destination.c_str());
    }
    return result;
}
