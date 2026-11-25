#include "ui-framework/kit/Selector.h"

#include <algorithm>

#include "ui-framework/kit/Icon.h"
#include "ui-framework/kit/Primitives.h"

namespace ui {

int selectorHeight(const Theme &t) { return t.px(56); }
int selectorWidth(const Theme &t) { return t.px(520); }

void drawSelector(Painter &p, const Theme &t, const Rect &r, const SelectorSpec &spec) {
    const int radius = t.px(Radius::panel);
    p.fillRoundedRect(r, radius, t.palette.surfaceHi);
    if (t.borders()) p.strokeRoundedRect(r, radius, std::max(1, t.px(2)), Color::rgb(0xFFFFFF));
    if (spec.focused) drawFocusFrame(p, t, r, radius);

    const int icon = t.px(22);
    const int pad = t.px(16);
    const Color on = spec.focused ? t.palette.textPrimary : t.text(TextTier::Muted);
    const Color off = t.text(TextTier::Disabled);
    BootstrapIcon left("chevron-left", icon, icon);
    BootstrapIcon right("chevron-right", icon, icon);
    left.drawIn(p, {r.x + pad, r.y, icon, r.h}, spec.canStepLeft ? on : off);
    right.drawIn(p, {r.right() - pad - icon, r.y, icon, r.h}, spec.canStepRight ? on : off);
    drawLabel(p, t, {r.x + pad + icon + pad, r.y, r.w - 2 * (pad + icon + pad), r.h}, spec.value.str(), TextRole::Body,
              spec.focused ? TextTier::Primary : TextTier::Muted, Align::Center, true);
}

}  // namespace ui
