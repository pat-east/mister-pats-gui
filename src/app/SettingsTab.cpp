// Settings tab: the Classic Settings screen. Left the categories, in the middle the rows of the focused category,
// right what the focused row does.
//   Up/Down  move              Right, A  into the rows of the category        Left, B  back to the categories
//   A        on a row: toggles a switch, steps a choice, or activates it (the category opens a page)
// B on the category pane is not used, so the shell leaves the GUI, as in Classic (the hint says "Leave" there).
// The tab only hosts: the rows, their values and what they do belong to the categories (src/app/settings/*Settings.*),
// and what a row opens is a SettingsPage on the tab's PageStack (src/app/settings/SettingsPage.h).

#include "app/SettingsTab.h"

#include <algorithm>

#include "app/settings/StandardPages.h"
#include "app/settings/StandardPagesDemo.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/Palettes.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace {

void strings(ui::Catalog &c) {
    c.set("settings.col.categories", "Categories");   // the column captions (Classic: upper case; here the wireframes' Label style)
    c.set("settings.col.help", "What this does");
    c.set("settings.hint.turnOn", "Turn on");         // A on a switch that is off
    c.set("settings.hint.turnOff", "Turn off");       // A on a switch that is on
    c.set("settings.notAvailable", "Not available: {1}");   // the help pane, under the help text of a disabled row
    registerStandardPageStrings(c);
    registerSettingsCategoryStrings(c);   // every category's labels, help, options and page texts
}

bool isHeading(const VisibleRow &r) { return r.kind == RowKind::Heading; }

}  // namespace

SettingsTab::SettingsTab(Services &s)
    : services_(s.settings ? *s.settings : fallback_),
      env_{services_, s.library, state_, pages_, s.openArcadeStatus},
      cats_(createSettingsCategories()) {
    pages_.setNotice(s.notice);   // PageStack::toast() is the shell's toast
    for (auto &cat : cats_) cat->attach(env_);
}

SettingsTab::~SettingsTab() = default;

std::vector<SettingsCategory *> SettingsTab::categories() const {
    std::vector<SettingsCategory *> out;
    for (const auto &c : cats_)
        if (c->shown(state_)) out.push_back(c.get());
    return out;
}

SettingsCategory *SettingsTab::focusedCategory() const {
    const std::vector<SettingsCategory *> cats = categories();
    if (cats.empty()) return nullptr;
    return cats[size_t(std::max(0, std::min(catCursor_, int(cats.size()) - 1)))];
}

std::vector<VisibleRow> SettingsTab::rows() const {
    SettingsCategory *cat = focusedCategory();
    return cat ? cat->rows(state_) : std::vector<VisibleRow>();
}

// The value of a Switch or cycling row: the category's real one when it has a service, else the tab's memory.
int SettingsTab::memory(const MenuRow &row) const {
    if (SettingsCategory *cat = focusedCategory()) {
        const int real = cat->current(row.id);
        if (real != SettingsCategory::kNoValue) return real;
    }
    const auto it = memory_.find(row.id);
    return it == memory_.end() ? row.initial : it->second;
}

ui::Text SettingsTab::label(const VisibleRow &r) const {
    if (!r.label.empty()) return ui::Text::data(r.label);
    return ui::Text::key((std::string("settings.") + r.row->id).c_str());
}

ui::Text SettingsTab::valueOf(const VisibleRow &r) const {
    SettingsCategory *cat = focusedCategory();
    if (!cat) return ui::Text::data(std::string());
    return cat->valueOf(r, state_, memory(*r.row));
}

bool SettingsTab::selectable(const std::vector<VisibleRow> &list, int index) const {
    return index >= 0 && index < int(list.size()) && !isHeading(list[size_t(index)]);
}

int SettingsTab::firstSelectable() const {
    const std::vector<VisibleRow> list = rows();
    for (int i = 0; i < int(list.size()); ++i)
        if (selectable(list, i)) return i;
    return 0;
}

void SettingsTab::clampRow() {
    const std::vector<VisibleRow> list = rows();
    if (list.empty()) { rowCursor_ = 0; return; }
    rowCursor_ = std::max(0, std::min(rowCursor_, int(list.size()) - 1));
    if (!selectable(list, rowCursor_)) rowCursor_ = firstSelectable();
}

void SettingsTab::setState(const MenuState &state) {
    state_ = state;
    const int count = int(categories().size());
    catCursor_ = std::max(0, std::min(catCursor_, count - 1));
    clampRow();
}

void SettingsTab::enterRows() {
    clampRow();
    inRows_ = !rows().empty();
}

void SettingsTab::move(int delta) {
    if (!inRows_) {
        const int next = ui::ListLayout::move(catCursor_, int(categories().size()), delta);
        if (next != catCursor_) { catCursor_ = next; rowCursor_ = firstSelectable(); }   // a category opens on its first row
        return;
    }
    const std::vector<VisibleRow> list = rows();
    int next = rowCursor_ + delta;
    while (next >= 0 && next < int(list.size()) && isHeading(list[size_t(next)])) next += delta;   // skip titles
    if (selectable(list, next)) rowCursor_ = next;
}

// A row id activated: the category handles it (a page, a task); otherwise the seam of the first level runs.
void SettingsTab::activateRow(SettingsCategory &cat, const std::string &id) {
    if (!cat.activate(id, pages_) && actions_.activate) actions_.activate(id);
}

void SettingsTab::activate() {
    const std::vector<VisibleRow> list = rows();
    SettingsCategory *cat = focusedCategory();
    if (!cat || !selectable(list, rowCursor_)) return;
    const VisibleRow &r = list[size_t(rowCursor_)];
    const MenuRow &m = *r.row;
    if (!r.enabled || r.kind == RowKind::Info) return;
    const std::string id = m.id;
    if (r.kind == RowKind::Switch) {
        const int value = memory(m) ? 0 : 1;
        memory_[id] = value;
        cat->changed(id, value);
        if (actions_.changed) actions_.changed(id, value);
        return;
    }
    const int count = optionCount(m);
    if (r.kind == RowKind::Value && count > 0) {
        const int value = (memory(m) + 1) % count;
        memory_[id] = value;
        cat->changed(id, value);
        if (actions_.changed) actions_.changed(id, value);
        return;
    }
    activateRow(*cat, id);   // the seam: a flow or submenu
}

bool SettingsTab::handle(Event e) {
    if (!pages_.empty()) return pages_.handle(e);   // an open page owns every event
    switch (e) {
        case Event::Up: move(-1); return true;
        case Event::Down: move(1); return true;
        case Event::Left: inRows_ = false; return true;
        case Event::Right: enterRows(); return true;
        case Event::Confirm:
        case Event::Start:   // Classic: Start acts like A on this screen (App::handleAction)
            if (inRows_) activate(); else enterRows();
            return true;
        case Event::Back:
            // A category can claim B wherever the cursor is (Classic: B cancels a running update download).
            for (SettingsCategory *cat : categories()) {
                const std::string row = cat->backActivates();
                if (row.empty()) continue;
                activateRow(*cat, row);
                return true;
            }
            if (!inRows_) return false;   // not used: the shell leaves, as in Classic
            inRows_ = false;
            return true;
        default: return false;
    }
}

// The hint line follows the focus (Hints.md: only keys that do something now): the categories pane offers A Open and
// B Leave (B there leaves the tab); on a row A is named by its kind (Turn on / Turn off, Change, Open, Choose) and a
// disabled or inert row lists no A (the help pane says why). A category may name B itself (while an update
// downloads: "B Cancel download"). An open page's hints are the whole help line.
std::vector<ui::Hint> SettingsTab::hints() const {
    if (!pages_.empty()) return pages_.hints();
    std::vector<ui::Hint> h;
    h.push_back(ui::hint(ui::HintKey::UpDown, "hint.move"));
    ui::Hint back = ui::hint(ui::HintKey::B, inRows_ ? "hint.back" : "hint.leave", true);
    for (const SettingsCategory *cat : categories()) {   // a category may name B itself ("Cancel download")
        std::vector<ui::Hint> claimed;
        if (!cat->hintsHook(claimed)) continue;
        for (const ui::Hint &c : claimed)
            if (c.key == ui::HintKey::B) back = c;
        break;
    }
    if (!inRows_) {
        if (!categories().empty()) h.push_back(ui::hint(ui::HintKey::A, "hint.open", true));
        h.push_back(back);
        return h;
    }
    const std::vector<VisibleRow> list = rows();
    if (rowCursor_ >= 0 && rowCursor_ < int(list.size())) {
        const VisibleRow &r = list[size_t(rowCursor_)];
        if (r.enabled && r.kind != RowKind::Info && r.kind != RowKind::Heading) {
            const MenuRow &m = *r.row;
            const char *word = "hint.choose";
            if (r.kind == RowKind::Switch) word = memory(m) ? "settings.hint.turnOff" : "settings.hint.turnOn";
            else if (r.kind == RowKind::Value) word = optionCount(m) > 0 ? "hint.change" : "hint.open";
            h.push_back(ui::hint(ui::HintKey::A, word, true));
        }
    }
    h.push_back(back);
    return h;
}

// ---- The page flow, the host's hooks and the tests.

bool SettingsTab::update(float dt) {
    if (!shown_) {   // what Classic's App polls on every tab (the update snapshot): the categories' background hook
        bool redraw = false;
        for (const auto &cat : cats_)
            if (cat->background(dt)) redraw = true;
        return redraw;
    }
    bool redraw = false;
    for (const auto &cat : cats_)
        if (cat->update(dt)) redraw = true;
    if (pages_.update(dt)) redraw = true;
    if (redraw) clampRow();
    return redraw;
}

void SettingsTab::onShow() {
    shown_ = true;
    for (const auto &cat : cats_) cat->onShow();
}

void SettingsTab::onHide() {
    shown_ = false;
    pages_.clear();   // every open page is closed (their onClose stops what only they started)
    for (const auto &cat : cats_) cat->onHide();
}

bool SettingsTab::locked() const { return pages_.locked(); }
bool SettingsTab::dialogOpen() const { return pages_.dialogOpen(); }

bool SettingsTab::wantsAttention() const {
    for (const auto &cat : cats_)
        if (cat->wantsAttention()) return true;
    return false;
}

ui::Text SettingsTab::footerNote() const {
    for (const auto &cat : cats_) {
        const ui::Text note = cat->footerNote();
        if (!note.empty()) return note;
    }
    return ui::Text();
}

bool SettingsTab::openPageForTest(const std::string &categoryId, const std::string &pageId) {
    if (categoryId == "standard") return openStandardPageDemo(pageId, pages_);   // the helper pages with sample content
    const std::vector<SettingsCategory *> cats = categories();
    for (size_t i = 0; i < cats.size(); ++i) {
        if (categoryId != cats[i]->id()) continue;
        catCursor_ = int(i);
        rowCursor_ = firstSelectable();
        return cats[i]->openPage(pageId, pages_);
    }
    return false;
}

bool SettingsTab::activateRowForTest(const std::string &categoryId, const std::string &rowId) {
    for (const auto &cat : cats_)
        if (categoryId == cat->id()) { activateRow(*cat, rowId); return true; }
    return false;
}

std::vector<std::string> SettingsTab::pageIdsForTest() const {
    std::vector<std::string> out;
    for (const auto &cat : cats_)
        for (const std::string &page : cat->pageIds()) out.push_back(std::string(cat->id()) + "/" + page);
    for (const std::string &page : standardPageDemoIds()) out.push_back("standard/" + page);
    return out;
}

void SettingsTab::focusCategoryForTest(const std::string &categoryId) {
    const std::vector<SettingsCategory *> cats = categories();
    for (size_t i = 0; i < cats.size(); ++i)
        if (categoryId == cats[i]->id()) { catCursor_ = int(i); rowCursor_ = firstSelectable(); inRows_ = true; }
}

void SettingsTab::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) {
    if (!pages_.coversPanes()) drawPanes(p, t, c);
    pages_.draw(p, t, c);
}

void SettingsTab::drawPanes(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) {
    ui::PageHeaderSpec header;
    header.title = ui::Text::key("tab.settings");
    const ui::Rect body = ui::drawPageHeader(p, t, c, header);

    clampRow();
    const int gap = t.gap();
    const int usable = body.w - 2 * gap;
    const int catW = std::max(0, usable / 3), rowW = catW;   // Classic: three equal columns
    const ui::Rect catPanel{body.x, body.y, catW, body.h};
    const ui::Rect rowPanel{catPanel.right() + gap, body.y, rowW, body.h};
    const ui::Rect helpPanel{rowPanel.right() + gap, body.y, body.right() - rowPanel.right() - gap, body.h};
    for (const ui::Rect &r : {catPanel, rowPanel, helpPanel})
        ui::drawSurface(p, t, r, ui::SurfaceRole::Faint, t.px(ui::Radius::panel));

    // A caption over each pane (Title Case, small, bold, muted), all at the same inset as the row titles and the help
    // text; the middle one names the focused category, so the rows say what they belong to.
    const int caption = t.px(34);
    const int capInset = t.px(28);
    SettingsCategory *focused = focusedCategory();
    const ui::Text rowsCaption = focused ? ui::Text::key((std::string("settings.cat.") + focused->id()).c_str())
                                         : ui::Text::data(std::string());
    const ui::Rect captions[3] = {catPanel, rowPanel, helpPanel};
    const ui::Text captionText[3] = {ui::Text::key("settings.col.categories"), rowsCaption, ui::Text::key("settings.col.help")};
    for (int i = 0; i < 3; ++i)
        ui::drawLabel(p, t, {captions[i].x + capInset, captions[i].y, captions[i].w - 2 * capInset, caption},
                      captionText[i].str(), ui::TextRole::Small, ui::TextTier::Muted, ui::Align::Left, true);

    const int frame = ui::listItemFrameHeight(t, false);
    const int pitch = ui::listItemPitch(t, false);
    const int pad = t.px(10);

    // Categories: Nav rows (A, or Right, opens the category's rows).
    const std::vector<SettingsCategory *> cats = categories();
    const ui::Rect catArea{catPanel.x + pad, catPanel.y + caption, catPanel.w - 2 * pad, catPanel.h - caption - pad};
    catLayout_.configure(catArea, frame, pitch);
    // The scrollbar always has its own strip, so the values do not move when a category starts to scroll.
    catLayout_.configure({catArea.x, catArea.y, catArea.w - (ui::scrollbarGutter(t) - pad), catArea.h}, frame, pitch);
    catLayout_.ensureVisible(catCursor_);
    for (int i = 0; i < int(cats.size()); ++i) {
        if (!catLayout_.visible(i)) continue;
        ui::ListItemSpec spec;
        spec.kind = ui::ListItemKind::Nav;
        spec.title = ui::Text::key((std::string("settings.cat.") + cats[size_t(i)]->id()).c_str());
        spec.value = ui::Text::data(std::string());
        ui::ListItemState st;
        st.cursor = i == catCursor_;
        st.paneActive = !inRows_;
        ui::drawListItem(p, t, catLayout_.row(i), spec, st);
    }
    ui::drawScrollbar(p, t, catPanel, int(cats.size()), catLayout_.visibleRows(), catLayout_.scroll());

    // Rows of the focused category.
    const std::vector<VisibleRow> list = rows();
    const ui::Rect rowArea{rowPanel.x + pad, rowPanel.y + caption, rowPanel.w - 2 * pad, rowPanel.h - caption - pad};
    rowLayout_.configure(rowArea, frame, pitch);
    rowLayout_.configure({rowArea.x, rowArea.y, rowArea.w - (ui::scrollbarGutter(t) - pad), rowArea.h}, frame, pitch);
    rowLayout_.ensureVisible(rowCursor_);
    for (int i = 0; i < int(list.size()); ++i) {
        if (!rowLayout_.visible(i)) continue;
        const VisibleRow &r = list[size_t(i)];
        const MenuRow &m = *r.row;
        ui::ListItemSpec spec;
        spec.title = label(r);
        spec.disabled = !r.enabled;
        switch (r.kind) {
            case RowKind::Heading: spec.kind = ui::ListItemKind::Heading; break;
            case RowKind::Info: spec.kind = ui::ListItemKind::Info; break;
            case RowKind::Switch: spec.kind = ui::ListItemKind::Switch; spec.checked = memory(m) != 0; break;
            // A value that cycles is a Value; a value that opens a page or starts a task is a Nav with its value.
            case RowKind::Value: spec.kind = optionCount(m) > 0 ? ui::ListItemKind::Value : ui::ListItemKind::Nav; break;
            case RowKind::Action: spec.kind = ui::ListItemKind::Action; break;
        }
        spec.value = valueOf(r);
        if (!r.enabled && m.disabledValue) spec.kind = ui::ListItemKind::Value;   // "No adapter", "No drive": no A glyph
        ui::ListItemState st;
        st.cursor = i == rowCursor_;   // the remembered row stays marked while the categories have focus
        st.paneActive = inRows_;
        ui::drawListItem(p, t, rowLayout_.row(i), spec, st);
    }
    ui::drawScrollbar(p, t, rowPanel, int(list.size()), rowLayout_.visibleRows(), rowLayout_.scroll());

    // What the focused row does: its label as the heading, then Classic's help text.
    if (rowCursor_ >= 0 && rowCursor_ < int(list.size())) {
        const VisibleRow &r = list[size_t(rowCursor_)];
        const ui::Rect inner{helpPanel.x + t.px(24), helpPanel.y + caption + t.px(ui::Space::titleGap), helpPanel.w - t.px(48),
                             helpPanel.h - caption - 2 * t.px(ui::Space::titleGap)};
        const int used = ui::drawTextBlock(p, t, inner, label(r).str(), ui::TextRole::Heading, ui::TextTier::Primary, true);
        int top = used + t.px(ui::Space::titleGap);
        if (r.row->help) {
            top += ui::drawTextBlock(p, t, {inner.x, inner.y + top, inner.w, std::max(0, inner.h - top)},
                                     ui::Text::key((std::string("settings.") + r.row->id + ".help").c_str()).str(),
                                     ui::TextRole::Body, ui::TextTier::Primary);
        }
        if (!r.enabled && r.row->reason) {   // why the row does nothing: words, not only a grey row
            top += t.px(ui::Space::titleGap);
            const std::string reason = ui::format("settings.notAvailable",
                                               {ui::Text::key((std::string("settings.") + r.row->id + ".reason").c_str()).str()});
            ui::drawTextBlock(p, t, {inner.x, inner.y + top, inner.w, std::max(0, inner.h - top)}, reason,
                              ui::TextRole::Body, ui::TextTier::Primary, true);
        }
    }
}

PATSGUI_REGISTER_TAB(Tab::Settings, SettingsTab, strings);

}  // namespace patsgui
