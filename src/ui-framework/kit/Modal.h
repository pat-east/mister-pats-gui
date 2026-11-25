#pragma once

#include <vector>

#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/List.h"

namespace ui {

// ---- Modal: a dialog over the screen (docs/components/Modal.md). One frame for every variant: a panel with
//      a title, a body, a tone, and no buttons: the actions are named in the help (the bottom-left hints).
enum class ModalKind { Info, Confirm, Error, Result, Progress, Choose, Viewer };
enum class ModalSize { Small, Medium, Large };            // 760 wide / 1120 x 760 / 1812 x 860 design pixels
enum class ModalResult { Stay, Closed, Confirmed, Cancelled };
enum class ModalEvent { Confirm, Back, Up, Down, Left, Right };

struct ModalChoice {
    Text title, detail;
    bool checked = false;
};

class Modal {
public:
    // Information: something to read, nothing to decide. A or B closes it.
    static Modal info(const Text &title, const Text &body);
    // Confirm: an action that needs a yes. A confirms, B cancels.
    static Modal confirm(const Text &title, const Text &body);
    // Error: what went wrong in plain words, what was or was not changed, a small Details line.
    // With `retry`, A is "Try again"; otherwise A closes.
    static Modal error(const Text &title, const Text &whatHappened, const Text &whatToDo, const Text &details,
                       bool retry);
    // Result: the outcome of an action, with a status word in its tone.
    static Modal result(Tone tone, const Text &title, const Text &status, const Text &body);
    // Progress: one bar and one calm line; B cancels.
    static Modal progress(const Text &title, const Text &status, float fraction);
    // Choose: a list; the cursor moves, A selects, B cancels.
    static Modal choose(const Text &title, std::vector<ModalChoice> choices);
    // Viewer: pictures at their own size on a darker scrim; Left/Right browse, A or B closes.
    static Modal viewer(std::vector<ImagePtr> images);

    ModalKind kind() const { return kind_; }
    int selected() const { return cursor_; }

    ModalResult handle(ModalEvent event);
    std::vector<Hint> hints() const;

    // Draws the panel (the stack draws the scrim).
    void draw(Painter &p, const Theme &t, const Rect &canvas) const;

private:
    Modal() = default;
    Rect panelRect(Painter &p, const Theme &t, const Rect &canvas) const;
    int paragraphsHeight(Painter &p, const Theme &t, int width) const;

    ModalKind kind_ = ModalKind::Info;
    ModalSize size_ = ModalSize::Small;
    Tone tone_ = Tone::Neutral;
    Text title_, status_, details_;
    std::vector<Text> paragraphs_;           // the first is primary text, the others muted
    bool retry_ = false;
    float fraction_ = 0.0f;
    std::vector<ModalChoice> choices_;
    std::vector<ImagePtr> images_;
    int cursor_ = 0;
    mutable ListLayout list_;                // the Choose list's scroll
};

// ---- ModalStack: what is on top, who gets the input, what the help shows (docs/components/ModalStack.md).
//      A modal over a modal dims the lower one with a lighter scrim; only the top modal acts.
class ModalStack {
public:
    void push(const Modal &modal) { stack_.push_back(modal); }
    bool empty() const { return stack_.empty(); }
    void replaceTop(const Modal &modal) { if (!stack_.empty()) stack_.back() = modal; }
    int topSelected() const { return stack_.empty() ? 0 : stack_.back().selected(); }
    size_t size() const { return stack_.size(); }

    // Routes the event to the top modal and pops it when it closes; returns what the modal did.
    ModalResult handle(ModalEvent event);

    // The top modal's help (empty when nothing is open).
    std::vector<Hint> hints() const;

    // Draws every modal, bottom first, each over its own scrim.
    void draw(Painter &p, const Theme &t, const Rect &canvas) const;

private:
    std::vector<Modal> stack_;
};

}  // namespace ui
