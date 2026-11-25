#pragma once

#include <string>
#include <vector>

#include "ui-framework/kit/Components.h"

namespace ui {

// The text color with the better contrast on a given background (dark or white).
Color contrastText(const Color &background);

// The fill and label color of a focused, ready button: the usual white label on the accent, with the accent
// darkened as far as needed for 4.5 : 1; where white cannot get there (a light accent, a bright high-contrast
// highlight) a dark label on the accent itself.
struct ButtonColors { Color fill; Color text; };
ButtonColors readyButtonColors(const Palette &palette);

// ---- ActionButton: the large focusable action inside a screen (docs/components/ActionButton.md). Not a
//      dialog button: dialogs have none. Ready = can run; Idle = not focused and not running; Unavailable =
//      cannot run yet (it says why in its label).
enum class ButtonState { Ready, Unavailable };
void drawActionButton(Painter &p, const Theme &t, const Rect &r, const std::string &label, ButtonState state, bool focused);

// ---- TextField: one line of text (docs/components/TextField.md). The hint about the keys is not inside
//      the field; the help bar names them.
struct TextFieldSpec {
    Text placeholder;
    std::string value;
    bool masked = false;      // shows one * per character
    bool focused = false;     // the cursor is on the field: a focus outline
    bool editing = false;     // the keyboard is open: a text cursor after the value
};
void drawTextField(Painter &p, const Theme &t, const Rect &r, const TextFieldSpec &spec);

// ---- Keyboard: the on-screen keyboard (docs/components/Keyboard.md). A ported copy of Classic's
//      OnScreenKeyboard: 16 units wide, five rows, a number pad on the right, append-only text.
enum class KeyResult { None, Changed, Submitted };

class Keyboard {
public:
    explicit Keyboard(size_t maxLength = 64) : maxLength_(maxLength) {}
    ~Keyboard();

    void setValue(const std::string &value) { value_ = value.substr(0, maxLength_); }
    const std::string &value() const { return value_; }
    void setMaxLength(size_t maxLength) { maxLength_ = maxLength; }
    // A masked keyboard shows the Show / Hide key; masked() is whether the text is hidden right now.
    void setMasked(bool masked) { masked_ = masked; revealEnabled_ = masked; }
    bool masked() const { return masked_; }
    void clear();                    // wipes the text

    void move(int dx, int dy);       // the D-pad
    KeyResult press();               // the focused key (A)
    KeyResult backspace();           // the Delete shortcut (X)
    KeyResult space();               // the Space shortcut (Y)

    // The panel's height, and drawing it into `area` (the panel is as wide as `area` allows, centered).
    int height(const Theme &t) const;
    void draw(Painter &p, const Theme &t, const Rect &area) const;

private:
    enum class Kind { Character, Shift, Symbols, Space, Backspace, Clear, Reveal, Done };
    struct Key {
        Kind kind = Kind::Character;
        char character = 0;
        const char *label = nullptr;   // catalog key of an action key
        int row = 0, column = 0, units = 1;
    };
    std::vector<Key> keys() const;
    KeyResult erase();

    std::string value_;
    size_t maxLength_;
    int cursor_ = 0;                 // an index into keys()
    bool uppercase_ = false;         // Shift stays on until pressed again
    bool symbols_ = false;
    bool masked_ = false;
    bool revealEnabled_ = false;
};

}  // namespace ui
