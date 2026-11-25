#include "app/SystemsTab.h"

#include <algorithm>
#include <cstdio>

#include "app/EmptyPages.h"
#include "app/GridPage.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

namespace {

using namespace ui;

void strings(Catalog &c) {
    c.set("systems.noCore", "No core");
    c.set("systems.manage.title", "Show or hide systems");
    c.set("systems.hint.toggle", "Show or hide");
    c.set("systems.empty.title", "No systems yet");
    c.set("systems.empty.why", "No games were found, so there is nothing to show here.");
    c.set("systems.empty.next", "Build the game list in Settings, then come back.");
    c.set("systems.allHidden.title", "All systems are hidden");
    c.set("systems.allHidden.why", "You have hidden every system, so the grid is empty.");
    c.set("systems.allHidden.next", "Choose \"Show or hide systems\" in the help below to bring some back.");
    c.set("systems.manage.count", "{1} of {2} shown");
    c.set("systems.manage.explain", "Hidden systems stay in your library. They only disappear from the Systems tab.");
}

}  // namespace

PATSGUI_REGISTER_TAB(Tab::Systems, SystemsTab, strings);

SystemsTab::SystemsTab(Services &services) : library_(services.library) { reload(); }

SystemId SystemsTab::takeOpenRequest() {
    SystemId out;
    out.swap(request_);
    return out;
}

int SystemsTab::visibleIndex(const SystemId &id) const {
    for (size_t i = 0; i < shown_.size(); ++i)
        if (shown_[i].id == id) return int(i);
    return -1;
}

void SystemsTab::focusSystem(const SystemId &system) {
    const int i = visibleIndex(system);
    if (i >= 0) cursor_ = i;
}

void SystemsTab::reload() {
    const SystemId keep = (cursor_ >= 0 && cursor_ < int(shown_.size())) ? shown_[size_t(cursor_)].id : SystemId();
    const SystemId keepManage =
        (manageCursor_ >= 0 && manageCursor_ < int(all_.size())) ? all_[size_t(manageCursor_)].id : SystemId();
    all_.clear();
    shown_.clear();
    showArcade_ = library_.showArcadeInSystems();
    std::vector<SystemInfo> list = library_.systems();
    // Classic order: the catalog's (grouped) order; the library's name order breaks ties.
    std::stable_sort(list.begin(), list.end(),
                     [](const SystemInfo &a, const SystemInfo &b) { return a.order < b.order; });
    for (const SystemInfo &s : list) {
        all_.push_back(s);   // the visibility list names every system with games, as Classic does
        if (!s.hidden && (!s.isArcade || showArcade_)) shown_.push_back(s);
    }
    revision_ = library_.revision();
    const int at = visibleIndex(keep);
    if (at >= 0) cursor_ = at;
    cursor_ = std::max(0, std::min(cursor_, int(shown_.size()) - 1));
    for (size_t i = 0; i < all_.size(); ++i)
        if (all_[i].id == keepManage) manageCursor_ = int(i);
    manageCursor_ = std::max(0, std::min(manageCursor_, int(all_.size()) - 1));
}

bool SystemsTab::update(float) {
    if (library_.revision() == revision_ && library_.showArcadeInSystems() == showArcade_) return false;
    reload();
    return true;
}

void SystemsTab::onShow() {
    managing_ = false;
    library_.refresh();
    reload();
}

bool SystemsTab::handle(Event event) {
    if (managing_) {
        const int count = int(all_.size());
        switch (event) {
            case Event::Up:    manageCursor_ = ListLayout::move(manageCursor_, count, -1); return true;
            case Event::Down:  manageCursor_ = ListLayout::move(manageCursor_, count, 1); return true;
            case Event::Left:  manageCursor_ = ListLayout::move(manageCursor_, count, -list_.pageStep()); return true;
            case Event::Right: manageCursor_ = ListLayout::move(manageCursor_, count, list_.pageStep()); return true;
            case Event::Confirm:
                if (manageCursor_ >= 0 && manageCursor_ < count) {
                    const SystemInfo &s = all_[size_t(manageCursor_)];
                    library_.setSystemHidden(s.id, !s.hidden);
                    reload();
                }
                return true;
            case Event::Back:
                managing_ = false;
                reload();
                return true;
            default: return false;
        }
    }

    switch (event) {
        case Event::Up:    cursor_ = grid_.move(cursor_, int(shown_.size()), 0, -1); return true;
        case Event::Down:  cursor_ = grid_.move(cursor_, int(shown_.size()), 0, 1); return true;
        case Event::Left:  cursor_ = grid_.move(cursor_, int(shown_.size()), -1, 0); return true;
        case Event::Right: cursor_ = grid_.move(cursor_, int(shown_.size()), 1, 0); return true;
        case Event::Confirm:
            if (cursor_ >= 0 && cursor_ < int(shown_.size())) {
                request_ = shown_[size_t(cursor_)].id;
                return true;
            }
            return false;
        case Event::X:
            if (all_.empty()) return false;
            managing_ = true;
            manageCursor_ = std::max(0, std::min(manageCursor_, int(all_.size()) - 1));
            return true;
        default: return false;
    }
}

std::vector<Hint> SystemsTab::hints() const {
    if (managing_)
        return {hint(HintKey::UpDown, "hint.move", true), hint(HintKey::A, "systems.hint.toggle", true),
                hint(HintKey::LeftRight, "hint.page"), hint(HintKey::B, "hint.back", true)};
    std::vector<Hint> h;
    if (!shown_.empty()) {
        h.push_back(hint(HintKey::Dpad, "hint.move", true));
        h.push_back(hint(HintKey::A, "hint.open", true));
    }
    if (!all_.empty()) h.push_back(hint(HintKey::X, "systems.manage.title", true));
    return h;
}

void SystemsTab::draw(Painter &p, const Theme &t, const Rect &c) {
    PageHeaderSpec header;    // the one header: title left, info right, no search field
    if (managing_) {
        int on = 0;
        for (const SystemInfo &s : all_) on += s.hidden ? 0 : 1;
        header.title = Text::key("systems.manage.title");
        header.info = Text::data(format("systems.manage.count", {std::to_string(on), std::to_string(all_.size())}));
    } else {
        header.title = Text::key("tab.systems");
        if (!shown_.empty()) {
            header.info = Text::data(gridInfo(ViewMode::Grid, cursor_ + 1, int(shown_.size())));
        }
    }
    const Rect body = drawPageHeader(p, t, c, header);
    if (managing_) drawVisibility(p, t, body);
    else drawGrid(p, t, body);
}

void SystemsTab::drawGrid(Painter &p, const Theme &t, const Rect &area) {
    if (shown_.empty()) {
        const bool hiddenAll = !all_.empty();
        drawEmptyPage(p, t, area,
                      hiddenAll ? emptyPage("systems.allHidden.title", "systems.allHidden.why", "systems.allHidden.next")
                                : emptyPage("systems.empty.title", "systems.empty.why", "systems.empty.next"));
        return;
    }

    GridPage::Options o;
    for (const SystemInfo &s : shown_) o.secondLine = o.secondLine || s.noCore;   // the second line only says "No core" (D17)
    grid_.draw(p, t, area, false, int(shown_.size()), cursor_, true, o, [&](int i, bool, GridTile &tile) {
        const SystemInfo &s = shown_[size_t(i)];
        tile.label = s.name;                               // the name only, as Classic; "No core" where it applies
        if (s.noCore) tile.sublabel = Text::key("systems.noCore").str();
        tile.art = library_.systemArt(s.id, ArtSize::Grid);   // the icon, centered in the standard tile
        return true;
    });
}

void SystemsTab::drawVisibility(Painter &p, const Theme &t, const Rect &area) {
    const int listW = std::min(area.w, t.px(900));
    const Rect listArea{area.x, area.y, listW, area.h};
    const Rect rows{listArea.x, listArea.y, listArea.w - scrollbarGutter(t), listArea.h};
    const Density density = Density::Dense;
    list_.configure(rows, listItemFrameHeight(t, false, density), listItemPitch(t, false, density));
    list_.ensureVisible(manageCursor_);

    p.pushClip(listArea.inset(-t.px(4)));
    const int first = list_.scroll();
    const int last = std::min(int(all_.size()), first + list_.visibleRows());
    for (int i = first; i < last; ++i) {
        ListItemSpec spec;
        spec.kind = ListItemKind::Switch;
        spec.title = Text::data(all_[size_t(i)].name);
        spec.checked = !all_[size_t(i)].hidden;
        ListItemState state;
        state.cursor = i == manageCursor_;
        drawListItem(p, t, list_.row(i), spec, state);
    }
    drawScrollbar(p, t, listArea, int(all_.size()), list_.visibleRows(), list_.scroll());
    p.popClip();

    // What hiding does, in the room beside the list (when the screen is wide enough).
    const int gap = t.px(48);
    const Rect side{listArea.right() + gap, area.y, area.right() - listArea.right() - gap, area.h};
    if (side.w >= t.px(360))
        drawTextBlock(p, t, side, Text::key("systems.manage.explain").str(), TextRole::Body, TextTier::Muted);
}

}  // namespace patsgui
