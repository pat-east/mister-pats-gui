#include "GameDatabase.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "DebugLog.h"

namespace {

const char *kCatalogFile = "catalog.tsv";
const char *kRootsFile = "roots.tsv";

// Bumped when the layout changes, so an old database is rebuilt rather than misread.
// 2 added the probe column to roots.tsv. 3 stores a system's directory the same way a game's
// path is stored — root id plus what is relative to it — instead of baking in an absolute
// path at scan time, which a drive moving to a different mount point then left stale even
// though every actual game path re-resolved correctly around it.
const char *kHeader = "#mister-pat gamesdb 3";

// In the order MiSTer itself would use them. It creates usb0 through usb7 as empty
// directories whether or not anything is mounted there, which is exactly why a root is
// recognised by the library on it rather than by its path existing.
const char *kMountPoints[] = {
    "/media/fat",  "/media/usb0", "/media/usb1", "/media/usb2", "/media/usb3",
    "/media/usb4", "/media/usb5", "/media/usb6", "/media/usb7",
};

std::vector<std::string> splitTabs(const std::string &line) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t tab = line.find('\t', start);
        out.push_back(line.substr(start, tab - start));
        if (tab == std::string::npos) break;
        start = tab + 1;
    }
    return out;
}

bool isDirectory(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool makeDirectory(const std::string &path) {
    if (mkdir(path.c_str(), 0777) == 0) return true;
    return isDirectory(path);
}

void removeTsvFiles(const std::string &directory) {
    DIR *d = opendir(directory.c_str());
    if (!d) return;

    while (dirent *entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tsv") == 0)
            unlink((directory + "/" + name).c_str());
    }
    closedir(d);
}

void removeGenerationDirectory(const std::string &directory);   // forward, for the recursion below

// Anything that is not one of gamesdb's own .tsv files, encountered while clearing a whole
// generation of it out. Nothing is meant to put anything else there — MediaScraper's index
// cache did, once, and left a subdirectory removeTsvFiles() did not know to touch, which was
// enough to make rename() refuse to replace the directory ever again on every rebuild after
// the first. One misplaced file should not be able to do that again.
void removeStragglers(const std::string &directory) {
    DIR *d = opendir(directory.c_str());
    if (!d) return;

    while (dirent *entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name.empty() || name == "." || name == "..") continue;
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tsv") == 0) continue;

        const std::string full = directory + "/" + name;
        if (isDirectory(full)) removeGenerationDirectory(full);
        else unlink(full.c_str());
    }
    closedir(d);
}

// Synchronous, so only used on the rare path where an old generation is still sitting there
// unpruned when a new one needs the same name. The ordinary path never calls this — that is
// the whole point of pruneOldGenerationStep().
void removeGenerationDirectory(const std::string &directory) {
    removeTsvFiles(directory);
    removeStragglers(directory);
    rmdir(directory.c_str());
}

// A tab or a newline in a path would split the record and silently corrupt everything after
// it. Such filenames are pathological but not impossible, and dropping one entry beats
// producing a database that cannot be parsed.
bool storable(const std::string &path) {
    return path.find('\t') == std::string::npos && path.find('\n') == std::string::npos &&
           path.find('\r') == std::string::npos;
}

} // namespace

// An unused mount point is an empty directory, not an absent one, so existence says nothing.
// Contents do: a volume with anything at all on it is worth looking at, and one with nothing
// is a mount point waiting for a drive.
bool hasContents(const std::string &path) {
    DIR *d = opendir(path.c_str());
    if (!d) return false;

    bool anything = false;
    while (dirent *entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        anything = true;
        break;
    }
    closedir(d);
    return anything;
}

// Used only to accept a *fallback* match for a root that was not found where it was recorded
// (see resolveRoots()) — a bar hasContents() does not clear. A not-yet-installed system still
// gets an empty scaffolding directory from Console Mode (an NES folder with nothing but a
// palette file, say), which is "has contents" but nowhere near a real library. Counting past
// a handful of entries is enough to tell those apart without needing to know what a real
// count should be, and cheap: readdir() stops as soon as the threshold is reached.
bool looksSubstantial(const std::string &path, int minEntries) {
    DIR *d = opendir(path.c_str());
    if (!d) return false;

    int count = 0;
    while (dirent *entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (++count >= minEntries) break;
    }
    closedir(d);
    return count >= minEntries;
}

std::vector<std::string> GameDatabase::mountPoints() {
    std::vector<std::string> found;
    for (const char *candidate : kMountPoints)
        if (isDirectory(candidate) && hasContents(candidate)) found.push_back(candidate);
    return found;
}

std::string GameDatabase::pathOf(const std::string &file) const {
    return directory_ + "/" + file;
}

std::string GameDatabase::stagingPathOf(const std::string &file) const {
    return stagingDirectory() + "/" + file;
}

bool GameDatabase::exists() const {
    std::ifstream in(pathOf(kCatalogFile));
    if (!in) return false;

    std::string header;
    std::getline(in, header);
    return header == kHeader;
}

size_t GameDatabase::totalGames() const {
    size_t total = 0;
    for (const DatabaseSystem &system : systems_) total += system.count;
    return total;
}

namespace {

// A drive that is still mounting right after boot looks, for a moment, exactly like one that
// genuinely is not there — the mount point directory exists either way. Giving the recorded
// location a few short retries before treating it as gone matters more here than it looks:
// the fallback search below would otherwise go looking for the same probe directory on every
// *other* mount point, including /media/fat, where a not-yet-installed system's own empty
// scaffolding directory (created by Console Mode for every known core) can satisfy the same
// isDirectory() check and get a whole root silently, permanently pointed at the wrong drive
// for the rest of the session.
// Measured on a real device: a marginal USB bridge can disconnect and re-enumerate once
// during boot, with the filesystem not actually mounted until ~15s in. 24 * 500ms comfortably
// covers that, and it is only ever paid once, by whichever root is not ready yet — a root
// that is already there resolves on the first, immediate attempt.
constexpr int kSettleAttempts = 24;
constexpr int kSettleDelayUs = 500000;   // ~12s worst case per not-yet-ready root

// How much content a fallback candidate must have to be trusted — see looksSubstantial().
constexpr int kSubstantialEntries = 5;

} // namespace

void GameDatabase::resolveRoots(const std::vector<Root> &recorded) {
    roots_.assign(recorded.size(), std::string());
    missingRoots_.clear();
    rootsMoved_ = false;

    for (size_t i = 0; i < recorded.size(); ++i) {
        const Root &root = recorded[i];
        if (root.path.empty()) continue;

        if (root.probe.empty()) {
            roots_[i] = root.path;
            continue;
        }

        // Still where it was. The common case, and the only one that costs nothing once the
        // drive has settled.
        bool stillThere = false;
        for (int attempt = 0; attempt < kSettleAttempts; ++attempt) {
            if (attempt > 0) usleep(kSettleDelayUs);
            if (isDirectory(root.path + "/" + root.probe)) { stillThere = true; break; }
        }
        if (stillThere) {
            roots_[i] = root.path;
            continue;
        }

        // The drive order changed. Look for the same library somewhere else — but only
        // somewhere that actually looks like it, not a same-named placeholder directory.
        // Re-scanned now rather than trusting the snapshot from construction time: this
        // object is built at process startup, and the drive that is about to be found here
        // may not have finished enumerating yet at that point, even after the settle wait
        // above got the *recorded* location itself checked repeatedly.
        if (!candidatesPinned_) candidates_ = mountPoints();
        for (const std::string &candidate : candidates_) {
            if (root.path == candidate) continue;
            if (!looksSubstantial(candidate + "/" + root.probe, kSubstantialEntries)) continue;

            roots_[i] = candidate;
            rootsMoved_ = true;
            std::printf("gamesdb: root moved, %s is now %s\n", root.path.c_str(),
                        candidate.c_str());

            char line[256];
            std::snprintf(line, sizeof(line), "root moved: %s is now %s (matched by %s)",
                         root.path.c_str(), candidate.c_str(), root.probe.c_str());
            DebugLog::warn(line);
            break;
        }

        if (roots_[i].empty()) {
            missingRoots_.push_back(root.path);
            std::printf("gamesdb: root %s not found (looked for %s on every mount point)\n",
                        root.path.c_str(), root.probe.c_str());

            char line[256];
            std::snprintf(line, sizeof(line),
                         "root not found: %s (looked for %s on every mount point — its "
                         "games will not appear)",
                         root.path.c_str(), root.probe.c_str());
            DebugLog::warn(line);
        }
    }
}

bool GameDatabase::load() {
    roots_.clear();
    systems_.clear();
    missingRoots_.clear();
    rootsMoved_ = false;
    error_.clear();

    std::vector<Root> recorded;
    {
        std::ifstream in(pathOf(kRootsFile));
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            const std::vector<std::string> field = splitTabs(line);
            if (field.size() < 2) continue;

            const size_t id = size_t(std::atoi(field[0].c_str()));
            if (recorded.size() <= id) recorded.resize(id + 1);
            recorded[id].path = field[1];
            if (field.size() >= 3) recorded[id].probe = field[2];
        }
    }
    resolveRoots(recorded);

    std::ifstream in(pathOf(kCatalogFile));
    if (!in) {
        error_ = "no catalogue at " + pathOf(kCatalogFile);
        return false;
    }

    std::string line;
    std::getline(in, line);
    if (line != kHeader) {
        error_ = "database written by a different version, rebuild it";
        DebugLog::warn(error_);
        return false;
    }

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> field = splitTabs(line);
        if (field.size() < 8) continue;

        DatabaseSystem system;
        system.key = field[0];
        system.name = field[1];
        system.group = field[2];
        system.core = field[3];
        system.discBased = field[4] == "1";
        system.count = size_t(std::atoi(field[5].c_str()));

        // Same resolution a game's own path gets (see pathsFor) — left empty, not guessed
        // at, when the root it belongs to did not resolve, so a caller does not end up
        // treating an unresolved system as though it lived at some root-less relative path.
        const size_t rootId = size_t(std::atoi(field[6].c_str()));
        if (rootId < roots_.size() && !roots_[rootId].empty())
            system.dir = roots_[rootId] + "/" + field[7];

        if (system.count) systems_.push_back(system);
    }

    std::printf("gamesdb: %zu systems, %zu games, %zu roots%s\n", systems_.size(), totalGames(),
                roots_.size(), rootsMoved_ ? " (a root moved)" : "");

    char summary[160];
    std::snprintf(summary, sizeof(summary), "loaded: %zu systems, %zu games, %zu roots%s",
                 systems_.size(), totalGames(), roots_.size(),
                 missingRoots_.empty() ? "" : " (a root is missing)");
    DebugLog::info(summary);
    return !systems_.empty();
}

std::vector<std::string> GameDatabase::pathsFor(const std::string &key) const {
    std::vector<std::string> paths;

    std::ifstream in(pathOf(key + ".tsv"));
    if (!in) return paths;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> field = splitTabs(line);
        if (field.size() < 2) continue;

        const size_t id = size_t(std::atoi(field[0].c_str()));
        if (id >= roots_.size() || roots_[id].empty()) continue;
        paths.push_back(roots_[id] + "/" + field[1]);
    }

    return paths;
}

int GameDatabase::rootIdFor(const std::string &absolutePath) const {
    int best = -1;
    size_t bestLength = 0;

    for (size_t i = 0; i < writing_.size(); ++i) {
        const std::string &root = writing_[i].path;
        if (root.empty() || absolutePath.compare(0, root.size(), root) != 0) continue;
        if (root.size() > bestLength) { best = int(i); bestLength = root.size(); }
    }

    return best;
}

bool GameDatabase::beginWrite(const std::vector<std::string> &roots) {
    error_.clear();
    pending_.clear();

    writing_.clear();
    for (const std::string &root : roots) {
        Root entry;
        entry.path = root;
        writing_.push_back(entry);
    }

    if (!makeDirectory(stagingDirectory())) {
        error_ = "cannot create " + stagingDirectory();
        return false;
    }

    // Clear whatever an earlier aborted run left staged. The live database is not touched.
    removeTsvFiles(stagingDirectory());
    return true;
}

bool GameDatabase::writeSystem(const DatabaseSystem &system,
                               const std::vector<std::string> &paths) {
    if (paths.empty()) return true;

    std::ofstream out(stagingPathOf(system.key + ".tsv"), std::ios::trunc);
    if (!out) {
        error_ = "cannot write " + stagingPathOf(system.key + ".tsv");
        return false;
    }

    size_t written = 0;
    for (const std::string &path : paths) {
        const int id = rootIdFor(path);
        if (id < 0 || !storable(path)) continue;

        // +1 for the separating slash, which belongs to neither side.
        const std::string relative = path.substr(writing_[size_t(id)].path.size() + 1);
        out << id << "\t" << relative << "\n";
        ++written;
    }

    if (!written) return true;

    DatabaseSystem recorded = system;
    recorded.count = written;
    pending_.push_back(recorded);
    return true;
}

bool GameDatabase::beginFinish() {
    DebugLog::info("finishWrite: begin, " + std::to_string(pending_.size()) + " systems");

    // Recognise each root by its largest system rather than by whichever came first
    // alphabetically. A probe pointing at a one-game folder would declare the whole drive
    // missing the day that folder goes. Cheap — O(roots × systems) — so this stays one shot.
    for (size_t i = 0; i < writing_.size(); ++i) {
        const std::string &root = writing_[i].path;
        size_t best = 0;
        for (const DatabaseSystem &system : pending_) {
            if (system.dir.compare(0, root.size(), root) != 0) continue;
            if (system.count <= best) continue;
            best = system.count;
            writing_[i].probe = system.dir.substr(root.size() + 1);
        }
    }

    {
        std::ofstream out(stagingPathOf(kRootsFile), std::ios::trunc);
        if (!out) {
            error_ = "cannot write " + stagingPathOf(kRootsFile);
            DebugLog::error(error_);
            return false;
        }
        out << kHeader << "\n";
        for (size_t i = 0; i < writing_.size(); ++i)
            out << i << "\t" << writing_[i].path << "\t" << writing_[i].probe << "\n";
    }

    catalogOut_.open(stagingPathOf(kCatalogFile), std::ios::trunc);
    if (!catalogOut_) {
        error_ = "cannot write " + stagingPathOf(kCatalogFile);
        DebugLog::error(error_);
        return false;
    }
    catalogOut_ << kHeader << "\n";
    finishPosition_ = 0;
    return true;
}

bool GameDatabase::finishStep() {
    if (finishPosition_ >= pending_.size()) return false;

    const DatabaseSystem &system = pending_[finishPosition_++];

    // Root id plus what is relative to it, the same as a game's own path (see writeSystem)
    // — not the absolute directory itself, which a drive moving to a different mount point
    // would otherwise leave permanently stale.
    const int rootId = rootIdFor(system.dir);
    const std::string relative =
        rootId >= 0 ? system.dir.substr(writing_[size_t(rootId)].path.size() + 1) : "";

    catalogOut_ << system.key << "\t" << system.name << "\t" << system.group << "\t"
                << system.core << "\t" << (system.discBased ? "1" : "0") << "\t"
                << system.count << "\t" << rootId << "\t" << relative << "\n";

    return finishPosition_ < pending_.size();
}

bool GameDatabase::finishCommit() {
    catalogOut_.close();
    DebugLog::info("finishWrite: catalogue written, committing");

    // The swap. A directory rename touches one entry, not the files inside it, so it costs
    // one sync no matter how many systems the database holds. Deleting and renaming every
    // system's file individually — the previous approach — cost one sync *per file*: on a
    // card that forces a sync on every write, that turned this step into several minutes
    // with nothing visible happening, easily read as a hang. Two renames replace all of that.
    const std::string oldGeneration = directory_ + ".old";

    // Normally nothing is here — pruneOldGenerationStep() clears it out during the *next*
    // scan's own stepping. This only fires if a rebuild follows another before that had a
    // chance to run, and it is the one place still willing to block for a while to stay
    // correct.
    if (isDirectory(oldGeneration)) removeGenerationDirectory(oldGeneration);

    if (isDirectory(directory_) && rename(directory_.c_str(), oldGeneration.c_str()) != 0) {
        error_ = "cannot move the previous database aside";
        DebugLog::error(error_);
        return false;
    }

    if (rename(stagingDirectory().c_str(), directory_.c_str()) != 0) {
        error_ = "cannot activate the new database";
        DebugLog::error(error_);
        rename(oldGeneration.c_str(), directory_.c_str());   // restore rather than go dark
        return false;
    }

    systems_ = pending_;
    pending_.clear();
    pruneChecked_ = false;   // there may be a fresh generation waiting to be cleaned up

    std::vector<Root> recorded = writing_;
    resolveRoots(recorded);

    std::printf("gamesdb: written, %zu systems, %zu games\n", systems_.size(), totalGames());

    char summary[160];
    std::snprintf(summary, sizeof(summary), "scan written: %zu systems, %zu games",
                 systems_.size(), totalGames());
    DebugLog::info(summary);
    return true;
}

bool GameDatabase::finishWrite() {
    if (!beginFinish()) return false;
    while (finishStep()) {}
    return finishCommit();
}

void GameDatabase::pruneOldGenerationStep() {
    const std::string oldGeneration = directory_ + ".old";

    if (!pruneHandle_) {
        if (pruneChecked_) return;   // already looked, there was nothing there
        pruneChecked_ = true;
        pruneHandle_ = opendir(oldGeneration.c_str());
        if (!pruneHandle_) return;
    }

    DIR *d = static_cast<DIR *>(pruneHandle_);
    dirent *entry;
    while ((entry = readdir(d)) != nullptr) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
            continue;

        const std::string full = oldGeneration + "/" + entry->d_name;
        // Nothing is meant to be a directory here — see removeStragglers()'s note — but if
        // one turns up anyway, unlink() on it would just fail silently and leave this step
        // never able to finish. Worth the one synchronous exception to stay correct.
        if (isDirectory(full)) removeGenerationDirectory(full);
        else unlink(full.c_str());
        return;   // exactly one filesystem operation per call, the fallback above aside
    }

    closedir(d);
    pruneHandle_ = nullptr;
    rmdir(oldGeneration.c_str());
}
