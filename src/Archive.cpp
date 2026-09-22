#include "Archive.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr long kMaxTail = 65558;  // end record plus the largest possible comment

uint16_t read16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }

uint32_t read32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

} // namespace

std::vector<std::string> Archive::entries(const std::string &path) {
    std::vector<std::string> out;

    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return out;

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    if (size < 22) { std::fclose(file); return out; }

    const long tailLength = (size < kMaxTail) ? size : kMaxTail;
    std::vector<uint8_t> tail;
    tail.resize(size_t(tailLength));

    std::fseek(file, size - tailLength, SEEK_SET);
    if (std::fread(tail.data(), 1, size_t(tailLength), file) != size_t(tailLength)) {
        std::fclose(file);
        return out;
    }

    long endRecord = -1;
    for (long i = tailLength - 22; i >= 0; --i) {
        if (std::memcmp(&tail[size_t(i)], "PK\x05\x06", 4) == 0) { endRecord = i; break; }
    }
    if (endRecord < 0) { std::fclose(file); return out; }

    const uint16_t count = read16(&tail[size_t(endRecord) + 10]);
    const uint32_t directorySize = read32(&tail[size_t(endRecord) + 12]);
    const uint32_t directoryOffset = read32(&tail[size_t(endRecord) + 16]);

    if (!count || !directorySize || directoryOffset >= uint32_t(size)) {
        std::fclose(file);
        return out;
    }

    std::vector<uint8_t> directory;
    directory.resize(directorySize);
    std::fseek(file, long(directoryOffset), SEEK_SET);
    const size_t got = std::fread(directory.data(), 1, directorySize, file);
    std::fclose(file);
    if (got != directorySize) return out;

    size_t pos = 0;
    for (uint16_t i = 0; i < count && pos + 46 <= directory.size(); ++i) {
        if (std::memcmp(&directory[pos], "PK\x01\x02", 4) != 0) break;

        const uint16_t nameLength = read16(&directory[pos + 28]);
        const uint16_t extraLength = read16(&directory[pos + 30]);
        const uint16_t commentLength = read16(&directory[pos + 32]);
        if (pos + 46 + nameLength > directory.size()) break;

        std::string name(reinterpret_cast<const char *>(&directory[pos + 46]), nameLength);
        if (!name.empty() && name.back() != '/') out.push_back(std::move(name));

        pos += 46u + nameLength + extraLength + commentLength;
    }

    return out;
}

std::string Archive::findRom(const std::string &path, const std::vector<std::string> &extensions) {
    for (const std::string &name : entries(path)) {
        for (const std::string &ext : extensions) {
            if (ext == ".zip") continue;
            if (name.size() > ext.size() &&
                strcasecmp(name.c_str() + name.size() - ext.size(), ext.c_str()) == 0)
                return name;
        }
    }
    return std::string();
}
