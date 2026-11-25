#include <algorithm>

#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Primitives.h"

namespace ui {

void drawCrashScreen(Painter &p, const Theme &t, const std::string &title, const std::string &what,
                     const std::string &changed, const std::string &details, const std::string &nextLine) {
    const int W = t.screenWidth(), H = t.screenHeight();
    drawPage(p, t, {0, 0, W, H});
    const int pad = t.px(28);
    const int cardW = std::min(W - 2 * t.marginX(), t.px(1000));
    const int iconBox = t.px(72);
    const int textX = pad + iconBox + t.px(24);
    const int textW = cardW - textX - pad;
    const int headH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    const int bodyLine = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    const int smallLine = p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
    const int whatH = int(wrapText(p, t, what, TextRole::Body, false, textW).size()) * bodyLine;
    const int changedH = int(wrapText(p, t, changed, TextRole::Body, false, textW).size()) * bodyLine;
    const int gap = t.px(12);
    const int cardH = t.px(6) + pad + headH + gap + whatH + changedH + t.px(20) + smallLine + t.px(24) + bodyLine + pad + t.px(10);
    const Rect card{(W - cardW) / 2, (H - cardH) / 2, cardW, cardH};

    drawSurface(p, t, card, SurfaceRole::Raised, t.px(Radius::panel));
    p.fillRect({card.x + t.px(Radius::panel), card.y, card.w - 2 * t.px(Radius::panel), t.px(6)}, t.palette.error);

    int y = card.y + t.px(6) + pad;
    const Rect icon{card.x + pad, y, iconBox, iconBox};
    drawIcon(p, IconId::Alert, icon.inset(t.px(8)), t.palette.error, std::max(2, t.px(4)));
    drawLabel(p, t, {card.x + textX, y, textW, headH}, title, TextRole::Heading, TextTier::Primary, Align::Left, true);
    y += headH + gap;
    y += drawTextBlock(p, t, {card.x + textX, y, textW, whatH}, what, TextRole::Body, TextTier::Primary);
    y += drawTextBlock(p, t, {card.x + textX, y, textW, changedH}, changed, TextRole::Body, TextTier::Muted);
    y += t.px(20);
    drawLabel(p, t, {card.x + pad, y, card.w - 2 * pad, smallLine}, details, TextRole::Small, TextTier::Muted);
    y += smallLine + t.px(24);
    drawLabel(p, t, {card.x + pad, y, card.w - 2 * pad, bodyLine}, nextLine, TextRole::Body, TextTier::Primary,
              Align::Left, true);
}

}  // namespace ui
