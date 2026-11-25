#pragma once

#include <string>
#include <vector>

#include "ui-framework/kit/Components.h"

namespace ui {

// ---- TabRow: a row of a few short labels that switch between views or filters of the same content, with an
//      underline under the active one (docs/components/TabRow.md). The in-screen variant: body-size labels, 36 px
//      apart, the active one bold in the primary tier with a 3 px underline in the focus color.
struct TabRowSpec {
    std::vector<Text> labels;
    int active = 0;
};
// Height used by the row including the underline.
int tabRowHeight(Painter &p, const Theme &t);
// Draws the row with its left edge at `area.x` (indented 4 design px) and its top at `area.y`; returns the height used.
int drawTabRow(Painter &p, const Theme &t, const Rect &area, const TabRowSpec &spec);

}  // namespace ui
