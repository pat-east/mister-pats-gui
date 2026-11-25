#include "ui-framework/kit/Data.h"

#include <algorithm>

#include "ui-framework/kit/List.h"

namespace ui {

int tableRowHeight(const Theme &t) { return t.px(44); }

void drawTable(Painter &p, const Theme &t, const Rect &area, const std::vector<TableColumn> &columns,
               const std::vector<TableRow> &rows, int cursor, int first) {
    const int headerH = t.px(36), rowH = tableRowHeight(t), pad = t.px(10);
    int totalWeight = 0;
    for (const TableColumn &c : columns) totalWeight += c.weight;
    std::vector<int> xs, ws;
    int x = area.x;
    for (size_t i = 0; i < columns.size(); ++i) {
        const int w = i + 1 == columns.size() ? area.right() - x : area.w * columns[i].weight / std::max(1, totalWeight);
        xs.push_back(x);
        ws.push_back(w);
        x += w;
    }

    p.fillRoundedRect({area.x, area.y, area.w, headerH}, t.px(8), t.palette.surfaceHi);
    for (size_t i = 0; i < columns.size(); ++i) {
        drawLabel(p, t, {xs[i] + pad, area.y, ws[i] - 2 * pad, headerH}, columns[i].header.str(), TextRole::Small,
                  TextTier::Muted, columns[i].kind == CellKind::Flag ? Align::Center : Align::Left, true);
    }

    const int visible = std::max(0, (area.h - headerH - t.px(6)) / rowH);
    int y = area.y + headerH + t.px(6);
    for (int r = first; r < int(rows.size()) && r < first + visible; ++r, y += rowH) {
        const Rect row{area.x, y, area.w, rowH};
        const bool focused = r == cursor;
        if (focused) {
            p.fillRoundedRect(row, t.px(Radius::row), t.palette.surfaceHi);
            drawFocusMarker(p, t, row);
            drawFocusFrame(p, t, row, t.px(Radius::row));
        } else if ((r - first) % 2 == 1) {
            p.fillRoundedRect(row, t.px(Radius::row), t.palette.surface);
        }
        for (size_t c = 0; c < columns.size() && c < rows[size_t(r)].size(); ++c) {
            const TableCell &cell = rows[size_t(r)][c];
            const Rect box{xs[c] + pad, y, ws[c] - 2 * pad, rowH};
            if (columns[c].kind == CellKind::Flag) {
                const int icon = t.px(20);
                drawIcon(p, cell.flag ? IconId::Check : IconId::Cross, {box.x + (box.w - icon) / 2, y + (rowH - icon) / 2, icon, icon},
                         cell.flag ? t.tone(Tone::Done) : t.tone(Tone::Warning), std::max(2, t.px(2)));
            } else {
                drawLabel(p, t, {box.x, box.y, box.w - t.px(6), box.h}, cell.text, TextRole::Body, TextTier::Primary);
            }
        }
    }
}

int drawKeyValueList(Painter &p, const Theme &t, const Rect &areaIn, const std::vector<KeyValueEntry> &entries) {
    KeyValueView view;
    return drawKeyValueList(p, t, areaIn, entries, view);
}

int drawKeyValueList(Painter &p, const Theme &t, const Rect &areaIn, const std::vector<KeyValueEntry> &entries, KeyValueView &view) {
    const int rowH = std::max(t.px(40), p.lineHeight(FontRole::Regular, t.size(TextRole::Body)) + t.px(12));
    const int spacer = t.px(6), gap = t.px(16);
    const int count = int(entries.size());
    auto heightOf = [&](const KeyValueEntry &e) { return e.kind == KeyValueKind::Spacer ? spacer : rowH; };
    int sum = 0;
    for (const KeyValueEntry &e : entries) sum += heightOf(e);
    Rect area = areaIn;
    view.total = count;
    view.scrolls = sum > area.h;
    if (view.scrolls) area.w = std::max(0, area.w - scrollbarGutter(t));
    int labelW = 0;
    for (const KeyValueEntry &e : entries)
        if (e.kind == KeyValueKind::Row) labelW = std::max(labelW, labelWidth(p, t, e.label.str(), TextRole::Body));
    bool anyStatus = false;   // plain values start where a status word starts (after its icon), so the words line up
    for (const KeyValueEntry &e : entries) anyStatus = anyStatus || (e.kind == KeyValueKind::Row && e.hasStatus);
    labelW = std::max(labelW, areaIn.w * view.minLabelPercent / 100);
    labelW = std::min(labelW, std::max(area.w * 38 / 100, areaIn.w * view.minLabelPercent / 100));
    // The last first-entry that still fills the area to the end.
    int maxFirst = 0;
    if (view.scrolls) {
        int fit = 0;
        maxFirst = count;
        while (maxFirst > 0 && fit + heightOf(entries[size_t(maxFirst - 1)]) <= area.h) fit += heightOf(entries[size_t(--maxFirst)]);
    }
    view.first = std::max(0, std::min(view.first, maxFirst));
    int y = area.y, shown = 0;
    for (int i = view.first; i < count; ++i) {
        const KeyValueEntry &e = entries[size_t(i)];
        if (y + heightOf(e) > area.bottom()) break;
        ++shown;
        switch (e.kind) {
            case KeyValueKind::Spacer: y += spacer; break;
            case KeyValueKind::Heading:
                drawLabel(p, t, {area.x, y, area.w, rowH}, e.label.str(), TextRole::Body, TextTier::Primary, Align::Left, true);
                p.fillRect({area.x, y + rowH - 2, area.w, std::max(1, t.px(1))}, t.palette.surfaceHi);
                y += rowH;
                break;
            case KeyValueKind::Row: {
                drawLabel(p, t, {area.x, y, labelW, rowH}, e.label.str(), TextRole::Body, TextTier::Muted);
                const int vx = area.x + labelW + gap;
                if (e.hasStatus) {
                    const int line = p.lineHeight(FontRole::Bold, t.size(TextRole::Body));
                    drawStatus(p, t, vx, y + (rowH - line) / 2, e.tone, e.value.str(), TextRole::Body);
                } else {
                    const int indent = anyStatus ? t.px(30) : 0;   // icon 20 + gap 10 (drawStatus)
                    drawLabel(p, t, {vx + indent, y, area.right() - vx - indent, rowH}, e.value.str(), TextRole::Body, TextTier::Primary);
                }
                y += rowH;
                break;
            }
        }
    }
    view.visible = shown;
    if (view.scrolls) drawScrollbar(p, t, areaIn, count, shown, view.first);
    return y - area.y;
}

int resultItemHeight(const Theme &t, const ResultItemSpec &spec) {
    const bool second = !spec.details.empty() && (spec.tone == Tone::Error || spec.tone == Tone::Warning);
    return t.px(second ? 62 : 46);
}

void drawResultItem(Painter &p, const Theme &t, const Rect &r, const ResultItemSpec &spec) {
    const bool second = !spec.details.empty() && (spec.tone == Tone::Error || spec.tone == Tone::Warning);
    const int pad = t.px(12);
    const int bodyLine = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    const int smallLine = p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
    const int block = bodyLine + (second ? smallLine : 0);
    const int y = r.y + (r.h - block) / 2;
    int right = r.right() - pad;
    if (!spec.count.empty()) {
        const int w = labelWidth(p, t, spec.count.str(), TextRole::Body);
        drawLabel(p, t, {right - w, y, w, bodyLine}, spec.count.str(), TextRole::Body, TextTier::Muted, Align::Right);
        right -= w + t.px(20);
    }
    const int sw = statusWidth(p, t, spec.status.str(), TextRole::Body);
    drawStatus(p, t, right - sw, y, spec.tone, spec.status.str(), TextRole::Body);
    drawLabel(p, t, {r.x + pad, y, right - sw - t.px(20) - (r.x + pad), bodyLine}, spec.label.str(), TextRole::Body,
              TextTier::Primary);
    if (second)
        drawLabel(p, t, {r.x + pad, y + bodyLine, r.w - 2 * pad, smallLine}, spec.details.str(), TextRole::Small, TextTier::Faint);
}

void drawProgressPanel(Painter &p, const Theme &t, const Rect &area, const ProgressPanelSpec &spec) {
    const int width = spec.fill ? area.w : std::min(area.w, t.px(700));
    const int x = area.x + (area.w - width) / 2;
    const int titleH = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading));
    const int bodyH = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    const int smallH = p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
    const int bar = t.px(12);
    const int titleBlock = spec.showTitle ? titleH + t.px(20) : 0;
    int total = titleBlock + bar + t.px(24) + bodyH + (spec.detail.empty() ? 0 : t.px(8) + smallH);
    int y = area.y + (spec.fill ? 0 : std::max(0, (area.h - total) / 2));
    if (spec.showTitle) {
        drawLabel(p, t, {x, y, width, titleH}, spec.title.str(), TextRole::Heading, TextTier::Primary, Align::Left, true);
        y += titleBlock;
    }
    drawProgressBar(p, t, {x, y, width, bar}, spec.fraction, false);
    y += bar + t.px(24);
    drawLabel(p, t, {x, y, width, bodyH}, spec.status.str(), TextRole::Body, TextTier::Muted);
    if (!spec.detail.empty())
        drawLabel(p, t, {x, y + bodyH + t.px(8), width, smallH}, spec.detail.str(), TextRole::Small, TextTier::Faint);
}

}  // namespace ui
