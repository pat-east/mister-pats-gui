#include "app/ListDetail.h"

#include <algorithm>
#include <cstdlib>

#include "ui-framework/kit/DetailPanel.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

std::string ListDetail::factsLine(const GameInfo &g, const GameDetail &d) {
    std::string players;
    if (!d.players.empty()) {   // "1" is one player, "1-2" and "2-4" are players
        const bool number = d.players.find_first_not_of("0123456789") == std::string::npos;
        players = ui::plural("detail.players", number ? std::atol(d.players.c_str()) : 2, {d.players});
    }
    return ui::joinParts({g.systemName, d.year, players});
}

int ListDetail::move(int cursor, int count, int dx, int dy) const {
    if (dy != 0) cursor = ui::ListLayout::move(cursor, count, dy);
    if (dx != 0) cursor = ui::ListLayout::move(cursor, count, dx * list_.pageStep());
    return cursor;
}

void ListDetail::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, int count, int cursor, bool paneActive,
                      const Options &o, const RowFn &row, const PrepareFn &prepare) {
    const ui::GridListRects split = ui::splitGridList(t, area);
    const ui::Density density = ui::Density::Dense;
    const ui::Rect rows{split.list.x, split.list.y, split.list.w - ui::scrollbarGutter(t), split.list.h};
    list_.configure(rows, ui::listItemFrameHeight(t, false, density), ui::listItemPitch(t, false, density));
    if (pendingTop_) {   // after a letter jump: the row of the letter goes to the top
        pendingTop_ = false;
        if (cursor > list_.scroll()) list_.ensureVisible(std::min(count - 1, cursor + list_.visibleRows() - 1));
    }
    list_.ensureVisible(cursor);

    const int first = list_.scroll();
    const int last = std::min(count, first + list_.visibleRows());
    if (prepare) prepare(first, last);
    p.pushClip(split.list.inset(-t.px(4)));
    for (int i = first; i < last; ++i) {
        GameInfo g;
        if (!row(i, g)) continue;
        ui::ListItemSpec entry;
        entry.kind = ui::ListItemKind::Entry;
        entry.title = ui::Text::data(g.title);
        entry.favorite = g.favorite && o.favoriteMark;
        ui::ListItemState st;
        st.cursor = i == cursor;
        st.paneActive = paneActive;
        ui::drawListItem(p, t, list_.row(i), entry, st);
    }
    ui::drawScrollbar(p, t, split.list, count, list_.visibleRows(), list_.scroll());
    p.popClip();

    // The detail of the game under the cursor (looked up once per game, not per frame).
    GameInfo g;
    if (cursor < 0 || cursor >= count || !row(cursor, g)) return;
    if (!detailValid_ || detailRef_.system != g.ref.system || detailRef_.id != g.ref.id) {
        detailRef_ = g.ref;
        detail_ = library_.detail(g.ref);
        detailValid_ = true;
    }
    const std::string status = slot_.status();
    // The picture slot is the one the DetailPanel draws into; a video is never scaled, so the library is told how
    // big it may be. The cover is requested at the slot's own size, so it fills the slot (as Classic's detail panel).
    const ui::Rect slot = ui::detailPanelSlot(p, t, split.detail, !status.empty());
    slot_.setSlotSize(slot.w, slot.h);
    const PreviewSlot::Picture picture = slot_.pictureFit(g.ref, slot.w, slot.h);
    ui::DetailSpec ds;
    ds.art = picture.image;   // the video frame while a video plays
    ds.artSquare = picture.video;
    if (!status.empty()) ds.status = ui::Text::data(status);
    ds.title = ui::Text::data(detail_.info.title.empty() ? g.title : detail_.info.title);
    ds.subtitle = ui::Text::data(factsLine(g, detail_));
    ui::drawDetailPanel(p, t, split.detail, ds);
    slot_.noteSlot(slot);
}

}  // namespace patsgui
