#include "app/GameOptionsView.h"

#include <algorithm>

#include "app/ModalInput.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

namespace {

using ui::Text;

}  // namespace

void GameOptionsView::registerStrings(ui::Catalog &c) {
    c.set("options.title", "Game options");
    c.set("options.versions", "Available ROMs");   // Classic: "AVAILABLE ROMS" (App::openGameActions); sentence case is the framework rule
    c.set("options.screenshots", "Screenshots");
    c.set("options.selected", "Selected");
    c.set("options.standard", "Standard");
}

void GameOptionsView::open(const GameRef &game) {
    if (game.empty()) return;
    game_ = game;
    detail_ = library_.detail(game);
    // Classic lists the versions by label; the game's own path is the one preselected.
    std::stable_sort(detail_.versions.begin(), detail_.versions.end(),
                     [](const GameVersion &a, const GameVersion &b) { return a.label < b.label; });
    view_.reset();
    for (size_t i = 0; i < detail_.versions.size(); ++i)
        if (detail_.versions[i].id == game.id) view_.setSelectedVersion(int(i));
    library_.gameScreenshots(game_);   // starts the online lookup for this game only
    open_ = true;
}

void GameOptionsView::close() {
    open_ = false;
    viewer_ = ui::ModalStack();
}

bool GameOptionsView::update(float) {
    if (open_ && viewer_.empty()) slot_.update(game_);
    return false;
}

DetailSubject GameOptionsView::subject() {
    DetailSubject s;
    const PreviewSlot::Picture pic = slot_.picture(game_, ArtSize::Detail, true, true);   // Classic: the Game options ignore "Show box art"
    s.picture = pic.image;
    s.pictureIsVideo = pic.video;
    s.pictureLoading = pic.loading;
    s.pictureLoadingText = Text::key("picture.loading");
    s.pictureNoneText = Text::key("picture.none");
    s.pictureStatus = slot_.status();
    s.title = detail_.info.title;
    s.versionsLabel = Text::key("options.versions");
    s.selectedWord = Text::key("options.selected");
    for (const GameVersion &v : detail_.versions)
        s.versions.push_back(v.label.empty() ? Text::key("options.standard").str() : v.label);
    if (s.versions.empty()) s.versions.push_back(Text::key("options.standard").str());   // one row, as in Classic
    s.startLabel = Text::key("hint.startGame");
    s.shotsLabel = Text::key("options.screenshots");
    const ScreenshotState shots = library_.gameScreenshots(game_);
    switch (shots.phase) {
        case ScreenshotState::Phase::Idle: s.shotsPhase = ShotsPhase::Missing; break;
        case ScreenshotState::Phase::Searching: s.shotsPhase = ShotsPhase::Searching; break;
        case ScreenshotState::Phase::Ready: s.shotsPhase = ShotsPhase::Ready; break;
        case ScreenshotState::Phase::Missing: s.shotsPhase = ShotsPhase::Missing; break;
    }
    s.shotCount = shots.count;
    s.shot = [this](int index, ArtSize size) { return library_.gameScreenshot(game_, index, size, true); };
    return s;
}

void GameOptionsView::start() {
    const std::string version = detail_.versions.empty() ? std::string() : detail_.versions[size_t(view_.selectedVersion())].id;
    library_.launch(game_, version);   // on failure the host shows the error and the panel stays
}

void GameOptionsView::openViewer() {
    std::vector<ui::ImagePtr> images;
    int first = 0;
    const int count = library_.gameScreenshots(game_).count;
    const int focus = view_.shotFocus();
    for (int i = 0; i < count; ++i) {
        const Art art = library_.gameScreenshot(game_, i, ArtSize::Detail, true);
        if (!art.image) continue;
        if (i < focus) ++first;
        images.push_back(art.image);
    }
    if (images.empty()) return;
    viewer_.push(ui::Modal::viewer(images));
    for (int i = 0; i < first; ++i) viewer_.handle(ui::ModalEvent::Right);
}

bool GameOptionsView::handle(Event event) {
    if (!open_) return false;
    if (!viewer_.empty()) {
        ui::ModalEvent me;
        if (toModalEvent(event, me)) viewer_.handle(me);   // the viewer ignores Up and Down; the rest waits
        return true;
    }
    if (event == Event::Back) { close(); return true; }
    const DetailView::Result r = view_.handle(event, subject());
    if (r.action == DetailView::Action::Start) start();
    else if (r.action == DetailView::Action::ShowShot) openViewer();
    return true;   // tabs, Select and the rest wait until the panel is closed
}

std::vector<ui::Hint> GameOptionsView::hints() const {
    if (!viewer_.empty()) return viewer_.hints();
    std::vector<ui::Hint> o = view_.hints(DetailSubject());
    o.push_back(ui::hint(ui::HintKey::B, "hint.close", true));
    return o;
}

void GameOptionsView::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &all) {
    if (!open_) return;
    p.fillRect(all, ui::Color{0, 0, 0, uint8_t(ui::Alpha::scrimDialog)});
    const int usable = all.h - t.bottomBarHeight();
    const int W = std::min(t.px(1776), all.w - t.px(64)), H = std::min(t.px(800), usable - t.px(32));
    const ui::Rect panel{(all.w - W) / 2, std::max(t.px(16), (usable - H) / 2), W, H};
    ui::drawSurface(p, t, panel, ui::SurfaceRole::Raised, t.px(ui::Radius::panel));
    const ui::Rect in = panel.inset(t.px(ui::Space::pad));
    const int titleH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Heading));
    ui::drawLabel(p, t, {in.x, in.y, in.w, titleH}, Text::key("options.title").str(), ui::TextRole::Heading,
                  ui::TextTier::Primary, ui::Align::Left, true);
    const int top = in.y + titleH + t.px(ui::Space::titleGap);
    view_.draw(p, t, {in.x, top, in.w, std::max(0, in.bottom() - top)}, subject());
    // The video must fit the picture slot the view just drew.
    slot_.setSlotSize(view_.pictureRect().w, view_.pictureRect().h);
    slot_.noteSlot(view_.pictureRect());
    viewer_.draw(p, t, all);
}

}  // namespace patsgui
