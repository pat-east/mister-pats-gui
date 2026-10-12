#include "Downloader.h"

#include "Paths.h"

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace {

// Certificate bundles as they appear on a MiSTer, most specific first.
const char *kCertificates[] = {
    "/media/fat/Scripts/.config/downloader/cacert.pem",
    MISTER_PAT_ROOT "/cacert.pem",
    "/etc/ssl/certs/cacert.pem",
    "/etc/ssl/cert.pem",
};

bool fileExists(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool nonEmptyFile(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

} // namespace

bool Downloader::safeUrl(const std::string &url) {
    if (url.compare(0, 8, "https://") != 0 && url.compare(0, 7, "http://") != 0) return false;

    // The URL ends up inside a shell command, so anything that could end the argument or
    // start another one is refused outright rather than escaped. Quoting mistakes in this
    // position are how a downloader turns into a command injection.
    static const std::string allowed =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#&=+%";
    for (char c : url)
        if (allowed.find(c) == std::string::npos) return false;

    return true;
}

std::string Downloader::encodeComponent(const std::string &text) {
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size() * 3);

    for (unsigned char c : text) {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                                c == '~';
        if (unreserved) {
            out.push_back(char(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

std::string Downloader::certificateArgument() {
    for (const char *path : kCertificates)
        if (fileExists(path)) return std::string(" --cacert ") + path;
    return std::string();
}

std::string Downloader::command(Tool tool, const std::string &url,
                                const std::string &destination, int timeoutSeconds) const {
    const std::string time = std::to_string(timeoutSeconds);

    switch (tool) {
    case Tool::Curl:
        return "curl -4 -fsSL" + certificateArgument() + " --connect-timeout 12 --max-time " +
               time + " '" + url + "' -o '" + destination + "' 2>/dev/null";
    case Tool::CurlInsecure:
        return "curl -4 -fsSLk --connect-timeout 12 --max-time " + time + " '" + url +
               "' -o '" + destination + "' 2>/dev/null";
    case Tool::Wget:
        return "wget -4 --no-check-certificate --connect-timeout=12 -q -T " + time + " -O '" +
               destination + "' '" + url + "' 2>/dev/null";
    default:
        return std::string();
    }
}


bool Downloader::fetchWithStatus(const std::string &url, const std::string &destination,
                                 int &httpStatus, int timeoutSeconds,
                                 const std::atomic<bool> *cancel, size_t maxBytes) {
    error_.clear();
    httpStatus = 0;
    if (cancel && cancel->load()) {
        error_ = "cancelled";
        return false;
    }
    if (url.compare(0, 8, "https://") != 0 || url.size() > 512 ||
        !safeUrl(url) || timeoutSeconds < 1 || maxBytes == 0 ||
        maxBytes > 16u * 1024u * 1024u) {
        error_ = "refused an unsafe request";
        return false;
    }

    int configPipe[2] = {-1, -1}, statusPipe[2] = {-1, -1};
    if (pipe(configPipe) != 0) {
        error_ = "cannot create API request pipes";
        return false;
    }
    if (pipe(statusPipe) != 0) {
        close(configPipe[0]); close(configPipe[1]);
        error_ = "cannot create API request pipes";
        return false;
    }
    const std::string config = "url = \"" + url + "\"\n";
    size_t written = 0;
    while (written < config.size()) {
        const ssize_t n = write(configPipe[1], config.data() + written, config.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        written += size_t(n);
    }
    close(configPipe[1]);
    if (written != config.size()) {
        close(configPipe[0]); close(statusPipe[0]); close(statusPipe[1]);
        error_ = "cannot prepare API request";
        return false;
    }
    unlink(destination.c_str());
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, configPipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, statusPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, configPipe[0]);
    posix_spawn_file_actions_addclose(&actions, statusPipe[0]);
    posix_spawn_file_actions_addclose(&actions, statusPipe[1]);
    std::vector<std::string> args = {"curl", "-4", "-sS", "-L", "--proto", "=https",
        "--proto-redir", "=https", "--connect-timeout", "12", "--max-time",
        std::to_string(timeoutSeconds), "--max-filesize", std::to_string(maxBytes),
        "-w", "%{http_code}",
        "-o", destination, "--config", "-"};
    for (const char *path : kCertificates) {
        if (!fileExists(path)) continue;
        args.insert(args.end() - 2, {"--cacert", path});
        break;
    }
    std::vector<char *> argv;
    for (std::string &arg : args) argv.push_back(&arg[0]);
    argv.push_back(nullptr);
    pid_t child = -1;
    const int spawnError = posix_spawnp(&child, "curl", &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(configPipe[0]);
    close(statusPipe[1]);
    if (spawnError != 0) {
        close(statusPipe[0]);
        error_ = "cannot start curl";
        return false;
    }
    char output[16] = {};
    size_t count = 0;
    bool timedOut = false;
    bool cancelled = false;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeoutSeconds + 5);
    while (count < sizeof(output) - 1) {
        if (cancel && cancel->load()) { cancelled = true; break; }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) { timedOut = true; break; }
        struct pollfd pfd {statusPipe[0], POLLIN | POLLHUP, 0};
        const int ready = poll(&pfd, 1, int(std::min<long long>(200, remaining)));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) { timedOut = ready == 0; break; }
        const ssize_t n = read(statusPipe[0], output + count, sizeof(output) - 1 - count);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        count += size_t(n);
    }
    close(statusPipe[0]);
    if (timedOut || cancelled) kill(child, SIGTERM);
    int childStatus = 0;
    while (waitpid(child, &childStatus, 0) < 0 && errno == EINTR) {}
    if (cancelled || timedOut || written != config.size() || !WIFEXITED(childStatus) ||
        WEXITSTATUS(childStatus) != 0 || count < 3) {
        unlink(destination.c_str());
        error_ = cancelled ? "cancelled" : timedOut ? "request timed out" : "request failed";
        return false;
    }
    output[count] = '\0';
    httpStatus = std::atoi(output);
    if (!nonEmptyFile(destination)) {
        error_ = "empty response";
        return false;
    }
    return true;
}

bool Downloader::fetch(const std::string &url, const std::string &destination,
                       int timeoutSeconds) {
    error_.clear();

    if (!safeUrl(url)) {
        error_ = "refused an unsafe URL";
        return false;
    }
    if (!safeUrl(url) || destination.find("'") != std::string::npos) {
        error_ = "refused an unsafe destination";
        return false;
    }

    static const Tool kOrder[] = {Tool::Curl, Tool::CurlInsecure, Tool::Wget};

    if (tool_ == Tool::None) {
        error_ = "no downloader on this system (tried curl and wget)";
        return false;
    }

    // Once one has worked, stick with it. Probing three tools per file would triple the cost
    // of a scrape on a device that only has wget.
    if (tool_ != Tool::Unknown) {
        unlink(destination.c_str());
        if (std::system(command(tool_, url, destination, timeoutSeconds).c_str()) == 0 &&
            nonEmptyFile(destination))
            return true;

        error_ = "download failed";
        return false;
    }

    for (Tool candidate : kOrder) {
        unlink(destination.c_str());
        if (std::system(command(candidate, url, destination, timeoutSeconds).c_str()) != 0)
            continue;
        if (!nonEmptyFile(destination)) continue;

        tool_ = candidate;
        std::printf("downloader: using %s\n", candidate == Tool::Wget          ? "wget"
                                              : candidate == Tool::CurlInsecure ? "curl -k"
                                                                                : "curl");
        return true;
    }

    // Nothing worked. That may be this one URL or it may be the whole toolchain; only give
    // up on the toolchain if not even a connection could be made.
    error_ = "download failed (tried curl, curl -k, wget)";
    return false;
}
