#include "LibraryScan.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <dirent.h>
#include <sys/stat.h>

#include "DebugLog.h"

namespace {

bool isDirectory(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// A disc-based system's BIOS commonly sits in region folders right next to the games —
// MegaCD/Europe, MegaCD/Japan, MegaCD/USA, each holding nothing but a cd_bios.rom — which
// otherwise look exactly like any other disc folder from the outside. Peeking inside for an
// actual disc image is what tells the two apart.
bool folderHasGame(const std::string &path, const std::vector<std::string> &extensions) {
    DIR *d = opendir(path.c_str());
    if (!d) return false;

    bool found = false;
    while (dirent *entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name.empty() || name[0] == '.') continue;
        if (SystemCatalog::looksLikeGame(name, extensions)) { found = true; break; }
    }
    closedir(d);
    return found;
}

} // namespace

void LibraryScan::start(const std::vector<std::string> &roots) {
    roots_ = roots.empty() ? detectRoots() : roots;
    queue_.clear();
    position_ = written_ = games_ = 0;
    current_.clear();
    error_.clear();
    writeStarted_ = false;
    arcadeStarted_ = false;
    arcadeKept_ = 0;
    arcadePrepared_ = false;
    arcadeLists_.clear();
    arcadeListsDone_ = 0;
    manufacturers_.clear();
    categories_.clear();
    state_ = State::Discovering;
}

void LibraryScan::cancel() {
    if (running()) {
        error_ = "cancelled";
        state_ = State::Failed;
    }
}

float LibraryScan::progress() const {
    if (state_ == State::Done) return 1.0f;
    if (state_ == State::Writing) {
        const size_t total = database_.finishTotal();
        return total ? float(database_.finishDone()) / float(total) : 1.0f;
    }
    if (queue_.empty()) return 0.0f;
    if (position_ >= queue_.size() && arcadeStarted_) return arcade_.progress();
    return float(position_) / float(queue_.size());
}

std::string LibraryScan::statusLine() const {
    char buffer[160];

    switch (state_) {
    case State::Idle:
        return "not started";
    case State::Discovering:
        return "Looking for systems on " + std::to_string(roots_.size()) + " volume(s) …";
    case State::Scanning:
        if (position_ >= queue_.size()) {
            if (arcadePrepared_) {
                std::snprintf(buffer, sizeof(buffer),
                              "Arcade  ·  %zu games that work  ·  writing lists %zu / %zu",
                              arcadeKept_, std::min(arcadeListsDone_, arcadeLists_.size()),
                              arcadeLists_.size());
                return buffer;
            }
            return "Arcade  ·  " + arcade_.statusLine();
        }
        std::snprintf(buffer, sizeof(buffer), "%zu / %zu  ·  %s  ·  %zu games so far",
                      position_, queue_.size(), current_.c_str(), games_);
        return buffer;
    case State::Writing:
        std::snprintf(buffer, sizeof(buffer), "Writing the catalogue …  %zu / %zu",
                      database_.finishDone(), database_.finishTotal());
        return buffer;
    case State::Done:
        std::snprintf(buffer, sizeof(buffer), "%zu systems, %zu games", written_, games_);
        return buffer;
    case State::Failed:
    default:
        return error_.empty() ? "failed" : error_;
    }
}

std::vector<std::string> LibraryScan::scanOne(const CatalogEntry &system) const {
    std::vector<std::string> games;
    std::vector<std::string> subdirectories;

    DIR *d = opendir(system.dir.c_str());
    if (!d) return games;

    while (dirent *entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name.empty() || name[0] == '.') continue;

        const std::string full = system.dir + "/" + name;

        if (isDirectory(full)) {
            if (SystemCatalog::isIgnoredDirectory(name)) continue;

            // A CD game is a folder of tracks, so the folder is the game — but only if it
            // actually holds one; a BIOS folder (MegaCD/Europe, /Japan, /USA, each holding
            // nothing but cd_bios.rom) looks exactly like one from the outside otherwise.
            // For everything else a folder is just how someone chose to organise their ROMs.
            if (system.discBased) {
                if (folderHasGame(full, system.extensions)) games.push_back(full);
            } else {
                subdirectories.push_back(full);
            }
            continue;
        }

        if (SystemCatalog::looksLikeGame(name, system.extensions)) games.push_back(full);
    }
    closedir(d);

    // One level down, and no further. Libraries are sometimes sorted into A/B/C folders;
    // they are never nested deeply, and unbounded recursion on a large drive is exactly the
    // access pattern to avoid.
    for (const std::string &sub : subdirectories) {
        DIR *inner = opendir(sub.c_str());
        if (!inner) continue;

        while (dirent *entry = readdir(inner)) {
            const std::string name = entry->d_name;
            if (name.empty() || name[0] == '.') continue;

            const std::string full = sub + "/" + name;
            if (isDirectory(full)) continue;
            if (SystemCatalog::looksLikeGame(name, system.extensions)) games.push_back(full);
        }
        closedir(inner);
    }

    std::sort(games.begin(), games.end(), [](const std::string &a, const std::string &b) {
        const size_t sa = a.find_last_of('/'), sb = b.find_last_of('/');
        return strcasecmp(a.c_str() + (sa == std::string::npos ? 0 : sa + 1),
                          b.c_str() + (sb == std::string::npos ? 0 : sb + 1)) < 0;
    });

    return games;
}

namespace {

// A file name for a group: the substitution LibretroIndex::open() already uses for its own
// cache file, extended to anything else a filesystem might object to. Keys that would land on
// the same name — including names differing only in case, which FAT does not tell apart —
// get a number, so two manufacturers never end up sharing one list.
std::string groupKey(const char *prefix, const std::string &name, std::set<std::string> &used) {
    std::string safe;
    for (char c : name) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                           c == '(' || c == ')' || c == '&' || c == ',' || c == '\'';
        safe.push_back(plain ? c : '_');
    }
    if (safe.size() > 60) safe.resize(60);

    const std::string base = std::string(prefix) + safe;
    std::string key = base;
    for (int n = 2;; ++n) {
        std::string lower = key;
        for (char &c : lower)
            if (c >= 'A' && c <= 'Z') c = char(c + 32);
        if (used.insert(lower).second) return key;
        key = base + "-" + std::to_string(n);
    }
}

bool nameLess(const std::string &a, const std::string &b) {
    const size_t sa = a.find_last_of('/'), sb = b.find_last_of('/');
    return strcasecmp(a.c_str() + (sa == std::string::npos ? 0 : sa + 1),
                      b.c_str() + (sb == std::string::npos ? 0 : sb + 1)) < 0;
}

// Games of one manufacturer or category. MRA authors spell the same thing differently —
// "Beat 'Em Up" and "Beat 'em Up" — and two tiles for one category is worse than either
// spelling, so names are grouped without regard to case or stray spaces, and shown under
// whichever spelling was met first.
struct Bucket {
    std::string name;
    std::vector<std::string> paths;
};
using Buckets = std::map<std::string, Bucket>;

std::string bucketKey(const std::string &name) {
    std::string out;
    bool space = false;
    for (char c : name) {
        if (c == ' ' || c == '\t') { space = !out.empty(); continue; }
        if (space) out.push_back(' ');
        space = false;
        out.push_back(c >= 'A' && c <= 'Z' ? char(c + 32) : c);
    }
    return out;
}

void addToBucket(Buckets &buckets, const std::string &name, const std::string &path) {
    Bucket &bucket = buckets[bucketKey(name)];
    if (bucket.name.empty()) bucket.name = name;
    bucket.paths.push_back(path);
}

void buildGroups(const char *prefix, const Buckets &by, std::vector<DatabaseGroup> &groups,
                 std::vector<std::pair<std::string, std::vector<std::string>>> &lists) {
    std::set<std::string> used;
    for (const auto &entry : by) {
        DatabaseGroup group;
        group.name = entry.second.name;
        group.count = entry.second.paths.size();
        group.key = groupKey(prefix, group.name, used);
        groups.push_back(group);
        lists.push_back({group.key, entry.second.paths});
    }
}

} // namespace

bool LibraryScan::prepareArcade() {
    arcadePrepared_ = true;

    // Only what can actually start, judged by the same core / ROM zip / CRC check the
    // Arcade Games table shows — the library lists what works, the table explains the rest.
    // The same game can sit on both the card and a drive, because people keep a full copy on
    // each; it is listed once, since a .mra is found by its path below _Arcade and MiSTer
    // searches the ROM zips on every volume regardless.
    std::set<std::string> seen;
    std::vector<const ArcadeEntry *> kept;
    for (const ArcadeEntry &entry : arcade_.entries()) {
        if (!entry.mightWork()) continue;

        const size_t marker = entry.mraPath.find("/_Arcade/");
        const std::string relative =
            marker == std::string::npos ? entry.mraPath : entry.mraPath.substr(marker + 9);
        if (seen.insert(relative).second) kept.push_back(&entry);
    }
    if (kept.empty()) return true;

    std::sort(kept.begin(), kept.end(), [](const ArcadeEntry *a, const ArcadeEntry *b) {
        return nameLess(a->mraPath, b->mraPath);
    });

    // A game with no manufacturer or category is still a game. Dropping it from those two
    // views would make it findable in one place only, without saying so.
    std::vector<std::string> all;
    Buckets byManufacturer, byCategory;
    for (const ArcadeEntry *entry : kept) {
        all.push_back(entry->mraPath);
        addToBucket(byManufacturer, entry->manufacturer.empty() ? "(Unknown)" : entry->manufacturer,
                    entry->mraPath);
        addToBucket(byCategory, entry->category.empty() ? "(Uncategorized)" : entry->category,
                    entry->mraPath);
    }

    // The catalogue records one directory per system. For Arcade that is only a probe for
    // recognising the volume again — the games themselves carry their own root each.
    std::string home;
    for (const std::string &root : roots_) {
        if (isDirectory(root + "/_Arcade")) { home = root + "/_Arcade"; break; }
    }

    DatabaseSystem record;
    record.key = "Arcade";
    record.name = "Arcade";
    record.group = "Arcade";
    record.dir = home;

    if (!database_.writeSystem(record, all)) {
        fail(database_.lastError());
        return false;
    }

    std::vector<std::pair<std::string, std::vector<std::string>>> lists;
    buildGroups("Arcade-Manufacturer-", byManufacturer, manufacturers_, lists);
    buildGroups("Arcade-Category-", byCategory, categories_, lists);
    for (auto &list : lists) arcadeLists_.push_back({list.first, std::move(list.second)});

    arcadeKept_ = all.size();
    ++written_;
    games_ += all.size();
    return true;
}

bool LibraryScan::writeArcadeStep() {
    if (arcadeListsDone_ < arcadeLists_.size()) {
        const ArcadeList &list = arcadeLists_[arcadeListsDone_++];
        if (!database_.writeGroupList(list.key, list.paths)) fail(database_.lastError());
        return true;
    }

    // The two catalogues go last, so a scan that stops part-way never leaves one naming
    // a list that was not written.
    if (arcadeListsDone_ == arcadeLists_.size()) {
        ++arcadeListsDone_;
        if (!manufacturers_.empty() &&
            (!database_.writeGroupCatalog(GameDatabase::kArcadeManufacturers, manufacturers_) ||
             !database_.writeGroupCatalog(GameDatabase::kArcadeCategories, categories_)))
            fail(database_.lastError());
        return true;
    }
    return false;
}

void LibraryScan::fail(std::string message) {
    error_ = std::move(message);
    DebugLog::warn("scan: " + error_);
    state_ = State::Failed;
}

void LibraryScan::step() {
    switch (state_) {
    case State::Discovering: {
        if (roots_.empty()) {
            fail("no volumes found");
            return;
        }

        // Arcade has no game directory next to a core to be found by the discovery below —
        // its games are the .mra files under _Arcade, and its cores sit in _Arcade/cores,
        // which the core index does not list — so a machine holding only those is still
        // worth scanning.
        bool hasArcade = false;
        for (const std::string &root : roots_)
            if (isDirectory(root + "/_Arcade")) hasArcade = true;

        cores_.scan(roots_);
        if (cores_.empty() && !hasArcade) {
            fail("no cores found — is this a MiSTer?");
            return;
        }

        queue_ = SystemCatalog::discover(roots_, cores_);
        if (queue_.empty() && !hasArcade) {
            fail("no game directories found next to an installed core");
            return;
        }

        if (!database_.beginWrite(roots_)) {
            fail(database_.lastError());
            return;
        }

        std::printf("scan: %zu candidate systems on %zu volumes\n", queue_.size(),
                    roots_.size());
        state_ = State::Scanning;
        return;
    }

    case State::Scanning: {
        // Piggybacks on a phase that already takes a while and already moves one step per
        // frame, so a few extra filesystem operations here are the last place anyone notices
        // them. This is what clears out whatever the *previous* rebuild's finishWrite() swap
        // left behind — see GameDatabase::pruneOldGenerationStep().
        database_.pruneOldGenerationStep();

        if (position_ >= queue_.size()) {
            if (!arcadeStarted_) {
                arcadeStarted_ = true;
                arcade_.start(roots_, romSearchOrder_);
                current_ = "Arcade";
                return;
            }
            if (arcade_.running()) {
                arcade_.step();
                return;
            }
            if (!arcadePrepared_) {
                prepareArcade();
                return;
            }
            if (writeArcadeStep()) return;
            state_ = State::Writing;
            return;
        }

        const CatalogEntry &system = queue_[position_];
        current_ = system.name;

        const std::vector<std::string> games = scanOne(system);
        if (!games.empty()) {
            DatabaseSystem record;
            record.key = system.key;
            record.name = system.name;
            record.group = system.group;
            record.core = system.core;
            record.dir = system.dir;
            record.discBased = system.discBased;

            if (!database_.writeSystem(record, games)) {
                fail(database_.lastError());
                return;
            }

            ++written_;
            games_ += games.size();
        }

        ++position_;
        return;
    }

    case State::Writing: {
        if (!writeStarted_) {
            writeStarted_ = true;
            if (!database_.beginFinish()) {
                fail(database_.lastError());
                return;
            }
            // Nothing to write a catalogue line for — go straight to committing rather
            // than waiting for a finishStep() call that would never have anything to do.
            if (database_.finishTotal() > 0) return;
        } else if (database_.finishStep()) {
            return;   // one more line written this call; more still to go
        }

        if (!database_.finishCommit()) {
            fail(database_.lastError());
            return;
        }
        state_ = State::Done;
        return;
    }

    default:
        return;
    }
}
