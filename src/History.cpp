#include "History.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace {

std::string basenameOf(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    return (dot == std::string::npos) ? name : name.substr(0, dot);
}

// Minimal extraction of one string value; enough for a flat state file and avoids pulling in
// a JSON library for a single field.
std::string jsonValue(const std::string &text, const std::string &key) {
    const size_t at = text.find("\"" + key + "\"");
    if (at == std::string::npos) return std::string();

    const size_t colon = text.find(':', at);
    if (colon == std::string::npos) return std::string();

    const size_t open = text.find('"', colon);
    if (open == std::string::npos) return std::string();

    std::string value;
    for (size_t i = open + 1; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {  // paths arrive as \/media\/fat
            value.push_back(text[++i]);
            continue;
        }
        if (text[i] == '"') break;
        value.push_back(text[i]);
    }
    return value;
}

} // namespace

bool History::load(const std::string &file) {
    file_ = file;
    entries_.clear();

    std::ifstream in(file_);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        const size_t first = line.find('\t');
        if (first == std::string::npos) continue;
        const size_t second = line.find('\t', first + 1);

        HistoryEntry entry;
        entry.system = line.substr(0, first);
        if (second == std::string::npos) {
            entry.path = line.substr(first + 1);
        } else {
            entry.path = line.substr(first + 1, second - first - 1);
            entry.name = line.substr(second + 1);
        }
        if (entry.name.empty()) entry.name = basenameOf(entry.path);

        entries_.push_back(std::move(entry));
    }

    mergeConsoleModeState();
    return true;
}

void History::mergeConsoleModeState() {
    std::ifstream in(kConsoleModeState);
    if (!in) return;

    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string resume = jsonValue(buffer.str(), "resumePath");
    if (resume.empty()) return;

    if (std::any_of(entries_.begin(), entries_.end(),
                    [&](const HistoryEntry &e) { return e.path == resume; }))
        return;

    // The state file does not name the system; the library resolves it from the path later.
    entries_.insert(entries_.begin(), HistoryEntry{std::string(), resume, basenameOf(resume)});
    std::printf("history: picked up ConsoleMode resume path\n");
}

bool History::save() const {
    std::ofstream out(file_, std::ios::trunc);
    if (!out) {
        std::printf("history: cannot write %s\n", file_.c_str());
        return false;
    }

    for (const HistoryEntry &entry : entries_)
        out << entry.system << '\t' << entry.path << '\t' << entry.name << '\n';

    return true;
}

void History::remember(const std::string &system, const std::string &path,
                       const std::string &name) {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&](const HistoryEntry &e) { return e.path == path; }),
                   entries_.end());

    entries_.insert(entries_.begin(), HistoryEntry{system, path, name});
    if (entries_.size() > kMaxEntries) entries_.resize(kMaxEntries);

    save();
}
