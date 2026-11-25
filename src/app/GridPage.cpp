#include "app/GridPage.h"

#include <algorithm>

#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

const char *viewNameKey(ViewMode m) {
    return m == ViewMode::Grid ? "view.grid" : m == ViewMode::Small ? "view.small" : "view.list";
}

std::string gridInfo(ViewMode m, int position, int total, bool capped) {
    return ui::format("grid.info", {ui::Catalog::get().tr(viewNameKey(m)), ui::position(position, total, capped)});
}

std::string gridInfoSearching(ViewMode m, int found, bool capped) {
    return ui::format("grid.info.searching", {ui::Catalog::get().tr(viewNameKey(m)), std::to_string(found) + (capped ? "+" : "")});
}

namespace {

ui::GridSpec specFor(const GridPage::Options &o, bool small) {
    ui::GridSpec spec;
    const ArtBounds b = artBounds(small ? ArtSize::Small : o.art);
    spec.artW = b.width;
    spec.artH = b.height;
    spec.caption = small ? ui::Caption::None : (o.secondLine ? ui::Caption::TitleSub : ui::Caption::Title);
    return spec;
}

ui::GridItemSpec itemFor(const GridTile &tile, const GridPage::Options &o) {
    ui::GridItemSpec item;
    item.label = ui::Text::data(tile.label);
    if (o.secondLine) item.sublabel = ui::Text::data(tile.sublabel);
    item.favorite = tile.favorite && o.favoriteMark;
    if (tile.art.image && tile.art.image->valid()) {
        item.kind = ui::GridItemKind::Cover;
        item.art = tile.art.image;
    } else {
        item.kind = ui::GridItemKind::Placeholder;
    }
    return item;
}

}  // namespace

void GridPage::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &body, bool small, int count, int cursor,
                    bool focused, const Options &o, const TileFn &tile, const PrepareFn &prepare) {
    // The small grid has no captions: the focused item gets a line of its own below the grid.
    const int lineH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Body));
    const int footerH = small ? lineH + t.px(10) : 0;
    const ui::GridSpec spec = specFor(o, small);
    const ui::Rect area{body.x, body.y + t.px(6), body.w, body.h - footerH - t.px(6)};
    const ui::Rect clip{body.x - t.px(6), area.y - t.px(6), body.w + t.px(12), area.h + t.px(6)};
    grid_.configure(p, t, area, spec);

    const int cols = grid_.columns(), rows = grid_.visibleRows();
    if (pendingTop_) {
        pendingTop_ = false;
        if (cursor / cols > grid_.scrollRow()) grid_.ensureVisible(std::min(count - 1, cursor + cols * (rows - 1)));
    }
    grid_.ensureVisible(cursor);

    const int first = grid_.firstVisible();
    const int last = grid_.endWithPeek(count);   // one more row, cut off by the clip: the "peek"
    if (prepare) prepare(first, last);
    p.pushClip(clip);
    int focusedIndex = -1;
    GridTile focusedTile;
    for (int i = first; i < last; ++i) {
        GridTile tl;
        if (!tile(i, i == cursor, tl)) continue;
        if (focused && i == cursor) { focusedIndex = i; focusedTile = tl; continue; }   // drawn last: its outline on top
        ui::drawGridItem(p, t, grid_, grid_.cell(i), spec, itemFor(tl, o), false);
    }
    if (focusedIndex >= 0) ui::drawGridItem(p, t, grid_, grid_.cell(focusedIndex), spec, itemFor(focusedTile, o), true);
    p.popClip();

    if (small && cursor >= 0 && cursor < count) {
        GridTile tl;
        if (tile(cursor, true, tl)) {
            const std::string line = ui::joinParts({tl.label, o.secondLine ? tl.sublabel : std::string()});
            ui::drawLabel(p, t, {body.x, body.bottom() - footerH, body.w, footerH}, line, ui::TextRole::Body,
                          ui::TextTier::Primary, ui::Align::Left, true);
        }
    }
}

int GridPage::drawStrip(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, int count, int cursor, bool focused,
                        const Options &o, const TileFn &tile) {
    const ui::GridSpec spec = specFor(o, false);
    const ui::Rect strip{area.x, area.y + t.px(6), area.w, area.h - t.px(6)};
    grid_.configure(p, t, strip, spec);   // the same geometry as the grid: one row of its cells
    const int cols = std::max(1, grid_.columns());
    const int step = grid_.cellWidth() + t.gap();
    if (cursor < stripScroll_) stripScroll_ = cursor;
    if (cursor >= stripScroll_ + cols) stripScroll_ = cursor - cols + 1;
    stripScroll_ = std::max(0, std::min(stripScroll_, std::max(0, count - cols)));

    const int h = grid_.cellHeight();
    p.pushClip(ui::Rect{strip.x - t.px(6), strip.y - t.px(6), strip.w + t.px(12), h + t.px(12)});
    const int last = std::min(count, stripScroll_ + cols + 1);   // one more shows as the peek
    for (int i = stripScroll_; i < last; ++i) {
        GridTile tl;
        if (!tile(i, i == cursor, tl)) continue;
        const ui::Rect cell{strip.x + (i - stripScroll_) * step, strip.y, grid_.cellWidth(), h};
        ui::drawGridItem(p, t, grid_, cell, spec, itemFor(tl, o), focused && i == cursor);
    }
    p.popClip();
    return t.px(6) + h;
}

}  // namespace patsgui
