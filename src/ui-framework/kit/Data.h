#pragma once

#include <string>
#include <vector>

#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Primitives.h"

namespace ui {

// ---- Table: weighted columns, a header, text and flag cells (docs/components/Table.md)
enum class CellKind { Text, Flag };
struct TableColumn {
    Text header;
    int weight = 10;             // relative; the weights of all columns add up to 100
    CellKind kind = CellKind::Text;
};
struct TableCell {
    std::string text;            // Text cells
    bool flag = false;           // Flag cells: check (true) or cross (false)
};
using TableRow = std::vector<TableCell>;
// `cursor` is the focused row, or -1 for none; rows are drawn from `first`.
void drawTable(Painter &p, const Theme &t, const Rect &area, const std::vector<TableColumn> &columns,
               const std::vector<TableRow> &rows, int cursor, int first = 0);
int tableRowHeight(const Theme &t);

// ---- KeyValueList: label/value rows with headings and spacers (docs/components/KeyValueList.md)
enum class KeyValueKind { Row, Heading, Spacer };
struct KeyValueEntry {
    KeyValueKind kind = KeyValueKind::Row;
    Text label;
    Text value;
    bool hasStatus = false;      // the value is a Status (icon + word in a tone)
    Tone tone = Tone::Neutral;
};
// Scroll state of a list taller than its area (docs/components/KeyValueList.md: it scrolls, it is never cut). The owner keeps one
// KeyValueView, changes `first` on Up/Down (any value: the draw clamps it) and reads `scrolls` / `visible` / `total` after the
// draw (for the hint and the position). `minLabelPercent` widens the label column to at least that share of the width (still
// at most 38 %), so two pages of one area put their values in the same column.
struct KeyValueView {
    int first = 0;               // the first entry shown
    int minLabelPercent = 0;
    int total = 0, visible = 0;  // out: entries, entries fully shown
    bool scrolls = false;        // out: the list is taller than the area (a scroll bar is drawn)
};
// Returns the height used. The row height follows the text size (rows grow at Largest).
int drawKeyValueList(Painter &p, const Theme &t, const Rect &area, const std::vector<KeyValueEntry> &entries, KeyValueView &view);
int drawKeyValueList(Painter &p, const Theme &t, const Rect &area, const std::vector<KeyValueEntry> &entries);

// ---- ResultItem: one line of an outcome list (docs/components/ResultItem.md)
struct ResultItemSpec {
    Text label;
    Tone tone = Tone::Neutral;
    Text status;                 // always present: the tone only colors it
    Text count;                  // optional, right of the status
    Text details;                // second line, Error and Warning only
};
int resultItemHeight(const Theme &t, const ResultItemSpec &spec);
void drawResultItem(Painter &p, const Theme &t, const Rect &r, const ResultItemSpec &spec);

// ---- ProgressPanel: title, bar, one status line, optional detail (docs/components/ProgressPanel.md)
struct ProgressPanelSpec {
    Text title;
    Text status;
    Text detail;                 // optional
    float fraction = 0.0f;
    bool fill = false;           // across the whole area, from its top (a page's content), instead of centered 700 wide
    bool showTitle = true;       // false: no title line (a dialog or wizard page already names itself in its title row)
};
void drawProgressPanel(Painter &p, const Theme &t, const Rect &area, const ProgressPanelSpec &spec);

}  // namespace ui
