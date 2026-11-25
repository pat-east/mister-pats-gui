#pragma once

#include <string>

#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Input.h"

namespace ui {

// ---- PageHeader: the one title row at the top of a tab's content window (docs/components/PageHeader.md).
//      Fixed layout on every screen: the title at the left, the search field (optional) centered on the
//      screen at a fixed size, the info text at the right. Nothing moves between tabs.
namespace PageHeaderMetrics {
constexpr int rowHeight = 60;       // title, field and info are centered in this row
constexpr int height = 72;          // the header: the row plus the gap to the body (design pixels)
constexpr int fieldWidth = 520;     // the search field, design pixels; fits the placeholder at 130 % text
constexpr int fieldHeight = 52;
constexpr int sideGap = 24;         // between the field and the title or the info
}  // namespace PageHeaderMetrics

// The search field of the header. The placeholder says what it searches ("Search games"); the value is
// the current query; `focused` is the cursor being on the field (the standard focus outline).
struct SearchFieldSpec {
    Text placeholder;
    std::string value;
    bool focused = false;
};

struct PageHeaderSpec {
    Text title;                  // heading style, left; may be empty
    Text info;                   // body, Faint, right ("Grid . 3 of 42"); never instructions; may be empty
    bool hasSearch = false;      // draw the search field
    SearchFieldSpec search;
};

// The row's height in real pixels. The body starts this far below the top of the area.
int pageHeaderHeight(const Theme &t);

// The search field's rectangle for a content area: centered horizontally on the screen, vertically in the
// header row, always the same size. Valid whether or not the header shows the field, so a tab can place
// a focus ring or jump targets from it.
Rect pageHeaderSearchRect(const Theme &t, const Rect &area);

// Draws the header at the top of `area` (the shell's content window) and returns the rect left for the body.
Rect drawPageHeader(Painter &p, const Theme &t, const Rect &area, const PageHeaderSpec &spec);

}  // namespace ui
