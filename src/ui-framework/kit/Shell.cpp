#include <algorithm>

#include "ui-framework/kit/Components.h"

namespace ui {

namespace {

// Tones on the top bar: the bar stays dark in light mode, so the surface tones (made for light panels) would
// not be readable there; it uses its own light tones instead.
Color barTone(const Theme &t, Tone tone) {
    if (!t.palette.topBarSolid) return t.tone(tone);
    switch (tone) {
        case Tone::Warning: return Color::rgb(0xFFC857);
        case Tone::Error:   return Color::rgb(0xFF8F85);
        default:            return t.palette.topBarFocus;
    }
}

// The column widths of an item's rows: label, value, note.
struct RowColumns { int label = 0, value = 0, note = 0; int gap = 0; int total() const { return label + gap + value + (note ? gap / 2 + note : 0); } };
RowColumns rowColumns(Painter &p, const Theme &t, const NavItemSpec &item) {
    RowColumns c;
    c.gap = t.px(10);
    for (const NavRow &r : item.rows) {
        c.label = std::max(c.label, labelWidth(p, t, r.label.str(), TextRole::Small));
        c.value = std::max(c.value, labelWidth(p, t, r.value.str(), TextRole::Small));
        c.note = std::max(c.note, labelWidth(p, t, r.note.str(), TextRole::Small));
    }
    return c;
}

int navItemWidth(Painter &p, const Theme &t, const NavItemSpec &item) {
    const TextRole role = item.isClock ? TextRole::Clock : TextRole::Body;
    int w = labelWidth(p, t, item.primary.str(), role, item.isClock);
    if (!item.rows.empty()) return std::max(w, rowColumns(p, t, item).total());
    if (!item.secondary.empty()) w = std::max(w, labelWidth(p, t, item.secondary.str(), TextRole::Small));
    if (item.icon != IconId::None) w += t.px(20) + t.px(10);
    return w;
}

// Draws one nav item with its top at `y`; `x` is the left edge of the item.
void drawNavItem(Painter &p, const Theme &t, int x, int y, int width, const NavItemSpec &item, Align align) {
    const TextRole role = item.isClock ? TextRole::Clock : TextRole::Body;
    const bool bold = item.isClock;
    const Color primaryColor = item.tone == Tone::Neutral ? t.palette.topBarText : barTone(t, item.tone);
    int textX = x, textW = width;
    if (item.icon != IconId::None) {
        const int icon = t.px(20);
        const int line = p.lineHeight(bold ? FontRole::Bold : FontRole::Regular, t.size(role));
        drawIcon(p, item.icon, {align == Align::Right ? x + width - icon : x, y + (line - icon) / 2, icon, icon},
                 item.tone == Tone::Neutral ? t.palette.topBarMuted : barTone(t, item.tone), std::max(2, t.px(2)));
        const int used = icon + t.px(10);
        textW -= used;
        if (align != Align::Right) textX += used;
    }
    const int lineA = p.lineHeight(bold ? FontRole::Bold : FontRole::Regular, t.size(role));
    if (!item.rows.empty()) {
        // A list: the title and the columns share the item's left edge, so the rows line up under it.
        drawLabelColor(p, t, {x, y, width, lineA}, item.primary.str(), role, primaryColor, Align::Left, bold);
        const RowColumns c = rowColumns(p, t, item);
        const int lineH = p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
        const int firstY = y + lineA + t.px(6);
        const int limitY = t.topBarHeight() + t.marginY() - t.px(4);   // the empty margin under the bar
        size_t room = limitY > firstY ? size_t((limitY - firstY) / std::max(1, lineH)) : 0;
        size_t count = std::min(room, item.rows.size());
        const bool cut = count < item.rows.size();
        if (cut && count > 0) --count;   // the last line says that more exist
        const int valueX = x + c.label + c.gap, noteX = valueX + c.value + c.gap / 2;
        int rowY = firstY;
        for (size_t i = 0; i < count; ++i) {
            const NavRow &r = item.rows[i];
            const Color color = r.tone == Tone::Neutral ? t.palette.topBarMuted : barTone(t, r.tone);
            drawLabelColor(p, t, {x, rowY, c.label, lineH}, r.label.str(), TextRole::Small, color, Align::Left);
            drawLabelColor(p, t, {valueX, rowY, c.value, lineH}, r.value.str(), TextRole::Small, color, Align::Right);
            if (c.note) drawLabelColor(p, t, {noteX, rowY, c.note, lineH}, r.note.str(), TextRole::Small, color, Align::Left);
            rowY += lineH;
        }
        if (cut) drawLabelColor(p, t, {x, rowY, c.label, lineH}, "...", TextRole::Small, t.palette.topBarMuted, Align::Left);
        return;
    }
    drawLabelColor(p, t, {textX, y, textW, lineA}, item.primary.str(), role, primaryColor, align, bold);
    if (!item.secondary.empty()) {
        const int lineB = p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
        drawLabelColor(p, t, {textX, y + lineA + t.px(6), textW, lineB}, item.secondary.str(), TextRole::Small,
                       t.palette.topBarMuted, align);
    }
}

}  // namespace

void drawShellBottomBar(Painter &p, const Theme &t, const ShellSpec &spec) {
    const Rect all = p.bounds();
    const int W = all.w, H = all.h;
    const int marginX = t.marginX();
    // bottom bar: footer (static) on the right, help (always) on the left
    const int barTop = H - t.bottomBarHeight();
    int footerX = W - marginX;
    int footerStart = footerX;
    std::vector<FooterTextSpec> footer = spec.footer;
    for (size_t i = 0; i < footer.size(); ++i) {
        const FooterTextSpec &f = footer[footer.size() - 1 - i];
        const int w = labelWidth(p, t, f.text.str(), TextRole::Small);
        const Color c = f.tone == Tone::Neutral ? t.text(TextTier::Muted) : t.tone(f.tone);
        drawLabelColor(p, t, {footerX - w, barTop, w, t.bottomBarHeight()}, f.text.str(), TextRole::Small, c, Align::Right);
        footerX -= w + t.px(Space::gap);
        footerStart = footerX + t.px(Space::gap);
    }
    const int helpW = std::max(0, footerStart - t.px(Space::gap) - marginX);
    if (spec.hold.active)
        drawHoldProgress(p, t, {marginX, barTop, helpW, t.bottomBarHeight()}, spec.hold.hint, spec.hold.progress);
    else
        drawHints(p, t, {marginX, barTop, helpW, t.bottomBarHeight()}, spec.help);
}

Rect drawShell(Painter &p, const Theme &t, const ShellSpec &spec) {
    const Rect all = p.bounds();
    const int W = all.w, H = all.h;
    const int marginX = t.marginX();

    drawPage(p, t, all);
    // The top bar fades into the page below its items.
    if (t.palette.topBarSolid) {
        // Light mode: solid up to the bar (nav items and tabs are light text on it), then a short fade.
        p.fillRect({0, 0, W, t.topBarHeight() - t.px(8)}, t.palette.topBarFill.withAlpha(238));
        p.fillGradient({0, t.topBarHeight() - t.px(8), W, t.px(8) + t.px(24)}, t.palette.topBarFill.withAlpha(238),
                       t.palette.topBarFill.withAlpha(0));
    } else {
        p.fillGradient({0, 0, W, t.topBarHeight()}, t.palette.topBarFill.withAlpha(210), t.palette.background.withAlpha(0));
    }

    // ---- nav items and tabs. Order of giving way when the bar is too narrow: nav items drop first
    //      (lowest priority first), then the tabs fall back to icons, then labels are elided.
    struct Placed { const NavItemSpec *item; int width; bool left; bool kept; };
    std::vector<Placed> placed;
    for (const NavItemSpec &item : spec.left) placed.push_back({&item, navItemWidth(p, t, item), true, true});
    for (const NavItemSpec &item : spec.right) placed.push_back({&item, navItemWidth(p, t, item), false, true});

    const int navGap = t.px(40);
    const int clearance = t.px(24);
    auto sideWidth = [&](bool left) {
        int total = 0, count = 0;
        for (const Placed &pl : placed)
            if (pl.kept && pl.left == left) { total += pl.width; ++count; }
        return count ? total + (count - 1) * navGap : 0;
    };

    const int tabSize = t.size(TextRole::Tab);
    const int n = int(spec.tabs.size());
    const int iconSize = t.px(26), iconGap = t.px(10);
    auto hasIcon = [&](int i) { return bool(spec.tabs[i].icon); };
    bool allIcons = n > 0;
    for (int i = 0; i < n; ++i) allIcons = allIcons && hasIcon(i);

    // The width of each tab for a display mode (the active tab is bold, hence wider).
    auto tabWidths = [&](TabDisplay mode) {
        std::vector<int> w;
        for (int i = 0; i < n; ++i) {
            const bool active = i == spec.activeTab;
            const int label = p.measure(active ? FontRole::Bold : FontRole::Regular, tabSize, spec.tabs[i].label.str());
            const bool icon = hasIcon(i) && mode != TabDisplay::Names;
            if (mode == TabDisplay::Icons && icon) w.push_back(iconSize);
            else w.push_back((icon ? iconSize + iconGap : 0) + label);
        }
        return w;
    };
    auto spacingFor = [&](TabDisplay mode) { return t.px(mode == TabDisplay::Icons ? 54 : 44); };
    const int minSpacing = t.px(16);
    auto minTotal = [&](const std::vector<int> &w) {
        int total = 0;
        for (size_t i = 0; i < w.size(); ++i) total += w[i] + (i ? minSpacing : 0);
        return total;
    };
    auto available = [&]() {
        const int l = sideWidth(true), r = sideWidth(false);
        const int from = l ? marginX + l + clearance : marginX;
        const int to = r ? W - marginX - r - clearance : W - marginX;
        return to - from;
    };

    TabDisplay mode = spec.tabDisplay;
    if (mode != TabDisplay::Names && !allIcons) mode = TabDisplay::Names;   // no pictures: names
    std::vector<int> widths = tabWidths(mode);
    while (n > 0 && minTotal(widths) > available()) {
        int victim = -1;
        for (size_t i = 0; i < placed.size(); ++i)
            if (placed[i].kept && (victim < 0 || placed[i].item->priority < placed[victim].item->priority)) victim = int(i);
        if (victim >= 0) { placed[victim].kept = false; continue; }
        if (mode != TabDisplay::Icons && allIcons) { mode = TabDisplay::Icons; widths = tabWidths(mode); continue; }
        break;
    }

    int leftX = marginX;
    int rightX = W - marginX;
    for (const Placed &pl : placed) {
        if (!pl.kept || !pl.left) continue;
        drawNavItem(p, t, leftX, t.px(16), pl.width, *pl.item, Align::Left);
        leftX += pl.width + navGap;
    }
    for (size_t i = placed.size(); i-- > 0;) {
        const Placed &pl = placed[i];
        if (!pl.kept || pl.left) continue;
        drawNavItem(p, t, rightX - pl.width, t.px(18), pl.width, *pl.item, Align::Right);
        rightX -= pl.width + navGap;
    }

    // ---- tabs: centered on the canvas when there is room, otherwise between the nav areas;
    //      spacing shrinks first, then labels are elided
    if (n > 0) {
        const int l = sideWidth(true), r = sideWidth(false);
        const int availLeft = l ? marginX + l + clearance : marginX;
        const int availRight = r ? W - marginX - r - clearance : W - marginX;
        const int room = availRight - availLeft;
        int widthsTotal = 0;
        for (int w : widths) widthsTotal += w;
        int spacing = spacingFor(mode);
        std::vector<int> drawW = widths;
        if (widthsTotal + spacing * (n - 1) > room) spacing = std::max(minSpacing, n > 1 ? (room - widthsTotal) / (n - 1) : 0);
        if (widthsTotal + spacing * (n - 1) > room) {
            const int share = std::max(t.px(40), (room - spacing * (n - 1)) / n);
            for (int i = 0; i < n; ++i) drawW[i] = std::min(widths[i], share);
        }
        int total = 0;
        for (int i = 0; i < n; ++i) total += drawW[i] + (i ? spacing : 0);
        int x = (W - total) / 2;
        if (x < availLeft || x + total > availRight) x = std::max(availLeft, (availLeft + availRight - total) / 2);
        const int lineH = p.lineHeight(FontRole::Bold, tabSize);
        const int y = (t.topBarHeight() - lineH) / 2 - t.px(4);
        for (int i = 0; i < n; ++i) {
            const bool active = i == spec.activeTab;
            const FontRole role = active ? FontRole::Bold : FontRole::Regular;
            Color c = active ? t.palette.topBarText : t.palette.topBarMuted;
            if (spec.tabsLocked) c = c.withAlpha(140);
            const bool icon = hasIcon(i) && mode != TabDisplay::Names;
            int textX = x;
            if (icon) {
                spec.tabs[i].icon->draw(p, x, y + (lineH - spec.tabs[i].icon->height()) / 2, c);
                textX += iconSize + iconGap;
            }
            if (mode != TabDisplay::Icons || !icon)
                p.drawText(role, tabSize, textX, y,
                           p.elide(role, tabSize, spec.tabs[i].label.str(), std::max(0, drawW[i] - (textX - x))), c);
            if (active) {
                const int uh = std::max(2, t.px(3));
                p.fillRoundedRect({x, y + lineH + t.px(4), drawW[i], uh}, uh / 2,
                                  spec.tabsLocked ? t.palette.topBarFocus.withAlpha(140) : t.palette.topBarFocus);
            }
            x += drawW[i] + spacing;
        }
    }

    drawShellBottomBar(p, t, spec);

    const int barTop = H - t.bottomBarHeight();
    // ---- the content window
    const int top = t.topBarHeight() + t.marginY();
    return {marginX, top, W - 2 * marginX, barTop - top - t.marginY()};
}

void drawSplash(Painter &p, const Theme &t, const Image *logo, float fraction, const std::string &status) {
    const Rect all = p.bounds();
    p.clear(t.palette.background);
    const int logoSize = t.px(128);
    const Rect logoArea{(all.w - logoSize) / 2, (all.h - logoSize) / 2 - t.px(30), logoSize, logoSize};
    if (logo && logo->valid()) p.drawImage(*logo, logoArea, 255, t.px(16));
    const int barW = t.px(280);
    const Rect bar{all.w / 2 - barW / 2, logoArea.bottom() + t.px(40), barW, t.px(12)};
    drawProgressBar(p, t, bar, fraction, false);
    if (!status.empty()) {
        const int line = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
        drawLabel(p, t, {all.w / 2 - t.px(400), bar.bottom() + t.px(16), t.px(800), line}, status, TextRole::Body,
                  TextTier::Muted, Align::Center);
    }
}

}  // namespace ui
