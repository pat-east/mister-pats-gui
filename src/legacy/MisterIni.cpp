#include "MisterIni.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#include "SafeFileWrite.h"

namespace {

std::string trim(const std::string &s) {
    size_t begin = 0, end = s.size();
    while (begin < end && std::isspace((unsigned char)s[begin])) ++begin;
    while (end > begin && std::isspace((unsigned char)s[end - 1])) --end;
    return s.substr(begin, end - begin);
}

// A `deadzone=` line's vidPid half, read as whatever hex MiSTer itself would parse
// (`0x`-prefixed, case-insensitive). Returns false if `line` is not a deadzone line at all.
bool parseDeadzoneLine(const std::string &line, uint32_t &vidPid, int &value) {
    const std::string trimmed = trim(line);
    if (trimmed.compare(0, 9, "deadzone=") != 0) return false;

    const std::string rhs = trimmed.substr(9);
    const size_t comma = rhs.find(',');
    if (comma == std::string::npos) return false;

    const std::string hexPart = trim(rhs.substr(0, comma));
    const std::string numPart = trim(rhs.substr(comma + 1));

    // Only the full `0x<8 hex digits>` form is matched here — the `VID:`/`PID:`-prefixed
    // partial form exists in MiSTer's own parser (see docs/CONTROLLER.md) but was never exercised
    // against real hardware this project has access to. Leaving it unmatched is the safe
    // choice for a file that must never be corrupted by a writer's own misunderstanding of a
    // form it does not fully know: worst case, a line in that form is left untouched and a
    // second, fully-qualified line is added alongside it.
    if (hexPart.size() < 3 || hexPart[0] != '0' || (hexPart[1] != 'x' && hexPart[1] != 'X'))
        return false;

    char *end = nullptr;
    const unsigned long parsedVidPid = std::strtoul(hexPart.c_str() + 2, &end, 16);
    if (!end || *end != '\0') return false;

    char *numEnd = nullptr;
    const long parsedValue = std::strtol(numPart.c_str(), &numEnd, 10);
    if (!numEnd || *numEnd != '\0') return false;

    vidPid = uint32_t(parsedVidPid);
    value = int(parsedValue);
    return true;
}

// Splits on '\n', keeping a trailing '\r' as part of each line (so the file's own line-ending
// style survives a round trip untouched) and remembering whether the file itself ended with a
// trailing newline (so one is not silently added or removed).
std::vector<std::string> splitLines(const std::string &content, bool &trailingNewline) {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) lines.push_back(line);
    trailingNewline = !content.empty() && content.back() == '\n';
    return lines;
}

std::string joinLines(const std::vector<std::string> &lines, bool trailingNewline) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size() || trailingNewline) out += '\n';
    }
    return out;
}

} // namespace

namespace MisterIni {

bool readDeadzone(uint32_t vidPid, int &value, const std::string &iniPath) {
    std::ifstream in(iniPath);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        uint32_t lineVidPid;
        int lineValue;
        if (parseDeadzoneLine(line, lineVidPid, lineValue) && lineVidPid == vidPid) {
            value = lineValue;
            return true;
        }
    }
    return false;
}

bool writeDeadzone(uint32_t vidPid, int value, const std::string &iniPath,
                  const std::string &backupDir) {
    // Mandatory, no exceptions — see docs/CONTROLLER.md, "Writing MiSTer.ini safely". Also doubles
    // as this function's own "can the file even be read" check: backupBeforeWrite refuses
    // (and touches nothing) if it cannot read `iniPath`, which is exactly the condition this
    // writer must refuse on too.
    if (!SafeFileWrite::backupBeforeWrite(iniPath, backupDir, MisterIni::kMaxBackups)) {
        std::printf("mister-ini: backup of %s failed, refusing to write\n", iniPath.c_str());
        return false;
    }

    // Read fresh, right after the backup that now holds this same content — narrower than
    // reading it before, in case something else touched the file in between.
    std::ifstream in(iniPath, std::ios::binary);
    if (!in) {
        std::printf("mister-ini: cannot read %s, refusing to write\n", iniPath.c_str());
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    in.close();

    bool trailingNewline = true;
    std::vector<std::string> lines = splitLines(buffer.str(), trailingNewline);

    char newLine[64];
    std::snprintf(newLine, sizeof(newLine), "deadzone=0x%08x, %d", vidPid, value);

    bool replaced = false;
    int sectionLine = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        uint32_t lineVidPid;
        int lineValue;
        if (parseDeadzoneLine(lines[i], lineVidPid, lineValue) && lineVidPid == vidPid) {
            lines[i] = newLine;
            replaced = true;
            break;
        }
        if (sectionLine < 0 && trim(lines[i]) == "[MiSTer]") sectionLine = int(i);
    }

    if (!replaced) {
        // Straight after the section header if there is one, so the new line lands with the
        // rest of that section's keys rather than at the very end of an otherwise unrelated
        // file; at the end if the section header was not found (an unusual MiSTer.ini, but
        // still one this must not refuse to write to).
        const size_t insertAt = sectionLine >= 0 ? size_t(sectionLine) + 1 : lines.size();
        lines.insert(lines.begin() + long(insertAt), newLine);
    }

    return SafeFileWrite::atomicWrite(iniPath, joinLines(lines, trailingNewline));
}

} // namespace MisterIni
