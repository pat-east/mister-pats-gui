// Reads entry names from a zip archive's central directory.
// MiSTer expects an archived ROM to be addressed as "<archive>.zip/<inner path>"; a path
// pointing at the bare archive is treated as a directory and loads nothing.
#pragma once

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

namespace zip {

inline uint16_t rd16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }
inline uint32_t rd32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline std::vector<std::string> entries(const char *path) {
    std::vector<std::string> out;

    FILE *f = std::fopen(path, "rb");
    if (!f) return out;

    std::fseek(f, 0, SEEK_END);
    long file_size = std::ftell(f);
    if (file_size < 22) { std::fclose(f); return out; }

    // The end-of-central-directory record sits at the very end, but a trailing comment can
    // push it back by up to 64 KiB, so scan backwards for its signature.
    long tail_len = (file_size < 65558) ? file_size : 65558;
    std::vector<uint8_t> tail;
    tail.resize(size_t(tail_len));
    std::fseek(f, file_size - tail_len, SEEK_SET);
    if (std::fread(tail.data(), 1, size_t(tail_len), f) != size_t(tail_len)) {
        std::fclose(f);
        return out;
    }

    long eocd = -1;
    for (long i = tail_len - 22; i >= 0; --i) {
        if (tail[size_t(i)] == 'P' && tail[size_t(i) + 1] == 'K' &&
            tail[size_t(i) + 2] == 5 && tail[size_t(i) + 3] == 6) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) { std::fclose(f); return out; }

    const uint16_t count    = rd16(&tail[size_t(eocd) + 10]);
    const uint32_t cd_size  = rd32(&tail[size_t(eocd) + 12]);
    const uint32_t cd_off   = rd32(&tail[size_t(eocd) + 16]);
    if (!count || !cd_size || cd_off >= uint32_t(file_size)) { std::fclose(f); return out; }

    std::vector<uint8_t> cd(cd_size);
    std::fseek(f, long(cd_off), SEEK_SET);
    size_t got = std::fread(cd.data(), 1, cd_size, f);
    std::fclose(f);
    if (got != cd_size) return out;

    size_t pos = 0;
    for (uint16_t i = 0; i < count && pos + 46 <= cd.size(); ++i) {
        if (std::memcmp(&cd[pos], "PK\x01\x02", 4) != 0) break;

        const uint16_t namelen    = rd16(&cd[pos + 28]);
        const uint16_t extralen   = rd16(&cd[pos + 30]);
        const uint16_t commentlen = rd16(&cd[pos + 32]);
        if (pos + 46 + namelen > cd.size()) break;

        std::string name((const char *)&cd[pos + 46], namelen);
        if (!name.empty() && name.back() != '/') out.push_back(name);

        pos += 46u + namelen + extralen + commentlen;
    }

    return out;
}

// Picks the first entry whose extension matches one of `exts` (comma-free, with dots).
inline std::string find_rom(const char *archive, const std::vector<std::string> &exts) {
    for (const std::string &name : entries(archive)) {
        for (const std::string &ext : exts) {
            if (name.size() > ext.size() &&
                !strcasecmp(name.c_str() + name.size() - ext.size(), ext.c_str()))
                return name;
        }
    }
    return std::string();
}

} // namespace zip
