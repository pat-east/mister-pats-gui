// The Arcade tab: the Games view over the arcade system (GamesTab does the grid, list, search, letters,
// favorites and the version choice, as in Classic) and the "Arcade game status" screen: the diagnostic
// table of every arcade game (works, core, ROM, CRC), the check that fills it, the Might work / Broken
// filter, letter jump and launching. See src/app/PARITY-arcade.md for the comparison with Classic.

#include "app/ArcadeTab.h"

#include <algorithm>

#include "app/EmptyPages.h"
#include "app/LetterJump.h"
#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Data.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

namespace {

using ui::Text;

std::string gameKey(const GameRef &r) { return r.system + "\n" + r.id; }

void strings(ui::Catalog &c) {
    c.set("arcade.none.title", "No arcade games found");
    c.set("arcade.none.why",
          "This tab lists the arcade .mra files in the _Arcade folder of your drives, and none were found.");
    c.set("arcade.none.next",
          "Put your arcade .mra files in the _Arcade folder, with the ROM files they need. Then press X to check the games.");
    c.set("arcade.hint.status", "Game status");
    c.set("arcade.status.title", "Arcade game status");
    c.set("arcade.col.works", "Works?");
    c.set("arcade.col.name", "Name");
    c.set("arcade.col.year", "Year");
    c.set("arcade.col.maker", "Manufacturer");
    c.set("arcade.col.category", "Category");
    c.set("arcade.col.core", "Core?");
    c.set("arcade.col.coreFile", "Core file");
    c.set("arcade.col.rom", "ROM?");
    c.set("arcade.col.crc", "CRC?");
    c.set("arcade.col.romFile", "ROM file");
    c.set("arcade.flag.yes", "Yes");
    c.set("arcade.flag.no", "No");
    c.set("arcade.flag.unchecked", "Not checked");
    c.set("arcade.filter.all", "All");
    c.set("arcade.filter.working", "Might work");
    c.set("arcade.filter.broken", "Broken");
    c.set("arcade.summary", "{1} of {2} might work");
    c.set("arcade.summary.showing", "Showing: {1}");
    c.set("arcade.scan.title", "Checking arcade games");
    c.set("arcade.scan.finding", "Looking for arcade game files\xE2\x80\xA6");
    c.set("arcade.scan.checking", "Checking game {1} of {2}");
    c.set("arcade.scan.detail.one", "{1} looks ok so far");
    c.set("arcade.scan.detail.other", "{1} look ok so far");
    c.set("arcade.empty.title", "No arcade game files found");
    c.set("arcade.empty.why", "No arcade game files were found on the connected drives.");
    c.set("arcade.empty.next", "Put your .mra files in the _Arcade folder of a drive, then open this screen again.");
    c.set("arcade.empty.found.one", "{1} game was checked, {2} might work. It is not in the games list yet.");
    c.set("arcade.empty.found.other", "{1} games were checked, {2} might work. They are not in the games list yet.");
    c.set("arcade.empty.foundNext", "Build the game database in Settings to add them to the list.");
    c.set("arcade.filtered.title", "Nothing in \xE2\x80\x9C{1}\xE2\x80\x9D");
    c.set("arcade.filtered.why", "No arcade game matches this filter.");
    c.set("arcade.filtered.next", "Press {Y} to change the filter.");
        c.set("arcade.hint.filter.all", "Filter: All");
    c.set("arcade.hint.filter.working", "Filter: Might work");
    c.set("arcade.hint.filter.broken", "Filter: Broken");
    c.set("arcade.hint.cancelScan", "Cancel the check and go back");
}

const char *filterKey(Works w) {
    return w == Works::All ? "arcade.filter.all" : w == Works::Working ? "arcade.filter.working" : "arcade.filter.broken";
}

}  // namespace

ArcadeTab::ArcadeTab(Services &services) : library_(services.library), notice_(services.notice), games_(services) { games_.setArcade(true); readArcadeSystem(); }

// ---------------------------------------------------------------- the arcade system

void ArcadeTab::readArcadeSystem() {
    SystemId id;
    for (const SystemInfo &s : library_.systems())
        if (s.isArcade) { id = s.id; break; }
    const bool changed = id != arcadeId_;
    arcadeId_ = id;
    hasArcade_ = !id.empty();
    revision_ = library_.revision();
    if (hasArcade_ && (changed || games_.system() != id)) games_.openSystem(id);
}

void ArcadeTab::onShow() {
    readArcadeSystem();
    if (hasArcade_) games_.onShow();
}

// The tab is left: nothing may keep running for it (the status screen's check, the games view's preview).
void ArcadeTab::onHide() {
    if (status_) closeStatus();
    games_.onHide();
}

void ArcadeTab::openStatus() {
    games_.onHide();                  // the games view is not on screen: no preview requests meanwhile
    status_ = true;
    options_.show();
    works_ = Works::All;
    cursor_ = 0;
    list_ = ui::ListLayout();
    files_.clear();
    library_.startArcadeScan();       // the user asked: Classic restarts the check each time this opens
    rebuildStatus();
}

void ArcadeTab::closeStatus() {
    if (library_.arcadeScan().phase == ArcadeScanState::Phase::Running) library_.cancelArcadeScan();
    status_ = false;
    options_.hide();
    readArcadeSystem();
    if (hasArcade_) games_.onShow();
}

void ArcadeTab::rebuildStatus() {
    windowValid_ = false;
    if (arcadeId_.empty()) { total_ = allTotal_ = 0; return; }
    allTotal_ = std::max(0, library_.gameCount(arcadeId_, Works::All));
    total_ = works_ == Works::All ? allTotal_ : std::max(0, library_.gameCount(arcadeId_, works_));
    cursor_ = std::max(0, std::min(cursor_, total_ - 1));
}

bool ArcadeTab::update(float dt) {
    bool changed = false;
    const uint64_t revision = library_.revision();
    if (revision != revision_) {
        readArcadeSystem();
        if (status_) rebuildStatus();
        changed = true;
    }
    if (status_) {
        if (options_.update(dt)) changed = true;
        if (library_.arcadeScan().phase == ArcadeScanState::Phase::Running) changed = true;
        if (fetchDetails()) changed = true;
    } else if (hasArcade_) {
        if (games_.update(dt)) changed = true;
    }
    return changed;
}

bool ArcadeTab::locked() const { return status_ || games_.locked(); }

std::string ArcadeTab::favoriteKey() const {
    if (status_ || !hasArcade_) return std::string();
    return games_.favoriteKey();
}

// ---------------------------------------------------------------- the status table

void ArcadeTab::ensureWindow(int first, int count) {
    if (windowValid_ && first >= windowFirst_ && first + count <= windowFirst_ + int(window_.size())) return;
    const int lo = std::max(0, first - count), hi = std::min(total_, first + 2 * count);
    windowFirst_ = lo;
    window_.clear();
    windowValid_ = true;
    if (hi > lo) window_ = library_.games(arcadeId_, lo, hi - lo, works_);
}

const GameInfo *ArcadeTab::gameAt(int index) const {
    const int i = index - windowFirst_;
    if (!windowValid_ || i < 0 || i >= int(window_.size())) return nullptr;
    return &window_[size_t(i)];
}

// The core and ROM file names come from detail(): a few per frame for the rows on screen, remembered.
bool ArcadeTab::fetchDetails() {
    if (arcadeId_.empty() || total_ == 0) return false;
    ensureWindow(list_.scroll(), list_.visibleRows());
    int budget = 6;
    bool fetched = false;
    for (int i = list_.scroll(); i < std::min(total_, list_.scroll() + list_.visibleRows()) && budget > 0; ++i) {
        const GameInfo *g = gameAt(i);
        if (!g || !g->checked || files_.count(gameKey(g->ref))) continue;
        const GameDetail d = library_.detail(g->ref);
        files_[gameKey(g->ref)] = {d.coreFile, d.romFile};
        --budget;
        fetched = true;
    }
    return fetched;
}

void ArcadeTab::moveCursor(int delta) {
    cursor_ = ui::ListLayout::move(cursor_, total_, delta);
}

// Started: the host hands the screen to the core. Failed: the host shows its error dialog. Several ROM versions:
// the Game options open, as on every tab, and the version is chosen there.
void ArcadeTab::play() {
    const GameInfo *g = gameAt(cursor_);
    if (g && library_.launch(g->ref).status == LaunchStatus::NeedsVersion) options_.open(g->ref);
}

void ArcadeTab::drawStatus(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) {
    using namespace ui;
    textScale_ = t.textScale();
    const ArcadeScanState scan = library_.arcadeScan();

    // The page header, as on every tab: the title, and the summary (how many might work, and the filter) as its info.
    PageHeaderSpec header;
    header.title = Text::key("arcade.status.title");
    // Classic's numbers: "n of m might work" counts the checked .mra files (m = what the check read), not the rows below.
    if (scan.phase != ArcadeScanState::Phase::Running && scan.entries > 0)
        header.info = Text::data(ui::format("arcade.summary", {std::to_string(scan.working), std::to_string(scan.entries)}) + "   " +
                                 ui::format("arcade.summary.showing", {Text::key(filterKey(works_)).str()}));
    const ui::Rect body = drawPageHeader(p, t, c, header);

    if (scan.phase == ArcadeScanState::Phase::Running) {
        ProgressPanelSpec spec;
        spec.title = Text::key("arcade.scan.title");
        spec.status = scan.total > 0
            ? Text::data(ui::format("arcade.scan.checking", {std::to_string(scan.checked), std::to_string(scan.total)}))
            : Text::key("arcade.scan.finding");
        if (scan.total > 0)
            spec.detail = Text::data(ui::plural("arcade.scan.detail", scan.working));
        spec.fraction = scan.progress;
        drawSurface(p, t, body, SurfaceRole::Faint, t.px(Radius::panel));
        drawProgressPanel(p, t, body.inset(t.px(Space::pad)), spec);
        return;
    }

    if (arcadeId_.empty() || allTotal_ == 0) {
        const bool checkedSome = scan.phase == ArcadeScanState::Phase::Done && scan.checked > 0;
        EmptyPageSpec page = emptyPage("arcade.empty.title", "arcade.empty.why", "arcade.empty.next");
        if (checkedSome) {
            page.why = Text::data(ui::plural("arcade.empty.found", scan.checked, {std::to_string(scan.working)}));
            page.next = Text::key("arcade.empty.foundNext");
        }
        drawEmptyPage(p, t, body, page);
        return;
    }

    const int scrollW = t.px(24);
    const Rect table{body.x, body.y, body.w - scrollW, body.h};
    const int headerH = t.px(36), top = t.px(6), rowH = tableRowHeight(t);
    const Rect rowsArea{table.x, table.y + headerH + top, table.w, std::max(0, table.h - headerH - top)};
    list_.configure(rowsArea, rowH, rowH);
    list_.ensureVisible(cursor_);
    const int scroll = list_.scroll(), visible = list_.visibleRows();
    ensureWindow(scroll, visible);

    // Columns of Classic's table, in its order and weights; the category and the core file go at the largest
    // text. Flags are glyphs while every row on screen has been checked, words otherwise.
    glyphs_ = true;
    for (int i = scroll; i < std::min(total_, scroll + visible); ++i)
        if (const GameInfo *g = gameAt(i)) glyphs_ = glyphs_ && g->checked;
    const bool narrow = textScale_ > 115;
    const CellKind flagKind = glyphs_ ? CellKind::Flag : CellKind::Text;
    struct Col { const char *key; int weight; CellKind kind; char field; };
    std::vector<Col> cols = {{"arcade.col.works", 7, flagKind, 'w'}, {"arcade.col.name", 19, CellKind::Text, 'n'},
                             {"arcade.col.year", 5, CellKind::Text, 'y'}, {"arcade.col.maker", 12, CellKind::Text, 'm'}};
    if (!narrow) cols.push_back({"arcade.col.category", 10, CellKind::Text, 'c'});
    cols.push_back({"arcade.col.core", 7, flagKind, 'k'});
    if (!narrow) cols.push_back({"arcade.col.coreFile", 13, CellKind::Text, 'f'});
    cols.push_back({"arcade.col.rom", 7, flagKind, 'r'});
    cols.push_back({"arcade.col.crc", 7, flagKind, 'x'});
    cols.push_back({"arcade.col.romFile", 17, CellKind::Text, 'z'});
    int sum = 0;
    for (const Col &col : cols) sum += col.weight;
    cols[1].weight += 100 - sum;                    // the name takes what the hidden columns leave

    std::vector<TableColumn> columns;
    for (const Col &col : cols) {
        TableColumn tc;
        tc.header = Text::key(col.key);
        tc.weight = col.weight;
        tc.kind = col.kind;
        columns.push_back(tc);
    }
    std::vector<TableRow> rows(static_cast<size_t>(scroll));
    for (int i = scroll; i < std::min(total_, scroll + visible); ++i) {
        const GameInfo *g = gameAt(i);
        TableRow row;
        for (const Col &col : cols) {
            TableCell cell;
            bool flag = false;
            switch (col.field) {
                case 'w': flag = g && g->works; break;
                case 'k': flag = g && g->coreFound; break;
                case 'r': flag = g && g->romFound; break;
                case 'x': flag = g && g->romCrcOk; break;
                case 'n': cell.text = g ? g->title : std::string(); break;
                case 'y': cell.text = g ? g->year : std::string(); break;
                case 'm': cell.text = g ? g->manufacturer : std::string(); break;
                case 'c': cell.text = g ? g->category : std::string(); break;
                case 'f': case 'z': {
                    auto it = g ? files_.find(gameKey(g->ref)) : files_.end();
                    if (it != files_.end()) cell.text = col.field == 'f' ? it->second.core : it->second.rom;
                    break;
                }
            }
            if (col.kind == CellKind::Flag) cell.flag = flag;
            else if (col.field == 'w' || col.field == 'k' || col.field == 'r' || col.field == 'x')
                cell.text = Text::key(!g || !g->checked ? "arcade.flag.unchecked" : flag ? "arcade.flag.yes" : "arcade.flag.no").str();
            row.push_back(cell);
        }
        rows.push_back(row);
    }
    drawTable(p, t, table, columns, rows, cursor_, scroll);
    drawScrollbar(p, t, {rowsArea.x, rowsArea.y, rowsArea.w + scrollW, rowsArea.h}, total_, visible, scroll);

    if (total_ == 0) {
        const Rect b{table.x, table.y + headerH + top, table.w + scrollW, table.h - headerH - top};
        EmptyPageSpec page = emptyPage("arcade.filtered.title", "arcade.filtered.why", "arcade.filtered.next");
        page.title = Text::data(ui::format("arcade.filtered.title", {Text::key(filterKey(works_)).str()}));
        drawEmptyPage(p, t, b, page);
    }
}

void ArcadeTab::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &c) {
    if (status_) {
        drawStatus(p, t, c);
        options_.draw(p, t, p.bounds());   // the version choice, over the table; the shell draws its hints
        return;
    }
    if (!hasArcade_) {
        ui::drawEmptyPage(p, t, c, emptyPage("arcade.none.title", "arcade.none.why", "arcade.none.next"));
        return;
    }
    games_.draw(p, t, c);
}

// ---------------------------------------------------------------- input

bool ArcadeTab::handleStatus(Event e) {
    if (options_.isOpen()) return options_.handle(e);   // it takes every event while open
    const bool running = library_.arcadeScan().phase == ArcadeScanState::Phase::Running;
    if (e == Event::Back) {
        closeStatus();
        return true;
    }
    if (running || total_ == 0) {
        if (e == Event::Y && !running && allTotal_ > 0) { works_ = Works::All; rebuildStatus(); }
        return true;   // nothing to move on while the check runs; tabs stay locked
    }
    switch (e) {
        case Event::Up: moveCursor(-1); break;
        case Event::Down: moveCursor(1); break;
        case Event::Left: moveCursor(-list_.pageStep()); break;    // a page: the visible rows (D10), as in every list
        case Event::Right: moveCursor(list_.pageStep()); break;
        case Event::LetterPrev:
        case Event::LetterNext:
            {
                const int target = library_.jumpLetter(arcadeId_, cursor_, e == Event::LetterNext ? 1 : -1, works_);
                if (target != cursor_) {
                    cursor_ = target;
                    const std::vector<GameInfo> at = library_.games(arcadeId_, target, 1, works_);
                    if (!at.empty()) LetterJump::announce(notice_, at[0].title);   // the same notice as every tab
                }
            }
            break;
        case Event::Confirm: play(); break;
        case Event::Y:
            works_ = works_ == Works::All ? Works::Working : works_ == Works::Working ? Works::Broken : Works::All;
            cursor_ = 0;
            list_ = ui::ListLayout();
            rebuildStatus();
            break;
        default: break;
    }
    return true;
}

bool ArcadeTab::handle(Event e) {
    if (status_) return handleStatus(e);
    if (!hasArcade_) {
        if (e == Event::X) { openStatus(); return true; }
        return false;
    }
    if (games_.handle(e)) return true;
    if (e == Event::X) { openStatus(); return true; }       // only arrives while the focus is not on a game
    return false;
}

std::vector<ui::Hint> ArcadeTab::hints() const {
    using ui::HintKey;
    if (status_) {
        if (options_.isOpen()) return options_.hints();
        if (library_.arcadeScan().phase == ArcadeScanState::Phase::Running)
            return {ui::hint(HintKey::B, "arcade.hint.cancelScan", true)};
        std::vector<ui::Hint> h;
        if (total_ > 0) {
            h.push_back(ui::hint(HintKey::A, "hint.play", true));
            h.push_back(ui::hint(HintKey::UpDown, "hint.move"));
            h.push_back(ui::hint(HintKey::LeftRight, "hint.page"));
            h.push_back(ui::hint(HintKey::Triggers, "hint.jump"));
        }
        if (allTotal_ > 0)
            h.push_back(ui::hint(HintKey::Y, works_ == Works::All ? "arcade.hint.filter.all"
                                         : works_ == Works::Working ? "arcade.hint.filter.working" : "arcade.hint.filter.broken"));
        h.push_back(ui::hint(HintKey::B, "hint.back", true));
        return h;
    }
    if (!hasArcade_) return {ui::hint(HintKey::X, "arcade.hint.status", true)};
    std::vector<ui::Hint> h = games_.hints();
    if (!games_.locked() && games_.favoriteKey().empty()) {
        bool xUsed = false;
        for (const ui::Hint &x : h) xUsed = xUsed || x.key == HintKey::X;
        if (!xUsed) h.push_back(ui::hint(HintKey::X, "arcade.hint.status"));
    }
    return h;
}

PATSGUI_REGISTER_TAB(Tab::Arcade, ArcadeTab, strings);

}  // namespace patsgui
