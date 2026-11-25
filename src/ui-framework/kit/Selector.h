#pragma once

#include <string>

#include "ui-framework/kit/Components.h"

namespace ui {

// ---- Selector: one value out of a short list, stepped with Left and Right (docs/components/Selector.md).
//      A compact field with a chevron on each side and the value centered between them. The caller draws a small
//      caption above it and names the Left/Right keys in the help. Use it where a list would be too much for a
//      handful of choices on a page; in a settings row a cycling Value row does the same job.
struct SelectorSpec {
    Text value;
    bool focused = true;        // the cursor is on it: a focus outline
    bool canStepLeft = true;    // false: the left chevron is drawn faint (the first of a list that does not wrap)
    bool canStepRight = true;
};
int selectorHeight(const Theme &t);
// Draws it in `r` (use `selectorWidth` for the usual width); the field fills the rect.
int selectorWidth(const Theme &t);
void drawSelector(Painter &p, const Theme &t, const Rect &r, const SelectorSpec &spec);

}  // namespace ui
