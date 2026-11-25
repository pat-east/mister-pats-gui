#pragma once

#include "ui-framework/kit/Components.h"

namespace ui {

// ---- DetailPanel: the detail of the focused entry (docs/components/DetailPanel.md): a picture at its
//      native size (never scaled, no shadow), the title and a muted second line, and an optional status.
struct DetailSpec {
    ImagePtr art;               // the prepared picture; drawn centered at its own size
    bool artSquare = false;     // drawn with square corners: for a moving picture (video), so that repainting
                                // it alone over its last frame gives exactly what a full draw gives
    Text title, subtitle;
    Text status;                // optional status word under the caption
    Tone statusTone = Tone::Neutral;
};

void drawDetailPanel(Painter &p, const Theme &t, const Rect &area, const DetailSpec &spec);
// The picture slot drawDetailPanel() uses inside `area` (the picture is centered in it at its own size).
// Lets a caller that repaints the picture alone know where it sits.
Rect detailPanelSlot(Painter &p, const Theme &t, const Rect &area, bool hasStatus);

// ---- Grid-List, the third grid view (after Grid and Grid-Small): a list of entries on the left and the
//      detail of the focused entry on the right, a gap between them.
struct GridListRects { Rect list, detail; };
GridListRects splitGridList(const Theme &t, const Rect &area);

}  // namespace ui
