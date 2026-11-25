#pragma once

#include <string>

namespace ui {

// The face-button layout the user's controller has. It decides what the framework prints for a button: the
// bottom face button confirms in every layout, only its name differs (Xbox A, PlayStation cross, Nintendo B).
// The layout is an input of the Theme (Theme::setButtonLayout); components never name a button themselves.
enum class ButtonLayout { Xbox, PlayStation, Nintendo };

// A button by position, independent of the layout: A is the bottom face button (confirm), B the right one (back),
// X the left one, Y the top one; the shoulders and triggers are the upper pair (left and right).
enum class PadButton { A, B, X, Y, Start, Select, ShoulderLeft, ShoulderRight, TriggerLeft, TriggerRight };

// What the layout prints for a button.
//   Xbox         A B X Y        LB RB   LT RT   START SELECT
//   PlayStation  cross circle square triangle   L1 R1   L2 R2   START SELECT
//   Nintendo     B A Y X        L R     ZL ZR   START SELECT   (the bottom button is marked B)
std::string buttonName(ButtonLayout layout, PadButton button);

// A layout from its name ("xbox", "playstation", "nintendo", any case); anything else is Xbox. And back.
ButtonLayout buttonLayoutFromName(const std::string &name);
const char *buttonLayoutName(ButtonLayout layout);

// Replaces the button placeholders of a text with the layout's names, so a sentence can name a button without
// spelling a layout: "Press {B} to go back." The placeholders (upper case, in braces): {A} {B} {X} {Y} {START}
// {SELECT} {LB} {RB} {LT} {RT} (also {L1} {R1} {L2} {R2}; they all mean the same shoulder or trigger). Anything
// else, including the numbered placeholders of ui::format ({1}), stays as written. The text is returned
// unchanged, and without copying work, when it has no "{". Components apply it when they draw a text (labels,
// text blocks, hints); a caller does not.
std::string formatButtons(ButtonLayout layout, const std::string &text);

}  // namespace ui
