#pragma once

#include <cstdint>
#include <string>

#include "Paths.h"

// The one small corner of `/media/fat/MiSTer.ini` this project's controller-management
// feature touches: the `deadzone=` line. Everything else in that file is read back verbatim
// and rewritten unchanged — see CONTROLLER.md, "Writing MiSTer.ini safely", for why this is
// as conservative as it is: this file is what everything else this project's boot patch
// depends on, and it must never be allowed to end up broken.
namespace MisterIni {

constexpr const char *kDefaultPath = "/media/fat/MiSTer.ini";
constexpr const char *kDefaultBackupDir = MISTER_PAT_ROOT "/mister-ini-backups";
constexpr int kMaxBackups = 30;

// True and `value` set if a `deadzone=` line already exists for this exact vendor:product
// (see ControllerId::vidPid). False, `value` untouched, if MiSTer.ini is missing or has no
// matching line.
bool readDeadzone(uint32_t vidPid, int &value, const std::string &iniPath = kDefaultPath);

// Backs up MiSTer.ini (mandatory, see SafeFileWrite::backupBeforeWrite), then replaces the
// existing `deadzone=` line for this vidPid in place, or adds a new one if none matched —
// every other line, including comments, blank lines and other pads' `deadzone=` lines, is
// preserved byte for byte. Fails (returns false, MiSTer.ini untouched) if the backup itself
// could not be made, or if `iniPath` cannot be read at all.
bool writeDeadzone(uint32_t vidPid, int value, const std::string &iniPath = kDefaultPath,
                  const std::string &backupDir = kDefaultBackupDir);

} // namespace MisterIni
