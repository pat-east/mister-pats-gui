#pragma once

#include "Paths.h"

#include <string>
#include <vector>

struct HistoryEntry {
    std::string system;
    std::string path;
    std::string name;
};

// Recently played games, newest first. Keeps its own list and additionally picks up
// ConsoleMode's `resumePath`, so a game started there also shows up here.
class History {
public:
    bool load(const std::string &file = kDefaultFile);
    bool save() const;

    void remember(const std::string &system, const std::string &path, const std::string &name);

    const std::vector<HistoryEntry> &entries() const { return entries_; }
    size_t size() const { return entries_.size(); }

    static constexpr const char *kDefaultFile = MISTER_PAT_ROOT "/history.txt";
    static constexpr const char *kConsoleModeState = "/media/fat/ConsoleMode/.state";
    static constexpr size_t kMaxEntries = 40;

private:
    // Reads `resumePath` out of ConsoleMode's state file and folds it in as the newest entry.
    void mergeConsoleModeState();

    std::vector<HistoryEntry> entries_;
    std::string file_ = kDefaultFile;
};
