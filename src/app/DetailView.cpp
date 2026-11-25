#include "app/DetailView.h"
#include "app/PreviewSlot.h"

#include <algorithm>

#include "ui-framework/kit/Data.h"
#include "ui-framework/kit/Input.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

namespace {

using ui::Text;

}  // namespace

void DetailView::registerStrings(ui::Catalog &c) {
    c.set("detail.shots.loading", "Loading screenshots\xE2\x80\xA6");
    c.set("detail.shots.none", "No screenshots available");
    c.set("detail.hint.pickVersion", "Select version");
}

void DetailView::reset() {
    cursor_ = 0;
    selected_ = 0;
    list_ = ui::ListLayout();
}

int DetailView::shownShots(const DetailSubject &s) const {
    return s.shotsPhase == ShotsPhase::Ready ? std::max(0, std::min(s.shotCount, kMaxShots)) : 0;
}

DetailView::Result DetailView::handle(Event event, const DetailSubject &s) {
    Result r;
    const int rows = int(s.versions.size());
    const int shots = shownShots(s);
    if (cursor_ >= kShotBase && (!shotsFocusable(s) || cursor_ - kShotBase >= shots)) cursor_ = 0;
    if (cursor_ >= 1 && cursor_ < kShotBase && cursor_ > rows) cursor_ = 0;
    switch (event) {
        case Event::Up:
            if (cursor_ == 0) { if (rows > 0) { cursor_ = rows; r.used = true; } }
            else if (cursor_ >= kShotBase) { cursor_ = 0; r.used = true; }
            else { cursor_ = std::max(1, cursor_ - 1); r.used = true; }
            return r;
        case Event::Down:
            if (cursor_ == 0) { if (shotsFocusable(s)) { cursor_ = kShotBase; r.used = true; } }
            else if (cursor_ < kShotBase) { cursor_ = cursor_ < rows ? cursor_ + 1 : 0; r.used = true; }
            return r;
        case Event::Left:
            if (cursor_ > kShotBase) { --cursor_; r.used = true; }
            return r;
        case Event::Right:
            if (cursor_ >= kShotBase && cursor_ - kShotBase + 1 < shots) { ++cursor_; r.used = true; }
            return r;
        case Event::Confirm:
            r.used = true;
            if (cursor_ >= kShotBase) { r.action = Action::ShowShot; r.index = cursor_ - kShotBase; }
            else if (cursor_ == 0) { r.action = Action::Start; }
            else { selected_ = cursor_ - 1; r.action = Action::VersionChosen; r.index = selected_; cursor_ = 0; }
            return r;
        default:
            return r;
    }
}

std::vector<ui::Hint> DetailView::hints(const DetailSubject &s) const {
    std::vector<ui::Hint> o;
    o.push_back(ui::hint(ui::HintKey::UpDown, "hint.move"));
    if (onShot()) {
        o.push_back(ui::hint(ui::HintKey::LeftRight, "hint.browse"));
        o.push_back(ui::hint(ui::HintKey::A, "hint.viewShot", true));
    } else if (onButton()) {
        o.push_back(ui::hint(ui::HintKey::A, "hint.startGame", true));
    } else {
        o.push_back(ui::hint(ui::HintKey::A, "detail.hint.pickVersion", true));
    }
    (void)s;
    return o;
}

void DetailView::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, const DetailSubject &s) {
    using namespace ui;
    const int bodyH = p.lineHeight(FontRole::Regular, t.size(TextRole::Body));
    const int smallH = p.lineHeight(FontRole::Bold, t.size(TextRole::Small));
    const int gap = t.px(Space::gap);

    // ---- Left: the picture and the preview's status line under it.
    const int statusH = bodyH;
    const int pictureW = std::min(t.px(744), area.w * 2 / 5);
    picture_ = {area.x, area.y, pictureW, std::max(0, area.h - statusH - t.px(8))};
    drawSurface(p, t, picture_, SurfaceRole::Faint, t.px(Radius::row));
    if (s.picture && s.picture->valid()) {
        PreviewSlot::drawPicture(p, t, picture_, *s.picture, s.pictureIsVideo);   // the code redrawVideo() repeats per frame
    } else {
        p.pushClip(picture_);
        const Text &word = s.pictureLoading
            ? (s.pictureLoadingText.empty() ? Text::key("picture.loading") : s.pictureLoadingText)
            : (s.pictureNoneText.empty() ? Text::key("picture.none") : s.pictureNoneText);
        drawLabel(p, t, picture_.inset(t.px(16)), word.str(), TextRole::Body, TextTier::Muted, Align::Center);
        p.popClip();
    }
    if (!s.pictureStatus.empty())
        drawLabel(p, t, {picture_.x, picture_.bottom() + t.px(8), picture_.w, statusH}, s.pictureStatus, TextRole::Body,
                  TextTier::Muted);

    // ---- Right column, laid out from the bottom: the screenshot strip, then the Start button.
    const int rx = picture_.right() + t.px(36);
    const Rect col{rx, area.y, std::max(0, area.right() - rx), area.h};
    int bottom = col.bottom();

    const int thumbGap = t.px(14);
    const int thumbW = (col.w - thumbGap * (kMaxShots - 1)) / kMaxShots;
    const int thumbH = thumbW * 9 / 16;
    const int shots = shownShots(s);
    if (s.shotsPhase != ShotsPhase::None) {
        const int frameOut = std::max(2, t.px(3));
        const Rect strip{col.x, bottom - thumbH, col.w, thumbH};
        bool anyHope = false;   // a thumbnail is on screen or on its way
        for (int i = 0; i < shots; ++i) {
            const Art a = s.shot ? s.shot(i, ArtSize::Grid) : Art();
            if ((a.image && a.image->valid()) || a.loading) anyHope = true;
        }
        if (shots > 0 && anyHope) {
            for (int i = 0; i < shots; ++i) {
                const Rect r{col.x + i * (thumbW + thumbGap), strip.y, thumbW, thumbH};
                drawSurface(p, t, r, SurfaceRole::Faint, t.px(Radius::chip));   // a calm tile until the picture arrives
                const Art a = s.shot ? s.shot(i, ArtSize::Grid) : Art();
                if (a.image && a.image->valid())
                    p.drawImage(*a.image, r.fitAspect(a.image->width(), a.image->height()), 255, t.px(Radius::chip));
                if (cursor_ == kShotBase + i) drawFocusFrame(p, t, r.inset(-frameOut), t.px(Radius::chip) + frameOut);
            }
        } else {
            // Nothing to show: say why in the one tile (no row of empty frames).
            drawSurface(p, t, strip, SurfaceRole::Faint, t.px(Radius::chip));
            const bool loading = s.shotsPhase == ShotsPhase::Searching || (shots > 0 && s.shotsPhase == ShotsPhase::Ready);
            drawLabel(p, t, strip, Text::key(loading ? "detail.shots.loading" : "detail.shots.none").str(),
                      TextRole::Small, TextTier::Muted, Align::Center);
        }
        bottom = strip.y - t.px(8);
        if (!s.shotsLabel.empty()) {
            bottom -= smallH;
            drawLabel(p, t, {col.x, bottom, col.w, smallH}, s.shotsLabel.str(), TextRole::Small, TextTier::Muted,
                      Align::Left, true);
            bottom -= t.px(8);
        } else {
            bottom -= t.px(4);
        }
    }
    const int btnH = std::max(t.px(62), p.lineHeight(FontRole::Bold, t.size(TextRole::Body)) + t.px(24));
    const Rect button{col.x, bottom - btnH, col.w, btnH};
    drawActionButton(p, t, button, s.startLabel.str(), s.startReady ? ButtonState::Ready : ButtonState::Unavailable,
                     cursor_ == 0);
    const int limit = button.y - gap;

    // ---- From the top: title, facts, versions or overview.
    int y = col.y;
    const int headingStep = p.lineHeight(FontRole::Bold, t.size(TextRole::Heading)) + t.px(Space::lineExtra);
    y += drawTextBlock(p, t, {col.x, y, col.w, headingStep * 2}, s.title, TextRole::Heading, TextTier::Primary, true, 2);
    y += t.px(Space::titleGap);

    std::vector<KeyValueEntry> facts;
    for (const DetailFact &f : s.facts) {
        if (f.value.empty() && f.valueText.empty()) continue;
        KeyValueEntry e;
        e.label = f.label;
        e.value = f.valueText.empty() ? Text::data(f.value) : f.valueText;
        facts.push_back(e);
    }
    if (!facts.empty()) y += drawKeyValueList(p, t, {col.x, y, col.w, std::max(0, limit - y)}, facts) + gap;

    int textLimit = limit;
    if (!s.warning.empty()) {   // the technical reason
        const int line = p.lineHeight(FontRole::Bold, t.size(TextRole::Body));
        drawStatus(p, t, col.x, limit - line, Tone::Warning, s.warning, TextRole::Body);
        textLimit = limit - line - gap;
    }

    if (!s.versions.empty()) {
        drawLabel(p, t, {col.x, y, col.w, smallH}, s.versionsLabel.str(), TextRole::Small, TextTier::Muted, Align::Left, true);
        y += smallH + t.px(8);
        const Density density = Density::Dense;
        const Rect rows{col.x, y, col.w, std::max(0, textLimit - y)};
        list_.configure({rows.x, rows.y, rows.w - scrollbarGutter(t), rows.h}, listItemFrameHeight(t, false, density),
                        listItemPitch(t, false, density));
        if (cursor_ >= 1 && cursor_ < kShotBase) list_.ensureVisible(cursor_ - 1);
        p.pushClip(rows.inset(-t.px(4)));
        const int first = list_.scroll();
        const int last = std::min(int(s.versions.size()), first + list_.visibleRows());
        for (int i = first; i < last; ++i) {
            ListItemSpec item;
            item.kind = ListItemKind::Value;   // the chosen version says "Selected"; the others say nothing
            item.title = Text::data(s.versions[size_t(i)]);
            item.value = i == selected_ ? s.selectedWord : Text::data(std::string());
            ListItemState st;
            st.cursor = cursor_ == i + 1;
            drawListItem(p, t, list_.row(i), item, st);
        }
        drawScrollbar(p, t, rows, int(s.versions.size()), list_.visibleRows(), list_.scroll());
        p.popClip();
    } else if (!s.overview.empty() && textLimit - y >= 2 * bodyH) {
        const int labelH = smallH + t.px(Space::lineExtra);
        if (!s.overviewLabel.empty()) {
            drawLabel(p, t, {col.x, y, col.w, labelH}, s.overviewLabel.str(), TextRole::Small, TextTier::Muted, Align::Left, true);
            y += labelH;
        }
        const int step = bodyH + t.px(Space::lineExtra);
        const int lines = std::max(1, (textLimit - y) / step);
        drawTextBlock(p, t, {col.x, y, std::min(col.w, t.px(900)), textLimit - y}, s.overview, TextRole::Body,
                      TextTier::Muted, false, lines);
    }
}

}  // namespace patsgui
