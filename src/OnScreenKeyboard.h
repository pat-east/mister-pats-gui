#pragma once

#include <string>
#include <vector>

#include "Geometry.h"
#include "Input.h"

class Canvas;
class Theme;

// Controller-operated text entry shared by search and future settings flows.
class OnScreenKeyboard {
public:
    enum class Result { None, Changed, Submitted };

    explicit OnScreenKeyboard(size_t maxLength = 64) : maxLength_(maxLength) {}

    Result handle(Action action);
    Rect bounds(Theme &theme, const Rect &available) const;
    void render(Canvas &canvas, Theme &theme, const Rect &area) const;
    const std::string &value() const { return value_; }
    void clear() { value_.clear(); }
    void setValue(const std::string &value) { value_ = value.substr(0, maxLength_); }

private:
    enum class Kind { Character, Shift, Symbols, Space, Backspace, Clear, Done };
    struct Key {
        Kind kind = Kind::Character;
        char character = 0;
        const char *label = "";
        int row = 0;
        int column = 0;
        int units = 1;
    };

    std::vector<Key> keys() const;
    void moveCursor(int dx, int dy);

    std::string value_;
    size_t maxLength_;
    int cursor_ = 0;
    bool uppercase_ = false;
    bool symbols_ = false;
};
