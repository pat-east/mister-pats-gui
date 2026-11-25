#include "ControllerMap.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "SafeFileWrite.h"

namespace {

// Explicit little-endian pack/unpack rather than a raw struct memcpy — this file's byte
// layout is a fact about MiSTer's own ARM binary, not about whatever machine happens to be
// compiling or running this code (this project's own test suite runs on the host machine,
// not the MiSTer's ARM Linux).
void putLE32(std::string &out, uint32_t value) {
    out += char(value & 0xFF);
    out += char((value >> 8) & 0xFF);
    out += char((value >> 16) & 0xFF);
    out += char((value >> 24) & 0xFF);
}

uint32_t getLE32(const unsigned char *bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) |
          (uint32_t(bytes[3]) << 24);
}

} // namespace

namespace ControllerMap {

std::string pathFor(const std::string &idstr, const std::string &inputsDir) {
    return inputsDir + "/input_" + idstr + "_v3.map";
}

bool read(const std::string &idstr, Slots &out, const std::string &inputsDir) {
    out = Slots{};

    std::ifstream in(pathFor(idstr, inputsDir), std::ios::binary);
    if (!in) return false;

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string content = buffer.str();

    // A short or oversized file is not this format — treated the same as "no mapping yet"
    // rather than trusting a partial read, since a wizard building on top of garbage would
    // silently produce a worse-than-empty mapping.
    if (content.size() != kSlotCount * 4) return false;

    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(content.data());
    for (int i = 0; i < kSlotCount; ++i) out.values[i] = getLE32(bytes + i * 4);
    return true;
}

bool write(const std::string &idstr, const Slots &slots, const std::string &inputsDir) {
    std::string content;
    content.reserve(kSlotCount * 4);
    for (int i = 0; i < kSlotCount; ++i) putLE32(content, slots.values[i]);

    return SafeFileWrite::atomicWrite(pathFor(idstr, inputsDir), content);
}

} // namespace ControllerMap
