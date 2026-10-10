#include "UpdateService.h"

#include <cerrno>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Paths.h"
#include "UpdateFiles.h"
#include "Version.h"

namespace {

constexpr const char *kApi =
    "https://api.github.com/repos/pat-east/mister-pats-gui/releases/latest";

class CheckStaging {
public:
    CheckStaging() {
        char pattern[] = "/tmp/mister-pat-check-XXXXXX";
        if (char *made = mkdtemp(pattern)) path_ = made;
    }
    ~CheckStaging() {
        if (path_.empty()) return;
        for (const char *name : {"release.json", "release-info.txt", "SHA256SUMS"})
            unlink((path_ + "/" + name).c_str());
        rmdir(path_.c_str());
    }
    const std::string &path() const { return path_; }
private:
    std::string path_;
};

std::string readableError(std::string error) {
    if (error.size() > 180) error.resize(180);
    for (char &ch : error)
        if ((unsigned char)ch < 0x20) ch = ' ';
    return error;
}

bool databaseMismatch(uint16_t needed) {
    std::ifstream in(std::string(UpdateFiles::kRoot) + "/gamesdb/catalog.tsv");
    if (!in) return false;
    char line[128] = {};
    if (!in.getline(line, sizeof(line))) return true;
    const std::string header(line);
    const std::string prefix = "#mister-pat gamesdb ";
    if (header.compare(0, prefix.size(), prefix) != 0) return true;
    const std::string number = header.substr(prefix.size());
    if (number.empty() || (number.size() > 1 && number[0] == '0')) return true;
    unsigned value = 0;
    for (char ch : number) {
        if (ch < '0' || ch > '9') return true;
        value = value * 10u + unsigned(ch - '0');
        if (value > 65535u) return true;
    }
    return value != needed;
}

bool retryable(const DownloadResult &result) {
    return result.code == DownloadResult::Code::Network ||
           result.code == DownloadResult::Code::Timeout;
}

bool networkAddressAvailable() {
    struct ifaddrs *addresses = nullptr;
    if (getifaddrs(&addresses) != 0) return false;
    bool available = false;
    for (const struct ifaddrs *item = addresses; item; item = item->ifa_next) {
        if (!item->ifa_addr || !(item->ifa_flags & IFF_UP) ||
            (item->ifa_flags & IFF_LOOPBACK)) continue;
        const int family = item->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) { available = true; break; }
    }
    freeifaddrs(addresses);
    return available;
}

void waitBeforeRetry(const std::atomic<bool> &cancel, int seconds) {
    for (int tick = 0; tick < seconds * 10 && !cancel.load(); ++tick)
        usleep(100000);
}

bool pathExists(const std::string &path) {
    struct stat st {};
    return lstat(path.c_str(), &st) == 0;
}

} // namespace

UpdateService::UpdateService() : worker_([this] { workerLoop(); }) {}
UpdateService::~UpdateService() { shutdown(); }

void UpdateService::initializeLocalState() {
    const std::string blocked = localBlockReason();
    UpdateFiles::LibrarySelection selection;
    std::string error;
    const bool selected = UpdateFiles::librarySelection(selection, error);
    Version running;
    const bool runningValid = parseVersion(kAppVersion, running);
    std::lock_guard<std::mutex> guard(mutex_);
    snapshot_.runningVersion = kAppVersion;
    snapshot_.selectedVersion = selected ? versionText(selection.selectedVersion) : "unknown";
    snapshot_.newestInstalledVersion = selected ? versionText(selection.newestVersion) : "unknown";
    snapshot_.pinned = selected && selection.pinned;
    if (!blocked.empty()) {
        snapshot_.phase = UpdatePhase::Blocked;
        snapshot_.statusText = blocked;
    } else if (runningValid && compareVersions(running, selection.selectedVersion) != 0) {
        snapshot_.phase = UpdatePhase::RestartRequired;
        snapshot_.statusText = "Restart MiSTer to load v" + snapshot_.selectedVersion;
    } else {
        snapshot_.phase = UpdatePhase::Idle;
        snapshot_.statusText.clear();
    }
    snapshot_.canInstall = false;
}

std::string UpdateService::localBlockReason() const {
    const std::string root = UpdateFiles::kRoot;
    if (pathExists(root + "/.development-build"))
        return "Development build — restore the release before updating";
    Version running;
    if (!parseVersion(kAppVersion, running))
        return "Unknown running version — repair via SSH";
    UpdateFiles::LibrarySelection selection;
    std::string error;
    if (!UpdateFiles::librarySelection(selection, error))
        return "GUI library needs repair: " + error;
    if (!UpdateFiles::regularFile(root + "/mister-gui") ||
        !UpdateFiles::regularFile(root + "/MiSTer_gui"))
        return "Loader or MiSTer main binary needs repair via SSH";
    return {};
}

bool UpdateService::startCheck(bool automatic) {
    std::lock_guard<std::mutex> guard(mutex_);
    const UpdatePhase phase = snapshot_.phase;
    if (stopping_ || pending_ != Command::None ||
        (automatic && phase == UpdatePhase::Blocked) ||
        !(phase == UpdatePhase::Idle || phase == UpdatePhase::Current ||
          phase == UpdatePhase::Available || phase == UpdatePhase::Error ||
          phase == UpdatePhase::Blocked)) return false;
    cancel_.store(false);
    snapshot_.phase = automatic ? UpdatePhase::WaitingForInternet : UpdatePhase::Checking;
    snapshot_.statusText = automatic ? "Waiting for Internet…" : "Checking GitHub…";
    snapshot_.errorText.clear();
    snapshot_.canInstall = false;
    automaticCheckActive_ = automatic;
    pending_ = automatic ? Command::AutomaticCheck : Command::Check;
    wake_.notify_one();
    return true;
}

void UpdateService::cancelAutomaticCheck() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (automaticCheckActive_) cancel_.store(true);
}

bool UpdateService::beginConfirmation() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (stopping_ || snapshot_.phase != UpdatePhase::Available || !snapshot_.canInstall)
        return false;
    snapshot_.phase = UpdatePhase::Confirming;
    return true;
}

bool UpdateService::startInstall() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (stopping_ || pending_ != Command::None ||
        snapshot_.phase != UpdatePhase::Confirming || candidate_.tag.empty()) return false;
    cancel_.store(false);
    snapshot_.phase = UpdatePhase::Downloading;
    snapshot_.statusText = "Downloading…";
    snapshot_.bytesDone = 0;
    snapshot_.bytesTotal = 0;
    snapshot_.canInstall = false;
    pending_ = Command::Install;
    wake_.notify_one();
    return true;
}

void UpdateService::cancelBeforeCommit() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (snapshot_.phase == UpdatePhase::Confirming) {
        snapshot_.phase = UpdatePhase::Available;
        snapshot_.canInstall = true;
    } else if (snapshot_.phase == UpdatePhase::Downloading ||
               snapshot_.phase == UpdatePhase::Verifying) {
        cancel_.store(true);
    }
}

void UpdateService::dismissInstallSuccess() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (snapshot_.phase == UpdatePhase::RestartRequired)
        snapshot_.successPromptVisible = false;
}

void UpdateService::showInstallSuccess() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (snapshot_.phase == UpdatePhase::RestartRequired)
        snapshot_.successPromptVisible = true;
}

bool UpdateService::removePinnedVersion(std::string &error) {
    std::lock_guard<std::mutex> guard(mutex_);
    const UpdatePhase phase = snapshot_.phase;
    if (stopping_ || pending_ != Command::None || !snapshot_.pinned ||
        !(phase == UpdatePhase::Idle || phase == UpdatePhase::Current ||
          phase == UpdatePhase::Available || phase == UpdatePhase::Blocked ||
          phase == UpdatePhase::Error || phase == UpdatePhase::RestartRequired)) {
        error = "version pin cannot be removed while an update is active";
        return false;
    }
    Version selected;
    Version newest;
    if (!parseVersion(snapshot_.selectedVersion, selected) ||
        !parseVersion(snapshot_.newestInstalledVersion, newest)) {
        error = "cannot determine the pinned and newest GUI versions";
        return false;
    }
    UpdateInstaller installer;
    if (!installer.removeVersionPin(selected, newest, error)) return false;
    snapshot_.pinned = false;
    snapshot_.selectedVersion = versionText(newest);
    snapshot_.phase = UpdatePhase::RestartRequired;
    snapshot_.statusText = "Version pin removed; next start selects v" +
                           snapshot_.selectedVersion;
    snapshot_.canInstall = false;
    snapshot_.successPromptVisible = true;
    snapshot_.pinRemoved = true;
    return true;
}

bool UpdateService::prepareRestart(bool removePin, std::string &error) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (snapshot_.phase != UpdatePhase::RestartRequired) {
        error = "no completed update to restart for";
        return false;
    }
    Version selected;
    Version newest;
    if (!parseVersion(snapshot_.selectedVersion, selected) ||
        !parseVersion(snapshot_.newestInstalledVersion, newest)) {
        error = "cannot determine the startup version";
        return false;
    }
    UpdateFiles::LibrarySelection current;
    if (!UpdateFiles::librarySelection(current, error)) return false;
    if (current.pinned != snapshot_.pinned ||
        compareVersions(current.selectedVersion, selected) != 0 ||
        compareVersions(current.newestVersion, newest) != 0) {
        snapshot_.pinned = current.pinned;
        snapshot_.selectedVersion = versionText(current.selectedVersion);
        snapshot_.newestInstalledVersion = versionText(current.newestVersion);
        error = "startup selection changed; review the restart destination";
        return false;
    }
    if (!removePin) return true;
    if (!current.pinned) {
        error = "no version pin to remove";
        return false;
    }
    UpdateInstaller installer;
    if (!installer.removeVersionPin(selected, newest, error)) return false;
    snapshot_.pinned = false;
    snapshot_.selectedVersion = snapshot_.newestInstalledVersion;
    snapshot_.statusText = "v" + snapshot_.targetVersion +
                           " installed; restart MiSTer to load v" + snapshot_.selectedVersion;
    return true;
}

UpdateSnapshot UpdateService::snapshot() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return snapshot_;
}

bool UpdateService::commitRunning() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return snapshot_.phase == UpdatePhase::Installing;
}

void UpdateService::shutdown() {
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (stopping_) return;
        stopping_ = true;
        cancel_.store(true);
        wake_.notify_one();
    }
    if (worker_.joinable()) worker_.join();
}

void UpdateService::publishError(const std::string &error) {
    const std::string blocked = localBlockReason();
    std::lock_guard<std::mutex> guard(mutex_);
    if (cancel_.load()) return;
    snapshot_.phase = blocked.empty() ? UpdatePhase::Error : UpdatePhase::Blocked;
    snapshot_.errorText = readableError(error);
    snapshot_.statusText = blocked.empty() ? snapshot_.errorText :
                           blocked + " · check failed: " + snapshot_.errorText;
    snapshot_.canInstall = false;
}

void UpdateService::workerLoop() {
    for (;;) {
        Command command;
        {
            std::unique_lock<std::mutex> guard(mutex_);
            wake_.wait(guard, [this] { return stopping_ || pending_ != Command::None; });
            if (stopping_ && pending_ == Command::None) return;
            command = pending_;
            pending_ = Command::None;
        }
        try {
            if (command == Command::Check || command == Command::AutomaticCheck)
                check(command == Command::AutomaticCheck);
            else if (command == Command::Install) install();
        } catch (const std::exception &error) {
            publishError(error.what());
        } catch (...) {
            publishError("unexpected update error");
        }
        std::lock_guard<std::mutex> guard(mutex_);
        if (command == Command::AutomaticCheck) {
            automaticCheckActive_ = false;
            if (cancel_.load()) {
                snapshot_.phase = UpdatePhase::Idle;
                snapshot_.statusText.clear();
                snapshot_.errorText.clear();
                snapshot_.canInstall = false;
            }
        }
        if (stopping_) return;
    }
}

void UpdateService::check(bool automatic) {
    std::string blocked = localBlockReason();
    CheckStaging staging;
    if (staging.path().empty()) { publishError("cannot create check staging"); return; }
    auto fetch = [this, &staging](const std::string &url, const std::string &name,
                                  uint64_t limit) -> bool {
        DownloadResult transfer;
        for (int attempt = 0; attempt < 2; ++attempt) {
            transfer = downloader_.fetch(url, staging.path() + "/" + name, limit, 15,
                                         cancel_, {});
            if (transfer.ok() || !retryable(transfer)) break;
        }
        if (transfer.ok()) return true;
        if (cancel_.load()) return false;
        publishError(transfer.code == DownloadResult::Code::Network
                         ? "Network not ready — try Check now after Wi-Fi connects"
                         : transfer.error);
        return false;
    };

    if (automatic) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (!cancel_.load()) {
            const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) {
                publishError("Internet unavailable after 60 seconds — use Check now to retry");
                return;
            }
            if (!networkAddressAvailable()) { waitBeforeRetry(cancel_, 1); continue; }
            const DownloadResult transfer = downloader_.fetch(
                kApi, staging.path() + "/release.json", 1024u * 1024u,
                int(std::min<int64_t>(15, remaining)), cancel_, {});
            if (transfer.ok()) {
                std::lock_guard<std::mutex> guard(mutex_);
                if (cancel_.load()) return;
                snapshot_.phase = UpdatePhase::Checking;
                snapshot_.statusText = "Checking GitHub…";
                break;
            }
            if (cancel_.load()) return;
            if (!retryable(transfer)) { publishError(transfer.error); return; }
            const auto retryRemaining = std::chrono::duration_cast<std::chrono::seconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (retryRemaining > 0)
                waitBeforeRetry(cancel_, int(std::min<int64_t>(5, retryRemaining)));
        }
        if (cancel_.load()) return;
    } else if (!fetch(kApi, "release.json", 1024u * 1024u)) return;
    std::string json;
    if (!UpdateFiles::readLimited(staging.path() + "/release.json", 1024u * 1024u, json)) {
        publishError("cannot read GitHub reply");
        return;
    }
    LatestRelease latest;
    std::string error;
    if (!parseLatestReleaseJson(json, latest, error)) { publishError(error); return; }
    Version tagVersion;
    if (!parseVersion(latest.tag.substr(1), tagVersion)) {
        publishError("invalid release version"); return;
    }
    UpdateFiles::LibrarySelection selection;
    if (!UpdateFiles::librarySelection(selection, error)) { publishError(error); return; }
    if (compareVersions(tagVersion, selection.selectedVersion) <= 0) {
        std::lock_guard<std::mutex> guard(mutex_);
        snapshot_.phase = blocked.empty() ? UpdatePhase::Current : UpdatePhase::Blocked;
        snapshot_.targetVersion = versionText(tagVersion);
        snapshot_.statusText = (blocked.empty() ? "No newer release" : blocked) +
                               " (GitHub: v" + snapshot_.targetVersion + ")";
        snapshot_.canInstall = false;
        return;
    }
    if (!releaseHasAssets(latest, tagVersion, error)) { publishError(error); return; }
    if (!fetch(releaseAssetUrl(latest.tag, "release-info.txt"), "release-info.txt", 1024) ||
        !fetch(releaseAssetUrl(latest.tag, "SHA256SUMS"), "SHA256SUMS", 4096)) return;
    std::string infoText;
    std::string manifestText;
    if (!UpdateFiles::readLimited(staging.path() + "/release-info.txt", 1024, infoText) ||
        !UpdateFiles::readLimited(staging.path() + "/SHA256SUMS", 4096, manifestText)) {
        publishError("cannot read release metadata");
        return;
    }
    ReleaseInfo info;
    std::map<std::string, std::string> hashes;
    if (!parseReleaseInfo(infoText, info, error) ||
        !parseVersion(latest.tag.substr(1), tagVersion) ||
        compareVersions(info.version, tagVersion) != 0 ||
        !parseSha256Sums(manifestText, info.version, hashes, error)) {
        publishError(error.empty() ? "release versions do not match" : error);
        return;
    }
    if (!UpdateFiles::sameHash(staging.path() + "/release-info.txt",
                               hashes.at("release-info.txt"), error, &cancel_)) {
        publishError(error);
        return;
    }
    const std::string installedPath = std::string(UpdateFiles::kRoot) + "/" +
                                      libraryName(tagVersion);
    if (pathExists(installedPath)) {
        if (!UpdateFiles::regularFile(installedPath) ||
            !UpdateFiles::sameHash(installedPath, hashes.at(libraryName(tagVersion)), error,
                                   &cancel_)) {
            publishError(error.empty() ? "installed library differs from release" : error);
            return;
        }
        std::lock_guard<std::mutex> guard(mutex_);
        snapshot_.phase = blocked.empty() ? UpdatePhase::Current : UpdatePhase::Blocked;
        snapshot_.targetVersion = versionText(tagVersion);
        snapshot_.statusText = blocked.empty()
            ? "v" + snapshot_.targetVersion + " installed; pin selects v" +
                  versionText(selection.selectedVersion)
            : blocked;
        snapshot_.canInstall = false;
        return;
    }
    ReleaseCandidate found;
    found.version = info.version;
    found.tag = latest.tag;
    found.notes = latest.notes;
    found.gamesDbFormat = info.gamesDbFormat;
    found.hashes = std::move(hashes);
    if (!UpdateFiles::sha256(staging.path() + "/SHA256SUMS", found.manifestHash, error, &cancel_)) {
        publishError(error);
        return;
    }
    Version running;
    if (!parseVersion(kAppVersion, running) && blocked.empty()) {
        publishError("unknown running version");
        return;
    }

    blocked = localBlockReason();
    const bool rebuild = databaseMismatch(found.gamesDbFormat);
    std::lock_guard<std::mutex> guard(mutex_);
    candidate_ = std::move(found);
    snapshot_.targetVersion = versionText(candidate_.version);
    snapshot_.releaseNotes = candidate_.notes;
    snapshot_.databaseRebuildHint = rebuild;
    snapshot_.errorText.clear();
    snapshot_.repairPaths.clear();
    if (!blocked.empty()) {
        snapshot_.phase = UpdatePhase::Blocked;
        snapshot_.statusText = blocked + " · latest v" + snapshot_.targetVersion;
        snapshot_.canInstall = false;
    } else if (compareVersions(candidate_.version, selection.selectedVersion) > 0) {
        snapshot_.phase = UpdatePhase::Available;
        snapshot_.statusText = "v" + snapshot_.targetVersion + " available";
        snapshot_.canInstall = true;
    } else {
        snapshot_.phase = UpdatePhase::Current;
        snapshot_.statusText = "No newer release";
        snapshot_.canInstall = false;
    }
}

void UpdateService::install() {
    ReleaseCandidate selected;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        selected = candidate_;
    }
    UpdateInstaller installer;
    InstallResult completed = installer.install(
        selected, downloader_, cancel_,
        [this](InstallStep step, uint64_t bytes) {
            std::lock_guard<std::mutex> guard(mutex_);
            snapshot_.phase = step == InstallStep::Downloading ? UpdatePhase::Downloading :
                              step == InstallStep::Verifying ? UpdatePhase::Verifying :
                                                               UpdatePhase::Installing;
            snapshot_.statusText = step == InstallStep::Downloading ? "Downloading…" :
                                   step == InstallStep::Verifying ? "Verifying…" :
                                                                    "Installing…";
            snapshot_.bytesDone = bytes;
        });
    std::lock_guard<std::mutex> guard(mutex_);
    snapshot_.canInstall = false;
    snapshot_.errorText.clear();
    snapshot_.repairPaths = completed.paths;
    switch (completed.code) {
    case InstallResult::Code::Installed:
        snapshot_.phase = UpdatePhase::RestartRequired;
        snapshot_.successPromptVisible = true;
        snapshot_.pinRemoved = false;
        snapshot_.statusText = completed.message;
        {
            Version newest;
            if (!parseVersion(snapshot_.newestInstalledVersion, newest) ||
                compareVersions(candidate_.version, newest) > 0)
                snapshot_.newestInstalledVersion = snapshot_.targetVersion;
        }
        if (!snapshot_.pinned) snapshot_.selectedVersion = snapshot_.targetVersion;
        break;
    case InstallResult::Code::Cancelled:
        snapshot_.phase = UpdatePhase::Available;
        snapshot_.statusText = "v" + snapshot_.targetVersion + " available";
        snapshot_.canInstall = true;
        break;
    case InstallResult::Code::RepairRequired:
        snapshot_.phase = UpdatePhase::Blocked;
        snapshot_.statusText = readableError(completed.message);
        snapshot_.errorText = snapshot_.statusText;
        break;
    case InstallResult::Code::RetryableError:
        snapshot_.phase = UpdatePhase::Error;
        snapshot_.statusText = readableError(completed.message);
        snapshot_.errorText = snapshot_.statusText;
        break;
    }
}
