#include "Launcher.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "Archive.h"
#include "Library.h"

namespace {

// In preference order. A .cue names its .bin tracks, so it must win over the raw track files
// that sit beside it; .chd is a single self-contained file and is just as good.
const char *kDiscExtensions[] = {"cue", "chd", "iso", "ccd"};

// A drive still settling right after boot can make stat()/opendir() fail on a path that is
// perfectly real; a handful of short retries tells that apart from the path genuinely not
// being there. This matters here specifically because the alternative — silently treating an
// unreadable folder as if it were already the disc image — starts a core with nothing behind
// it rather than failing visibly.
//
// Three seconds, matching the settle delay SystemsScreen already waits out before its own
// first read: opening a large system's list is itself a burst of stat() calls, and launching
// a game straight out of that list is exactly when the drive is least likely to have caught
// up yet.
constexpr int kResolveAttempts = 10;
constexpr int kResolveDelayUs = 300000;   // 300 ms; ten attempts is a three-second ceiling

int openDirectoryNoFollow(const std::string &path) {
    if (path.empty() || path.front() != '/') return -1;
    int directory = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) return -1;
    size_t start = 1;
    while (start < path.size()) {
        const size_t slash = path.find('/', start);
        const std::string component = path.substr(start,
            slash == std::string::npos ? std::string::npos : slash - start);
        if (component.empty() || component == "." || component == "..") {
            close(directory);
            return -1;
        }
        const int next = openat(directory, component.c_str(),
                                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        close(directory);
        if (next < 0) return -1;
        directory = next;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return directory;
}

bool readPhysicalMgl(const std::string &root, const std::string &relative,
                     std::string &contents) {
    contents.clear();
    if (relative.empty()) return false;
    int directory = openDirectoryNoFollow(root);
    if (directory < 0) return false;

    size_t start = 0;
    while (start < relative.size()) {
        const size_t slash = relative.find('/', start);
        const std::string component = relative.substr(start,
            slash == std::string::npos ? std::string::npos : slash - start);
        if (component.empty() || component == "." || component == "..") {
            close(directory);
            return false;
        }
        const bool final = slash == std::string::npos;
        const int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC |
                          (final ? 0 : O_DIRECTORY);
        const int next = openat(directory, component.c_str(), flags);
        close(directory);
        if (next < 0) return false;
        if (final) {
            struct stat info {};
            if (fstat(next, &info) != 0 || !S_ISREG(info.st_mode) ||
                info.st_size <= 0 || info.st_size > 16384) {
                close(next);
                return false;
            }
            char buffer[4096];
            for (;;) {
                const ssize_t count = read(next, buffer, sizeof(buffer));
                if (count < 0 && errno == EINTR) continue;
                if (count == 0) {
                    close(next);
                    return !contents.empty();
                }
                if (count < 0 || contents.size() + size_t(count) > 16384) {
                    close(next);
                    contents.clear();
                    return false;
                }
                contents.append(buffer, size_t(count));
            }
        }
        directory = next;
        start = slash + 1;
    }
    close(directory);
    return false;
}

std::string extensionOf(const std::string &name) {
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return std::string();

    std::string ext = name.substr(dot + 1);
    for (char &c : ext)
        if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return ext;
}

// MiSTer resolves MGL paths against the core's home directory. ConsoleMode's own files climb
// to the root instead of using absolute paths, and that is the form known to work.
std::string toMglPath(const std::string &absolutePath) {
    return "../../../../.." + absolutePath;
}

std::string xmlEscape(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&':  out += "&amp;";  break;
        case '<':  out += "&lt;";   break;
        case '>':  out += "&gt;";   break;
        case '"':  out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:   out.push_back(c);
        }
    }
    return out;
}

} // namespace

std::string Launcher::resolveDisc(const std::string &path) {
    struct stat info {};
    int attempt = 0;
    while (stat(path.c_str(), &info) != 0) {
        if (++attempt >= kResolveAttempts) return std::string();
        usleep(kResolveDelayUs);
    }
    if (!S_ISDIR(info.st_mode)) return path;

    DIR *dir = nullptr;
    attempt = 0;
    while (!(dir = opendir(path.c_str()))) {
        if (++attempt >= kResolveAttempts) return std::string();
        usleep(kResolveDelayUs);
    }

    // One folder, so a handful of entries — nothing like walking the library.
    std::vector<std::string> candidates[sizeof(kDiscExtensions) / sizeof(kDiscExtensions[0])];
    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.empty() || name[0] == '.') continue;

        const std::string ext = extensionOf(name);
        for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
            if (ext == kDiscExtensions[i]) candidates[i].push_back(name);
    }
    closedir(dir);

    for (std::vector<std::string> &found : candidates) {
        if (found.empty()) continue;
        // Multi-disc sets name their files "… (Disc 1)", "… (Disc 2)"; sorting starts at one.
        std::sort(found.begin(), found.end());
        return path + "/" + found.front();
    }

    return path;
}

bool Launcher::writeMgl(const std::string &core, const std::string &romPath, int fileIndex,
                        char fileType) {
    std::string content = "<mistergamedescription>\n\t<rbf>" + xmlEscape(core) + "</rbf>\n";

    if (!romPath.empty()) {
        content += std::string("\t<file delay=\"2\" type=\"") + fileType + "\" index=\"" +
                   std::to_string(fileIndex) + "\" path=\"" + xmlEscape(toMglPath(romPath)) +
                   "\"/>\n";
    }
    content += "</mistergamedescription>\n";

    std::ofstream out(kMglPath, std::ios::trunc);
    if (!out) {
        error_ = std::string("cannot write ") + kMglPath;
        return false;
    }
    out << content;
    mgl_ = content;
    return true;
}

bool Launcher::sendCommand(const std::string &command) {
    if (dryRun_) {
        std::printf("launcher: dry run, would send \"%s\" for\n%s", command.c_str(),
                    mgl_.c_str());
        return true;
    }

    FILE *fifo = std::fopen(kCommandFifo, "w");
    if (!fifo) {
        error_ = std::string("cannot open ") + kCommandFifo +
                 " - is the MiSTer binary running?";
        return false;
    }

    std::fprintf(fifo, "%s\n", command.c_str());
    std::fclose(fifo);
    return true;
}

bool Launcher::launchGame(const GameSystem &system, const Game &game) {
    error_.clear();

    if (system.isArcade) return launchArcade(game.path, game.name);

    if (!system.launchable()) {
        error_ = "no core found for " + system.name;
        return false;
    }

    std::string romPath = resolveDisc(game.path);
    if (romPath.empty()) {
        error_ = "could not read " + game.name + " - no disc image found, or the drive it is "
                 "on did not respond";
        return false;
    }

    if (game.isArchive()) {
        const std::string inner = Archive::findRom(game.path, system.romExts);
        if (inner.empty()) {
            error_ = "no ROM inside " + game.name + ".zip";
            return false;
        }
        romPath += "/" + inner;
    }

    if (!writeMgl(system.core, romPath, system.fileIndex, system.fileType)) return false;
    return sendCommand(std::string("load_core ") + kMglPath);
}

bool Launcher::launchArcade(const std::string &mraPath, const std::string &name) {
    error_.clear();
    mgl_.clear();

    struct stat info {};
    if (stat(mraPath.c_str(), &info) != 0) {
        error_ = "could not read " + name + " - " + mraPath + " is gone";
        return false;
    }

    return sendCommand(std::string("load_core ") + mraPath);
}

bool Launcher::launchCore(const GameSystem &system) {
    error_.clear();

    // Arcade has no core of its own to start — each game names one.
    if (system.isArcade) {
        error_ = "Arcade has no core of its own - pick a game";
        return false;
    }

    if (!system.launchable()) {
        error_ = "no core found for " + system.name;
        return false;
    }

    if (!writeMgl(system.core, std::string(), system.fileIndex, system.fileType)) return false;
    return sendCommand(std::string("load_core ") + kMglPath);
}

bool Launcher::launchPhysicalDisc(const std::string &mglPath) {
    error_.clear();
    mgl_.clear();
    const std::string root = "/media/fat/_Disc_Cores/";
    const std::string nestedRoot = "/media/fat/mgl/_Disc_Cores/";
    if (mglPath.compare(0, root.size(), root) != 0 &&
        mglPath.compare(0, nestedRoot.size(), nestedRoot) != 0) {
        error_ = "physical-disc MGL is outside the installed disc-core folder";
        return false;
    }
    size_t componentStart = 0;
    while (componentStart < mglPath.size()) {
        const size_t slash = mglPath.find('/', componentStart);
        const std::string component = mglPath.substr(componentStart,
            slash == std::string::npos ? std::string::npos : slash - componentStart);
        if (component == "." || component == "..") {
            error_ = "physical-disc MGL path contains a traversal component";
            return false;
        }
        if (slash == std::string::npos) break;
        componentStart = slash + 1;
    }
    for (unsigned char ch : mglPath) {
        const bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                          (ch >= '0' && ch <= '9') || ch == '/' || ch == '_' ||
                          ch == '-' || ch == '.';
        if (!safe) {
            error_ = "physical-disc MGL path contains unsupported characters";
            return false;
        }
    }
    const bool nested = mglPath.compare(0, nestedRoot.size(), nestedRoot) == 0;
    const std::string &selectedRoot = nested ? nestedRoot : root;
    const std::string rootWithoutSlash = selectedRoot.substr(0, selectedRoot.size() - 1);
    if (!readPhysicalMgl(rootWithoutSlash, mglPath.substr(selectedRoot.size()), mgl_)) {
        error_ = "physical-disc MGL cannot be opened safely or exceeds 16 KiB: " + mglPath;
        return false;
    }
    if (mgl_.empty() || mgl_.find("<mistergamedescription") == std::string::npos ||
        mgl_.find("<rbf>") == std::string::npos || mgl_.find("</rbf>") == std::string::npos) {
        error_ = "physical-disc MGL is invalid: " + mglPath;
        mgl_.clear();
        return false;
    }
    return sendCommand(std::string("load_core ") + mglPath);
}
