#pragma once

#include <string>
#include <vector>

#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Input.h"

namespace ui {

// ---- TextEntryDialog: the one on-screen-keyboard flow (docs/components/TextEntryDialog.md). A raised panel
//      over the dimmed screen with a title, the text field and the keyboard, always in the same place and
//      the same size. It owns the keyboard, its help hints and the Changed / Submitted / Cancelled outcome.
enum class TextEntryEvent { Up, Down, Left, Right, Press, Delete, Space, Done, Cancel };
enum class TextEntryResult { None, Changed, Submitted, Cancelled };

struct TextEntrySpec {
    Text title;                  // what is being entered ("Search games")
    Text placeholder;            // shown in the field while it is empty
    std::string value;           // the starting value; restored on Cancel
    bool masked = false;         // one * per character
    bool live = true;            // Changed fires while typing; false: only Submitted / Cancelled report
    size_t maxLength = 64;       // Classic's limit; 0 = unlimited. Keys that would exceed it do nothing
    bool trim = false;           // Submitted value has leading and trailing whitespace removed
    // The words of the help hints, when the generic ones do not fit (empty = the default: Type, Done, Back).
    Text pressHint, doneHint, cancelHint;
};

class TextEntryDialog {
public:
    TextEntryDialog() = default;
    explicit TextEntryDialog(const TextEntrySpec &spec);

    // The current text. While typing: what the field shows. After Submitted: the final (trimmed) value.
    // After Cancelled: the starting value, restored.
    const std::string &value() const { return keyboard_.value(); }

    // Routes one event. The caller closes the dialog on Submitted or Cancelled; it reads value() then.
    // Changed is reported only with `live`; without it typing returns None.
    TextEntryResult handle(TextEntryEvent event);

    // Choose key, A Type, X Delete, Y Space, Start Done, B Back (the words can be replaced in the spec).
    std::vector<Hint> hints() const;

    // Dims `canvas` with the dialog scrim and draws the panel centered on it. Redraw the bottom bar above
    // it with hints() as the help (the dialog does not draw the bar).
    void draw(Painter &p, const Theme &t, const Rect &canvas) const;

    // The panel's rectangle in `canvas` (fixed size, centered).
    Rect panelRect(const Theme &t, const Rect &canvas) const;

    // The same field and keyboard without the panel, scrim or title, for a host that has its own frame (a Wizard page,
    // see EntryPage in WizardParts.h): draw the field in a rect of fieldHeight(), the keyboard in one of keyboardHeight().
    int fieldHeight(const Theme &t) const;
    int keyboardHeight(const Theme &t) const { return keyboard_.height(t); }
    void drawField(Painter &p, const Theme &t, const Rect &r) const;
    void drawKeyboard(Painter &p, const Theme &t, const Rect &r) const { keyboard_.draw(p, t, r); }
    // Wipes the typed text (a password) and the starting value.
    void clear() { keyboard_.clear(); spec_.value.clear(); }

private:
    TextEntrySpec spec_;
    Keyboard keyboard_;
};

}  // namespace ui
