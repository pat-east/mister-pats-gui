#pragma once

#include <string>

// Shared plumbing for every write this project makes to a file it does not exclusively own
// (MiSTer.ini) or that a game core reads while running (a `.map` file). Both classes of file
// are things a torn write can leave silently, dangerously wrong — see CONTROLLER.md, "Writing
// MiSTer.ini safely" and "The .map file" — so both go through the same two primitives rather
// than each writer growing its own, easy-to-get-subtly-wrong version.
namespace SafeFileWrite {

// Temp file in the same directory, then rename() over the target — so a crash or power loss
// mid-write leaves either the old file or the new one, never a half-written one. Creates any
// missing leading directories of `path` first.
bool atomicWrite(const std::string &path, const std::string &content);

// Copies `sourcePath` into `backupDir` under a name that sorts chronologically, then deletes
// the oldest backups beyond `maxBackups`. Used before any write to a file that must never be
// allowed to end up broken (MiSTer.ini) — a backup is the recovery path if atomic write ever
// turns out not to be enough. Returns false (and leaves `backupDir` untouched) if
// `sourcePath` cannot be read at all, since backing up nothing is not a backup.
bool backupBeforeWrite(const std::string &sourcePath, const std::string &backupDir,
                       int maxBackups);

} // namespace SafeFileWrite
