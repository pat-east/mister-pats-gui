#pragma once

#include <string>

// A small on-device log at /media/fat/mister-pat/logs/debug.log, so a report of "artwork is
// missing" or "the scraper didn't find anything" can come with a timestamped record of what
// actually happened, without needing a debugger or an SSH session attached at the time. Never
// put anything identifying in a line — no IPs, no filenames from outside this app's own
// libraries — only what helps explain a bug: what this app did, and what it found while doing
// it. Rotates itself, so it cannot grow without bound.
namespace DebugLog {

enum class Level { Info, Warning, Error };

void write(Level level, const std::string &line);

inline void info(const std::string &line) { write(Level::Info, line); }
inline void warn(const std::string &line) { write(Level::Warning, line); }
inline void error(const std::string &line) { write(Level::Error, line); }

} // namespace DebugLog
