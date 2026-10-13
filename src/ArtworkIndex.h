#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// An in-memory list of the file names in every box-art directory, read once.
//
// exFAT finds a file by scanning its directory entry by entry. A system's media folder can
// hold tens of thousands of files (Game Boy Advance: about 17,000), so every lookup, and above
// all every lookup of a file that does not exist, costs tens of milliseconds on the MiSTer's
// CPU. The image loader tries several candidate names per cover; with this index it asks the
// list first and opens only files that are really there. See docs/BOXART.md.
//
// The index is built at startup while the splash screen is up and dropped and rebuilt on a
// library reload. It never writes anything. A directory that is not indexed, or a name the
// index cannot judge, answers "Unknown" and the caller falls back to asking the file system.
class ArtworkIndex {
public:
    enum class Presence { Unknown, Present, Absent };

    static ArtworkIndex &instance();
    ~ArtworkIndex();

    // Reads each directory once, in order, pausing between directories so a burst of reads does
    // not pile onto the storage at once. Replaces what was indexed before. Blocks until done.
    void build(const std::vector<std::string> &directories);
    // Same, on a background thread (used on reload so the screen stays responsive). The old
    // contents are dropped first; until the new ones arrive every answer is "Unknown".
    void buildAsync(std::vector<std::string> directories);
    void clear();

    Presence lookup(const std::string &filePath) const;
    // A file was just written (scraper, preparation): treat its name as present.
    void noteFile(const std::string &filePath);

private:
    void stopWorker();
    void scan(const std::vector<std::string> &directories, unsigned generation);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unordered_set<std::string>> directories_;
    size_t names_ = 0;
    std::thread worker_;
    std::atomic<unsigned> generation_{0};
};
