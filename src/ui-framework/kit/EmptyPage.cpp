#include "ui-framework/kit/EmptyPage.h"

#include <algorithm>
#include <vector>

namespace ui {

namespace {

// The block alone (no panel), centered in `area`: icon, title, why, next, Details, extra.
void drawBlock(Painter &p, const Theme &t, const Rect &area, const EmptyPageSpec &s) {
    const int w = std::min(area.w, t.px(900));
    const int x = area.x + (area.w - w) / 2;
    const int titleH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    const int step = p.lineHeight(FontRole::Regular, t.size(TextRole::Body)) + t.px(Space::lineExtra);
    const int smallStep = p.lineHeight(FontRole::Regular, t.size(TextRole::Small)) + t.px(Space::lineExtra);
    const int iconBox = t.px(56);
    const bool hasIcon = s.icon != IconId::None;
    const std::vector<std::string> titleLines = wrapText(p, t, s.title.str(), TextRole::Heading, true, w);
    const std::vector<std::string> whyLines = wrapText(p, t, s.why.str(), TextRole::Body, false, w);
    const std::vector<std::string> nextLines = s.next.empty() ? std::vector<std::string>()
                                                              : wrapText(p, t, s.next.str(), TextRole::Body, false, w);
    const std::string detailsLine = s.details.empty() ? std::string() : Text::key("modal.details").str() + ": " + s.details;
    const int titleStep = titleH + t.px(Space::lineExtra);
    const int blockH = (hasIcon ? iconBox + t.px(Space::titleGap) : 0) + int(titleLines.size()) * titleStep +
                       t.px(Space::titleGap) + int(whyLines.size()) * step +
                       (nextLines.empty() ? 0 : t.px(14) + int(nextLines.size()) * step) +
                       (detailsLine.empty() ? 0 : t.px(Space::gap) + smallStep) +
                       (s.extra.empty() ? 0 : t.px(Space::gap) + smallStep);
    int y = area.y + std::max(0, (area.h - blockH) / 2);
    if (hasIcon) {
        drawIcon(p, s.icon, {x + (w - iconBox) / 2, y, iconBox, iconBox}, t.tone(s.tone), std::max(3, t.px(4)));
        y += iconBox + t.px(Space::titleGap);
    }
    for (const std::string &line : titleLines) {
        drawLabel(p, t, {x, y, w, titleH}, line, TextRole::Heading, TextTier::Primary, Align::Center, true);
        y += titleStep;
    }
    y += t.px(Space::titleGap);
    for (const std::string &line : whyLines) {
        drawLabel(p, t, {x, y, w, step}, line, TextRole::Body, TextTier::Muted, Align::Center);
        y += step;
    }
    if (!nextLines.empty()) {
        y += t.px(14);
        for (const std::string &line : nextLines) {
            drawLabel(p, t, {x, y, w, step}, line, TextRole::Body, TextTier::Primary, Align::Center);
            y += step;
        }
    }
    if (!detailsLine.empty()) {
        y += t.px(Space::gap);
        drawLabel(p, t, {x, y, w, smallStep}, detailsLine, TextRole::Small, TextTier::Faint, Align::Center);
        y += smallStep;
    }
    if (!s.extra.empty()) {
        y += t.px(Space::gap);
        drawLabel(p, t, {x, y, w, smallStep}, s.extra, TextRole::Small, TextTier::Faint, Align::Center);
    }
}

}  // namespace

void drawEmptyPage(Painter &p, const Theme &t, const Rect &area, const EmptyPageSpec &spec) {
    drawSurface(p, t, area, SurfaceRole::Faint, t.px(Radius::panel));
    drawBlock(p, t, area.inset(t.px(Space::pad)), spec);
}

void drawEmptyState(Painter &p, const Theme &t, const Rect &area, const std::string &title, const std::string &why,
                    const std::string &next) {
    EmptyPageSpec spec;
    spec.title = Text::data(title);
    spec.why = Text::data(why);
    spec.next = Text::data(next);
    drawBlock(p, t, area, spec);
}

}  // namespace ui
