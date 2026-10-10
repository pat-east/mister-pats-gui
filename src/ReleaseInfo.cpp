#include "ReleaseInfo.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {

bool decimal(const std::string &text, uint16_t &out) {
    if (text.empty() || (text.size() > 1 && text[0] == '0')) return false;
    unsigned value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') return false;
        value = value * 10u + unsigned(ch - '0');
        if (value > 65535u) return false;
    }
    out = uint16_t(value);
    return true;
}

bool validUtf8(const std::string &text) {
    for (size_t i = 0; i < text.size();) {
        const unsigned char first = (unsigned char)text[i++];
        if (first < 0x80) continue;
        unsigned length = first >= 0xc2 && first <= 0xdf ? 2 :
                          first >= 0xe0 && first <= 0xef ? 3 :
                          first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (!length || i + length - 1 > text.size()) return false;
        unsigned code = first & ((1u << (7 - length)) - 1u);
        for (unsigned j = 1; j < length; ++j) {
            const unsigned char next = (unsigned char)text[i++];
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 0x3f);
        }
        if ((length == 2 && code < 0x80) || (length == 3 && code < 0x800) ||
            (length == 4 && code < 0x10000) || code > 0x10ffff ||
            (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    return true;
}

void appendUtf8(std::string &out, unsigned code) {
    if (code < 0x80) out.push_back(char(code));
    else if (code < 0x800) {
        out.push_back(char(0xc0 | (code >> 6)));
        out.push_back(char(0x80 | (code & 0x3f)));
    } else if (code < 0x10000) {
        out.push_back(char(0xe0 | (code >> 12)));
        out.push_back(char(0x80 | ((code >> 6) & 0x3f)));
        out.push_back(char(0x80 | (code & 0x3f)));
    } else {
        out.push_back(char(0xf0 | (code >> 18)));
        out.push_back(char(0x80 | ((code >> 12) & 0x3f)));
        out.push_back(char(0x80 | ((code >> 6) & 0x3f)));
        out.push_back(char(0x80 | (code & 0x3f)));
    }
}

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool boolean = false;
    std::string string;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;
};

class JsonParser {
public:
    explicit JsonParser(const std::string &source) : source_(source) {}
    bool read(Json &out) {
        if (!value(out, 0)) return false;
        space();
        return cursor_ == source_.size();
    }
private:
    const std::string &source_;
    size_t cursor_ = 0;

    void space() {
        while (cursor_ < source_.size() &&
               (source_[cursor_] == ' ' || source_[cursor_] == '\n' ||
                source_[cursor_] == '\r' || source_[cursor_] == '\t')) ++cursor_;
    }
    bool take(char ch) {
        space();
        if (cursor_ >= source_.size() || source_[cursor_] != ch) return false;
        ++cursor_;
        return true;
    }
    bool literal(const char *word) {
        const size_t length = std::char_traits<char>::length(word);
        if (source_.compare(cursor_, length, word) != 0) return false;
        cursor_ += length;
        return true;
    }
    bool hex4(unsigned &out) {
        if (cursor_ + 4 > source_.size()) return false;
        out = 0;
        for (int n = 0; n < 4; ++n) {
            const char ch = source_[cursor_++];
            unsigned digit = ch >= '0' && ch <= '9' ? unsigned(ch - '0') :
                             ch >= 'a' && ch <= 'f' ? unsigned(ch - 'a' + 10) :
                             ch >= 'A' && ch <= 'F' ? unsigned(ch - 'A' + 10) : 16u;
            if (digit == 16u) return false;
            out = (out << 4) | digit;
        }
        return true;
    }
    bool string(std::string &out) {
        if (!take('"')) return false;
        while (cursor_ < source_.size()) {
            unsigned char ch = (unsigned char)source_[cursor_++];
            if (ch == '"') return validUtf8(out);
            if (ch < 0x20) return false;
            if (ch != '\\') { out.push_back(char(ch)); continue; }
            if (cursor_ == source_.size()) return false;
            char escape = source_[cursor_++];
            switch (escape) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned code = 0;
                if (!hex4(code)) return false;
                if (code >= 0xd800 && code <= 0xdbff) {
                    if (cursor_ + 2 > source_.size() ||
                        source_[cursor_++] != '\\' || source_[cursor_++] != 'u') return false;
                    unsigned low = 0;
                    if (!hex4(low) || low < 0xdc00 || low > 0xdfff) return false;
                    code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                } else if (code >= 0xdc00 && code <= 0xdfff) return false;
                appendUtf8(out, code);
                break;
            }
            default: return false;
            }
        }
        return false;
    }
    bool number() {
        size_t begin = cursor_;
        if (cursor_ < source_.size() && source_[cursor_] == '-') ++cursor_;
        if (cursor_ == source_.size()) return false;
        if (source_[cursor_] == '0') ++cursor_;
        else {
            if (source_[cursor_] < '1' || source_[cursor_] > '9') return false;
            do { ++cursor_; }
            while (cursor_ < source_.size() && source_[cursor_] >= '0' && source_[cursor_] <= '9');
        }
        if (cursor_ < source_.size() && source_[cursor_] == '.') {
            ++cursor_;
            size_t start = cursor_;
            while (cursor_ < source_.size() && source_[cursor_] >= '0' &&
                   source_[cursor_] <= '9') ++cursor_;
            if (start == cursor_) return false;
        }
        if (cursor_ < source_.size() &&
            (source_[cursor_] == 'e' || source_[cursor_] == 'E')) {
            ++cursor_;
            if (cursor_ < source_.size() &&
                (source_[cursor_] == '+' || source_[cursor_] == '-')) ++cursor_;
            size_t start = cursor_;
            while (cursor_ < source_.size() && source_[cursor_] >= '0' &&
                   source_[cursor_] <= '9') ++cursor_;
            if (start == cursor_) return false;
        }
        return cursor_ > begin;
    }
    bool value(Json &out, unsigned depth) {
        if (depth > 32) return false;
        space();
        if (cursor_ == source_.size()) return false;
        char ch = source_[cursor_];
        if (ch == '"') {
            out.type = Json::Type::String;
            return string(out.string);
        }
        if (ch == '{') {
            out.type = Json::Type::Object;
            ++cursor_;
            if (take('}')) return true;
            do {
                std::string key;
                Json item;
                if (!string(key) || !take(':') || !value(item, depth + 1)) return false;
                out.object.emplace_back(std::move(key), std::move(item));
                if (take('}')) return true;
            } while (take(','));
            return false;
        }
        if (ch == '[') {
            out.type = Json::Type::Array;
            ++cursor_;
            if (take(']')) return true;
            do {
                Json item;
                if (!value(item, depth + 1)) return false;
                out.array.push_back(std::move(item));
                if (take(']')) return true;
            } while (take(','));
            return false;
        }
        if (ch == 't' && literal("true")) {
            out.type = Json::Type::Bool;
            out.boolean = true;
            return true;
        }
        if (ch == 'f' && literal("false")) {
            out.type = Json::Type::Bool;
            return true;
        }
        if (ch == 'n' && literal("null")) return true;
        out.type = Json::Type::Number;
        return number();
    }
};

const Json *uniqueField(const Json &object, const char *name) {
    const Json *found = nullptr;
    for (const auto &field : object.object) {
        if (field.first != name) continue;
        if (found) return nullptr;
        found = &field.second;
    }
    return found;
}

std::string truncateUtf8(const std::string &text, size_t limit) {
    if (text.size() <= limit) return text;
    size_t end = limit;
    while (end && ((unsigned char)text[end] & 0xc0) == 0x80) --end;
    return text.substr(0, end);
}

} // namespace

bool parseVersion(const std::string &text, Version &out) {
    const size_t a = text.find('.');
    if (a == std::string::npos) return false;
    const size_t b = text.find('.', a + 1);
    if (b == std::string::npos || text.find('.', b + 1) != std::string::npos) return false;
    return decimal(text.substr(0, a), out.major) &&
           decimal(text.substr(a + 1, b - a - 1), out.minor) &&
           decimal(text.substr(b + 1), out.patch);
}

int compareVersions(const Version &left, const Version &right) {
    if (left.major != right.major) return left.major < right.major ? -1 : 1;
    if (left.minor != right.minor) return left.minor < right.minor ? -1 : 1;
    if (left.patch != right.patch) return left.patch < right.patch ? -1 : 1;
    return 0;
}

std::string versionText(const Version &version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor) + "." +
           std::to_string(version.patch);
}

std::string libraryName(const Version &version) {
    return "mister-pats-gui-" + versionText(version) + ".so";
}

bool parseLibraryName(const std::string &name, Version &out) {
    const std::string prefix = "mister-pats-gui-";
    if (name.compare(0, prefix.size(), prefix) != 0 ||
        name.size() <= prefix.size() + 3 ||
        name.compare(name.size() - 3, 3, ".so") != 0) return false;
    return parseVersion(name.substr(prefix.size(), name.size() - prefix.size() - 3), out);
}

std::string releaseAssetUrl(const std::string &tag, const std::string &name) {
    return "https://github.com/pat-east/mister-pats-gui/releases/download/" + tag + "/" + name;
}

bool parseLatestReleaseJson(const std::string &json, LatestRelease &out, std::string &error) {
    if (json.empty() || json.size() > 1024 * 1024) { error = "GitHub reply too large or empty"; return false; }
    Json root;
    if (!JsonParser(json).read(root) || root.type != Json::Type::Object) {
        error = "invalid GitHub JSON"; return false;
    }
    const Json *tag = uniqueField(root, "tag_name");
    const Json *draft = uniqueField(root, "draft");
    const Json *prerelease = uniqueField(root, "prerelease");
    const Json *body = uniqueField(root, "body");
    const Json *assets = uniqueField(root, "assets");
    if (!tag || tag->type != Json::Type::String ||
        !draft || draft->type != Json::Type::Bool ||
        !prerelease || prerelease->type != Json::Type::Bool ||
        !body || body->type != Json::Type::String ||
        !assets || assets->type != Json::Type::Array) {
        error = "missing or duplicated release fields"; return false;
    }
    Version version;
    if (tag->string.empty() || tag->string[0] != 'v' ||
        !parseVersion(tag->string.substr(1), version) || draft->boolean || prerelease->boolean) {
        error = "release tag is not a stable vX.Y.Z"; return false;
    }
    out.tag = tag->string;
    out.notes = truncateUtf8(body->string, 2048);
    out.assets.clear();
    for (const Json &asset : assets->array) {
        if (asset.type != Json::Type::Object) { error = "invalid release asset"; return false; }
        const Json *name = uniqueField(asset, "name");
        if (!name || name->type != Json::Type::String || name->string.empty() ||
            !out.assets.insert(name->string).second) {
            error = "missing or duplicate release asset name"; return false;
        }
    }
    return true;
}

bool releaseHasAssets(const LatestRelease &release, const Version &version, std::string &error) {
    const char *required[] = {"release-info.txt", "SHA256SUMS"};
    for (const char *name : required) {
        if (!release.assets.count(name)) { error = std::string("missing asset ") + name; return false; }
    }
    if (!release.assets.count(libraryName(version))) {
        error = "missing versioned GUI library"; return false;
    }
    return true;
}

bool parseReleaseInfo(const std::string &text, ReleaseInfo &out, std::string &error) {
    if (text.empty() || text.size() > 1024 || text.back() != '\n' ||
        text.find('\r') != std::string::npos) {
        error = "invalid release-info.txt length or line endings"; return false;
    }
    const size_t a = text.find('\n');
    const size_t b = text.find('\n', a + 1);
    if (a == std::string::npos || b == std::string::npos ||
        text.find('\n', b + 1) != text.size() - 1 ||
        text.compare(0, 8, "version=") != 0 ||
        text.compare(a + 1, 11, "loader_abi=") != 0 ||
        text.compare(b + 1, 15, "gamesdb_format=") != 0) {
        error = "invalid release-info.txt fields"; return false;
    }
    uint16_t abi = 0;
    if (!parseVersion(text.substr(8, a - 8), out.version) ||
        !decimal(text.substr(a + 12, b - a - 12), abi) || abi != 1 ||
        !decimal(text.substr(b + 16, text.size() - b - 17), out.gamesDbFormat)) {
        error = "invalid release-info.txt values"; return false;
    }
    return true;
}

bool parseSha256Sums(const std::string &text, const Version &version,
                     std::map<std::string, std::string> &out, std::string &error) {
    if (text.empty() || text.size() > 4096 || text.back() != '\n') {
        error = "invalid SHA256SUMS size or ending"; return false;
    }
    const std::string names[] = {"mister-gui", "MiSTer_gui", libraryName(version), "release-info.txt"};
    out.clear();
    size_t start = 0;
    for (const std::string &name : names) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos || end - start != 66 + name.size() ||
            text.compare(start + 64, 2, "  ") != 0 ||
            text.compare(start + 66, name.size(), name) != 0) {
            error = "invalid SHA256SUMS entry"; return false;
        }
        for (size_t i = start; i < start + 64; ++i) {
            if (!((text[i] >= '0' && text[i] <= '9') ||
                  (text[i] >= 'a' && text[i] <= 'f'))) {
                error = "invalid SHA256SUMS hash"; return false;
            }
        }
        out[name] = text.substr(start, 64);
        start = end + 1;
    }
    if (start != text.size()) { error = "extra SHA256SUMS entries"; return false; }
    return true;
}
