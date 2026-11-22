#pragma once

#include <string>
#include <utility>
#include <vector>

// A small JSON reader for API answers (ScreenScraper's are nested lists of objects). It builds a
// tree, which is fine for the sizes involved (a title search answer is up to a few MB) and keeps
// the lookups readable. Strict enough to reject truncated or garbled answers; depth and size are
// bounded so a hostile answer cannot exhaust the stack or memory.
class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> items;                              // array elements
    std::vector<std::pair<std::string, JsonValue>> members;    // object members, in order

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }
    bool isNumber() const { return type == Type::Number; }

    // Object member, or a shared null value when this is not an object or the key is missing.
    const JsonValue &operator[](const std::string &key) const;
    // Array element, or the shared null value.
    const JsonValue &at(size_t index) const;

    // Text of a string, the decimal text of a number, or the fallback. ScreenScraper sends many
    // numbers as strings and the other way round, so callers rarely care which it was.
    std::string asString(const std::string &fallback = std::string()) const;
    long long asInteger(long long fallback = 0) const;
};

// Parses `input` into `out`. Returns false and a short reason on any error.
bool parseJson(const std::string &input, JsonValue &out, std::string &error);
