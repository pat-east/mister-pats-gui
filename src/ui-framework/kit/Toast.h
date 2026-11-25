#pragma once

#include "ui-framework/kit/Components.h"

namespace ui {

// ---- Toast: a short, quiet confirmation that disappears by itself (docs/components/Toast.md).
// One toast at a time, in a pill just above the bottom bar, right-aligned to the content margin. Icon plus text,
// never color alone. It never takes input or focus, adds no hint of its own and never carries the only copy of
// anything the user must read or act on (docs/ACCESSIBILITY.md 2.4). Application-agnostic: the owner (the
// application's shell) keeps one `Toast`, calls show() when something happened, update() every frame and draw()
// after the page, so it lies over the content but under the dialogs' scrim logic of the caller.
//
//   Toast toast_;                                              // the shell's member
//   toast_.show(Text::data("Mega Man added to favorites"));    // or Text::key("...")
//   redraw |= toast_.update(deltaSeconds, dialogOpenOrHoldShowing);   // true when it appeared or went away
//   toast_.draw(p, t, p.bounds());                             // after the tab, when not suppressed
class Toast {
public:
    // A change that happened (Done: accent, check icon) or one line explaining something (Neutral: dot icon).
    // Other tones are drawn as Neutral: there is no failure variant (a failure is an Error modal).
    // `minSeconds` (0 = none) keeps the line up at least that long when the owner knows it matters more than the
    // default (never shorter than the policy below).
    void show(const Text &text, Tone tone = Tone::Done, float minSeconds = 0.0f);
    void hide() { left_ = 0.0f; }
    bool visible() const { return left_ > 0.0f; }

    // How long a line stays: 3.5 s, plus 1 s for every further 40 characters (the accessibility policy: long
    // enough to read, never shorter than the default).
    static float duration(const std::string &text);

    // Time passes. While `suppressed` (a dialog is open, a hold hint is showing) the toast is not drawn and its
    // time does not run, so it is not lost unseen. True when it just disappeared (the caller redraws once).
    bool update(float deltaSeconds, bool suppressed = false);

    // The pill inside `canvas` (the whole screen); empty when no toast is showing.
    Rect rect(Painter &p, const Theme &t, const Rect &canvas) const;
    void draw(Painter &p, const Theme &t, const Rect &canvas) const;

private:
    Text text_;
    Tone tone_ = Tone::Done;
    float left_ = 0.0f;
};

}  // namespace ui
