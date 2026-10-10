#include "UpdateInstaller.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "UpdateFiles.h"
#include "Version.h"

namespace {

const std::string kRoot = UpdateFiles::kRoot;
const std::string kLock = kRoot + "/.update-lock";

InstallResult result(InstallResult::Code code, const std::string &message,
                     std::vector<std::string> paths = {}, bool restartRequired = false) {
    InstallResult out;
    out.code = code;
    out.message = message;
    out.paths = std::move(paths);
    out.restartRequired = restartRequired;
    return out;
}

bool parseOwner(const std::string &owner, pid_t &pid, std::string &boot) {
    const size_t line = owner.find('\n');
    if (line == std::string::npos || owner.compare(0, 4, "pid=") != 0 ||
        owner.compare(line + 1, 5, "boot=") != 0) return false;
    const std::string pidText = owner.substr(4, line - 4);
    if (pidText.empty()) return false;
    for (char ch : pidText) if (ch < '0' || ch > '9') return false;
    const long number = std::strtol(pidText.c_str(), nullptr, 10);
    if (number <= 0 || number > 2147483647) return false;
    boot = owner.substr(line + 6);
    if (boot.size() != 37 || boot.back() != '\n') return false;
    for (size_t i = 0; i < 36; ++i)
        if (!((boot[i] >= '0' && boot[i] <= '9') ||
              (boot[i] >= 'a' && boot[i] <= 'f') || boot[i] == '-')) return false;
    boot.pop_back();
    pid = pid_t(number);
    return true;
}

class UpdateLock {
public:
    ~UpdateLock() {
        if (!owned_) return;
        unlink((kLock + "/owner").c_str());
        rmdir(kLock.c_str());
    }
    bool acquire(std::string &error) {
        const std::string boot = UpdateFiles::bootId();
        if (boot.empty()) { error = "cannot read boot ID"; return false; }
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (mkdir(kLock.c_str(), 0700) == 0) {
                owned_ = true;
                const std::string owner = "pid=" + std::to_string(getpid()) + "\nboot=" + boot + "\n";
                return UpdateFiles::writeExclusive(kLock + "/owner", owner, 0600, error);
            }
            if (errno != EEXIST) {
                error = "cannot create update lock: " + std::string(std::strerror(errno));
                return false;
            }
            struct stat st {};
            if (lstat(kLock.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                error = "unsafe update lock: " + kLock;
                return false;
            }
            std::string owner;
            pid_t pid = 0;
            std::string oldBoot;
            if (!UpdateFiles::readLimited(kLock + "/owner", 200, owner) ||
                !parseOwner(owner, pid, oldBoot)) {
                error = "invalid update lock: " + kLock;
                return false;
            }
            if (oldBoot == boot && (kill(pid, 0) == 0 || errno == EPERM)) {
                error = "update already running";
                return false;
            }
            if (unlink((kLock + "/owner").c_str()) != 0 || rmdir(kLock.c_str()) != 0) {
                error = "cannot remove stale update lock: " + kLock;
                return false;
            }
        }
        error = "cannot acquire update lock";
        return false;
    }
private:
    bool owned_ = false;
};

class Stage {
public:
    ~Stage() {
        if (path_.empty()) return;
        unlink((path_ + "/library.so").c_str());
        rmdir(path_.c_str());
    }
    bool create(std::string &error) {
        const std::string parent = kRoot + "/.update-staging";
        if (mkdir(parent.c_str(), 0700) != 0 && errno != EEXIST) {
            error = "cannot create " + parent;
            return false;
        }
        struct stat st {};
        if (lstat(parent.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            error = "unsafe staging directory: " + parent;
            return false;
        }
        std::string pattern = parent + "/run-XXXXXX";
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        if (!mkdtemp(buffer.data())) { error = "cannot create update staging"; return false; }
        path_ = buffer.data();
        return true;
    }
    std::string library() const { return path_ + "/library.so"; }
private:
    std::string path_;
};

bool retryable(const DownloadResult &download) {
    return download.code == DownloadResult::Code::Network ||
           download.code == DownloadResult::Code::Timeout;
}

} // namespace

InstallResult UpdateInstaller::install(const ReleaseCandidate &candidate,
                                       const VerifiedDownloader &downloader,
                                       std::atomic<bool> &cancel, const Progress &progress) {
    std::string error;
    UpdateLock lock;
    if (!lock.acquire(error)) return result(InstallResult::Code::RetryableError, error);
    const std::string marker = kRoot + "/.development-build";
    struct stat st {};
    if (lstat(marker.c_str(), &st) == 0)
        return result(InstallResult::Code::RepairRequired,
                      "development build: restore the release before updating", {marker});
    if (!UpdateFiles::regularFile(kRoot + "/MiSTer_gui") ||
        !UpdateFiles::regularFile(kRoot + "/mister-gui"))
        return result(InstallResult::Code::RepairRequired,
                      "loader or MiSTer main binary needs repair");
    UpdateFiles::LibrarySelection selection;
    if (!UpdateFiles::librarySelection(selection, error))
        return result(InstallResult::Code::RepairRequired, error);
    Version running;
    if (!parseVersion(kAppVersion, running) ||
        compareVersions(running, selection.selectedVersion) != 0)
        return result(InstallResult::Code::RepairRequired,
                      "running GUI differs from selected library; restart first");
    if (compareVersions(candidate.version, running) <= 0)
        return result(InstallResult::Code::RetryableError, "release is not newer");

    const std::string name = libraryName(candidate.version);
    const auto hash = candidate.hashes.find(name);
    if (hash == candidate.hashes.end())
        return result(InstallResult::Code::RepairRequired, "missing library hash");
    const std::string destination = kRoot + "/" + name;
    if (lstat(destination.c_str(), &st) == 0) {
        if (!S_ISREG(st.st_mode) || !UpdateFiles::sameHash(destination, hash->second, error))
            return result(InstallResult::Code::RepairRequired,
                          error.empty() ? "installed library differs: " + destination : error,
                          {destination});
        return result(InstallResult::Code::Installed,
                      "v" + versionText(candidate.version) + " already installed" +
                          (selection.pinned ? "; version pin remains active" : "; restart MiSTer"),
                      {}, !selection.pinned);
    }
    if (errno != ENOENT)
        return result(InstallResult::Code::RepairRequired,
                      "cannot inspect " + destination, {destination});
    Stage stage;
    if (!stage.create(error)) return result(InstallResult::Code::RetryableError, error);
    DownloadResult transfer;
    for (int attempt = 0; attempt < 2; ++attempt) {
        transfer = downloader.fetch(releaseAssetUrl(candidate.tag, name), stage.library(),
                                    64u * 1024u * 1024u, 120, cancel,
                                    [&](uint64_t bytes) {
                                        if (progress) progress(InstallStep::Downloading, bytes);
                                    });
        if (transfer.ok() || !retryable(transfer)) break;
    }
    if (!transfer.ok())
        return result(transfer.code == DownloadResult::Code::Cancelled
                          ? InstallResult::Code::Cancelled
                          : InstallResult::Code::RetryableError, transfer.error);
    if (progress) progress(InstallStep::Verifying, transfer.bytes);
    if (cancel.load()) return result(InstallResult::Code::Cancelled, "cancelled");
    if (!UpdateFiles::isElf(stage.library()) ||
        !UpdateFiles::sameHash(stage.library(), hash->second, error, &cancel))
        return result(cancel.load() ? InstallResult::Code::Cancelled
                                    : InstallResult::Code::RetryableError,
                      error.empty() ? "invalid GUI library" : error);
    struct statvfs space {};
    if (statvfs(kRoot.c_str(), &space) != 0 ||
        uint64_t(space.f_bavail) * uint64_t(space.f_frsize) < transfer.bytes + 1024u * 1024u)
        return result(InstallResult::Code::RetryableError, "not enough free space for GUI library");
    if (cancel.load()) return result(InstallResult::Code::Cancelled, "cancelled");
    const std::string temporary = destination + ".new-" + std::to_string(getpid());
    unlink(temporary.c_str());
    if (!UpdateFiles::copyFile(stage.library(), temporary, 0755, error) ||
        !UpdateFiles::sameHash(temporary, hash->second, error, &cancel)) {
        unlink(temporary.c_str());
        return result(cancel.load() ? InstallResult::Code::Cancelled
                                    : InstallResult::Code::RetryableError, error);
    }
    if (cancel.load()) { unlink(temporary.c_str()); return result(InstallResult::Code::Cancelled, "cancelled"); }
    if (progress) progress(InstallStep::Installing, transfer.bytes);
    // The sole commit is publishing an immutable, verified versioned library.
    if (lstat(destination.c_str(), &st) == 0 || errno != ENOENT) {
        unlink(temporary.c_str());
        return result(InstallResult::Code::RepairRequired,
                      "library name became occupied: " + destination, {destination});
    }
    if (rename(temporary.c_str(), destination.c_str()) != 0) {
        unlink(temporary.c_str());
        return result(InstallResult::Code::RetryableError,
                      "cannot publish " + destination + ": " + std::strerror(errno));
    }
    sync();
    return result(InstallResult::Code::Installed,
                  "v" + versionText(candidate.version) + " installed" +
                      (selection.pinned ? "; version pin remains active" : " — restart MiSTer"),
                  {}, !selection.pinned);
}
