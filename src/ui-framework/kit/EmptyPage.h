#pragma once

#include <string>

#include "ui-framework/kit/Components.h"

namespace ui {

// ---- EmptyPage: the one frame for "nothing to show" (docs/components/EmptyState.md): why it is empty and what to do
//      next, centered on a faint panel. Every list, grid, table and notice page uses it, so every empty screen has the
//      same panel, the same column and the same spacing.
//
//      A notice page (a state the user can do something about: no drive, no disc, unreadable) adds a tone icon and,
//      small and Faint at the bottom, a Details line (the technical reason) and one more line of data.
struct EmptyPageSpec {
    Text title;                // heading, wrapped, centered
    Text why;                  // body, Muted, wrapped
    Text next;                 // body, Primary, wrapped; may be empty
    Tone tone = Tone::Neutral; // colors the icon
    IconId icon = IconId::None;
    std::string details;       // "Details: <this>", small and Faint (data); empty = none
    std::string extra;         // one more small Faint line (data, already worded); empty = none
};

// Draws the faint panel over `area`, then the block centered inside it.
void drawEmptyPage(Painter &p, const Theme &t, const Rect &area, const EmptyPageSpec &spec);

}  // namespace ui
