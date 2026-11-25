#include "ui-framework/kit/TabRow.h"

#include "ui-framework/kit/Primitives.h"

namespace ui {

int tabRowHeight(Painter &p, const Theme &t) {
    return p.lineHeight(FontRole::Bold, t.size(TextRole::Body)) + t.px(2) + t.px(3);
}

int drawTabRow(Painter &p, const Theme &t, const Rect &area, const TabRowSpec &spec) {
    const int line = p.lineHeight(FontRole::Bold, t.size(TextRole::Body));
    int x = area.x + t.px(4);
    for (size_t i = 0; i < spec.labels.size(); ++i) {
        const std::string word = spec.labels[i].str();   // a copy: Text::str() may refer to a temporary
        const bool active = int(i) == spec.active;
        const int w = labelWidth(p, t, word, TextRole::Body, active);
        drawLabel(p, t, {x, area.y, w, line}, word, TextRole::Body, active ? TextTier::Primary : TextTier::Muted,
                  Align::Left, active);
        if (active) p.fillRoundedRect({x, area.y + line + t.px(2), w, t.px(3)}, t.px(2), t.palette.focus);
        x += w + t.px(36);
    }
    return tabRowHeight(p, t);
}

}  // namespace ui
