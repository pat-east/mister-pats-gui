#include "MiniJson.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const JsonValue kNull;
constexpr int kMaxDepth = 64;
constexpr size_t kMaxInput = 16u * 1024u * 1024u;

class Parser {
public:
    Parser(const std::string &input) : s_(input) {}

    bool run(JsonValue &out, std::string &error) {
        skipSpace();
        if (!value(out, 0)) { error = error_.empty() ? "invalid JSON" : error_; return false; }
        skipSpace();
        if (pos_ != s_.size()) { error = "unexpected text after the JSON value"; return false; }
        return true;
    }

private:
    bool fail(const char *message) {
        if (error_.empty()) error_ = message;
        return false;
    }

    void skipSpace() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' ||
                                    s_[pos_] == '\r')) ++pos_;
    }

    bool literal(const char *word) {
        const size_t n = std::strlen(word);
        if (s_.compare(pos_, n, word) != 0) return false;
        pos_ += n;
        return true;
    }

    static void appendUtf8(std::string &out, unsigned long cp) {
        if (cp < 0x80) out += char(cp);
        else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(unsigned long &value) {
        if (pos_ + 4 > s_.size()) return false;
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = s_[pos_ + size_t(i)];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= unsigned(c - '0');
            else if (c >= 'a' && c <= 'f') value |= unsigned(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= unsigned(c - 'A' + 10);
            else return false;
        }
        pos_ += 4;
        return true;
    }

    bool string(std::string &out) {
        if (pos_ >= s_.size() || s_[pos_] != '"') return fail("expected a string");
        ++pos_;
        out.clear();
        while (pos_ < s_.size()) {
            const unsigned char c = static_cast<unsigned char>(s_[pos_++]);
            if (c == '"') return true;
            if (c < 0x20) return fail("control character in a string");
            if (c != '\\') { out += char(c); continue; }
            if (pos_ >= s_.size()) return fail("truncated escape");
            const char e = s_[pos_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned long cp = 0;
                if (!hex4(cp)) return fail("bad \\u escape");
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    unsigned long low = 0;
                    if (pos_ + 2 <= s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                        pos_ += 2;
                        if (!hex4(low) || low < 0xDC00 || low > 0xDFFF) return fail("bad surrogate pair");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else {
                        return fail("lone surrogate");
                    }
                }
                appendUtf8(out, cp);
                break;
            }
            default: return fail("unknown escape");
            }
        }
        return fail("unterminated string");
    }

    bool number(JsonValue &out) {
        const size_t start = pos_;
        if (pos_ < s_.size() && s_[pos_] == '-') ++pos_;
        while (pos_ < s_.size() && ((s_[pos_] >= '0' && s_[pos_] <= '9') || s_[pos_] == '.' ||
                                    s_[pos_] == 'e' || s_[pos_] == 'E' || s_[pos_] == '+' ||
                                    s_[pos_] == '-')) ++pos_;
        if (pos_ == start) return fail("expected a value");
        const std::string token = s_.substr(start, pos_ - start);
        char *end = nullptr;
        out.number = std::strtod(token.c_str(), &end);
        if (!end || *end != '\0') return fail("bad number");
        out.type = JsonValue::Type::Number;
        out.text = token;
        return true;
    }

    bool value(JsonValue &out, int depth) {
        if (depth > kMaxDepth) return fail("nesting too deep");
        skipSpace();
        if (pos_ >= s_.size()) return fail("unexpected end");
        const char c = s_[pos_];
        if (c == '{') {
            ++pos_;
            out.type = JsonValue::Type::Object;
            skipSpace();
            if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
            for (;;) {
                skipSpace();
                std::string key;
                if (!string(key)) return false;
                skipSpace();
                if (pos_ >= s_.size() || s_[pos_] != ':') return fail("expected ':'");
                ++pos_;
                JsonValue member;
                if (!value(member, depth + 1)) return false;
                out.members.emplace_back(std::move(key), std::move(member));
                skipSpace();
                if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
                if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            ++pos_;
            out.type = JsonValue::Type::Array;
            skipSpace();
            if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
            for (;;) {
                JsonValue item;
                if (!value(item, depth + 1)) return false;
                out.items.push_back(std::move(item));
                skipSpace();
                if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
                if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') {
            out.type = JsonValue::Type::String;
            return string(out.text);
        }
        if (c == 't') { if (!literal("true")) return fail("bad literal"); out.type = JsonValue::Type::Bool; out.boolean = true; return true; }
        if (c == 'f') { if (!literal("false")) return fail("bad literal"); out.type = JsonValue::Type::Bool; return true; }
        if (c == 'n') { if (!literal("null")) return fail("bad literal"); out.type = JsonValue::Type::Null; return true; }
        return number(out);
    }

    const std::string &s_;
    size_t pos_ = 0;
    std::string error_;
};

}  // namespace

const JsonValue &JsonValue::operator[](const std::string &key) const {
    if (type != Type::Object) return kNull;
    for (const auto &member : members)
        if (member.first == key) return member.second;
    return kNull;
}

const JsonValue &JsonValue::at(size_t index) const {
    if (type != Type::Array || index >= items.size()) return kNull;
    return items[index];
}

std::string JsonValue::asString(const std::string &fallback) const {
    if (type == Type::String || type == Type::Number) return text;
    if (type == Type::Bool) return boolean ? "true" : "false";
    return fallback;
}

long long JsonValue::asInteger(long long fallback) const {
    if (type == Type::Number) return static_cast<long long>(number);
    if (type == Type::String && !text.empty()) {
        char *end = nullptr;
        const long long v = std::strtoll(text.c_str(), &end, 10);
        if (end && *end == '\0') return v;
    }
    return fallback;
}

bool parseJson(const std::string &input, JsonValue &out, std::string &error) {
    out = JsonValue();
    if (input.size() > kMaxInput) { error = "answer too large"; return false; }
    Parser parser(input);
    return parser.run(out, error);
}
