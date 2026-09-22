#include "Downloader.h"

#include "Paths.h"

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

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

bool Downloader::fetch(const std::string &url, const std::string &destination,
                       int timeoutSeconds) {
    error_.clear();

    if (!safeUrl(url)) {
        error_ = "refused an unsafe URL";
        return false;
    }
    if (destination.find('\'') != std::string::npos) {
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
