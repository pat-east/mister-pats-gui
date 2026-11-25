// Settings -> Library -> ScreenScraper.fr -> Scraped videos: the videos downloaded so far, one row per file with its
// size, the total on top, and the way to delete one (A) or all of them (Start). New in the new GUI (Classic has no
// such list); the files and their deletion are the ScreenScraper cache's (LibraryService::scrapedVideos()).

#include <algorithm>
#include <cstdio>

#include "app/EmptyPages.h"
#include "app/settings/StandardPages.h"
#include "app/settings/library/LibraryPages.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace library {
namespace {

// "6.1 MB", "1.4 GB": one decimal, binary units like the rest of the GUI's sizes.
std::string sizeText(long long bytes) {
    const double mb = double(bytes) / (1024.0 * 1024.0);
    char text[32];
    if (mb >= 1024.0) std::snprintf(text, sizeof(text), "%.1f GB", mb / 1024.0);
    else std::snprintf(text, sizeof(text), "%.1f MB", mb);
    return text;
}

class VideosPage : public SettingsPage {
public:
    explicit VideosPage(LibraryService *svc) : svc_(svc) { reload(); }

    PagePresentation presentation() const override { return PagePresentation::Content; }

    bool update(float, PageStack &) override {
        if (!reloadNeeded_) return false;
        reloadNeeded_ = false;
        reload();
        return true;
    }

    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override {
        ui::PageHeaderSpec header;
        header.title = ui::Text::key("settings.library.videos.title");
        header.info = ui::Text::data(ui::format("settings.library.videos.total", {std::to_string(videos_.size()), sizeText(total_)}));
        const ui::Rect body = ui::drawPageHeader(p, t, content, header);
        if (videos_.empty()) {
            ui::drawEmptyPage(p, t, body, emptyPage("settings.library.videos.empty.title", "settings.library.videos.empty.why",
                                                    "settings.library.videos.empty.next"));
            return;
        }
        const ui::Density density = ui::Density::Dense;
        const ui::Rect listArea = ui::readingColumn(t, body);
        layout_.configure({listArea.x, listArea.y, listArea.w - ui::scrollbarGutter(t), listArea.h},
                          ui::listItemFrameHeight(t, false, density), ui::listItemPitch(t, false, density));
        layout_.ensureVisible(cursor_);
        p.pushClip(listArea.inset(-t.px(4)));
        const int first = layout_.scroll();
        const int last = std::min(int(videos_.size()), first + layout_.visibleRows());
        for (int i = first; i < last; ++i) {
            const LibraryService::ScrapedVideo &v = videos_[size_t(i)];
            ui::ListItemSpec spec;
            spec.kind = ui::ListItemKind::Value;
            spec.title = ui::Text::data(v.title);
            spec.value = ui::Text::data(v.system.empty() ? sizeText(v.bytes) : v.system + "  \xC2\xB7  " + sizeText(v.bytes));
            ui::ListItemState state;
            state.cursor = i == cursor_;
            ui::drawListItem(p, t, layout_.row(i), spec, state);
        }
        ui::drawScrollbar(p, t, listArea, int(videos_.size()), layout_.visibleRows(), layout_.scroll());
        p.popClip();
    }

    bool handle(Event event, PageStack &stack) override {
        const int count = int(videos_.size());
        switch (event) {
            case Event::Up: cursor_ = ui::ListLayout::move(cursor_, count, -1); return true;
            case Event::Down: cursor_ = ui::ListLayout::move(cursor_, count, 1); return true;
            case Event::Confirm:
                if (cursor_ >= 0 && cursor_ < count) askDeleteOne(stack);
                return true;
            case Event::Start:
                if (count > 0) askDeleteAll(stack);
                return true;
            case Event::Back: stack.pop(); return true;
            default: return true;
        }
    }

    std::vector<ui::Hint> hints() const override {
        std::vector<ui::Hint> h;
        if (!videos_.empty()) {
            h.push_back(ui::hint(ui::HintKey::A, "settings.library.videos.delete", true));
            h.push_back(ui::hint(ui::HintKey::Start, "settings.library.videos.deleteAll", false));
        }
        h.push_back(ui::hint(ui::HintKey::B, "hint.back", true));
        return h;
    }

private:
    void reload() {
        videos_ = svc_->scrapedVideos();
        total_ = 0;
        for (const LibraryService::ScrapedVideo &v : videos_) total_ += v.bytes;
        cursor_ = videos_.empty() ? 0 : std::min(cursor_, int(videos_.size()) - 1);
    }

    void askDeleteOne(PageStack &stack) {
        const LibraryService::ScrapedVideo v = videos_[size_t(cursor_)];
        ConfirmSpec spec;
        spec.title = ui::Text::key("settings.library.videos.one.title");
        spec.body = ui::Text::data(ui::format("settings.library.videos.one.body", {v.title, sizeText(v.bytes)}));
        spec.confirmHint = ui::Text::key("settings.library.videos.delete");
        LibraryService *svc = svc_;
        bool *flag = &reloadNeeded_;
        spec.onConfirm = [svc, flag, v](PageStack &) { svc->deleteScrapedVideo(v.id); *flag = true; };
        stack.push(std::unique_ptr<SettingsPage>(new ConfirmPage(spec)));
    }

    void askDeleteAll(PageStack &stack) {
        ConfirmSpec spec;
        spec.title = ui::Text::key("settings.library.videos.all.title");
        spec.body = ui::Text::data(ui::format("settings.library.videos.all.body", {std::to_string(videos_.size()), sizeText(total_)}));
        spec.confirmHint = ui::Text::key("settings.library.videos.deleteAll");
        LibraryService *svc = svc_;
        bool *flag = &reloadNeeded_;
        spec.onConfirm = [svc, flag](PageStack &) { svc->deleteAllScrapedVideos(); *flag = true; };
        stack.push(std::unique_ptr<SettingsPage>(new ConfirmPage(spec)));
    }

    LibraryService *svc_;
    std::vector<LibraryService::ScrapedVideo> videos_;
    long long total_ = 0;
    int cursor_ = 0;
    bool reloadNeeded_ = false;
    ui::ListLayout layout_;
};

}  // namespace

void openScrapedVideos(LibraryService &svc, PageStack &pages) { pages.push(std::unique_ptr<SettingsPage>(new VideosPage(&svc))); }

void registerVideoStrings(ui::Catalog &c) {
    c.set("settings.library.videos.title", "Scraped videos");
    c.set("settings.library.videos.total", "{1} videos  \xC2\xB7  {2}");
    c.set("settings.library.videos.empty.title", "No videos yet");
    c.set("settings.library.videos.empty.why", "Videos are downloaded from ScreenScraper.fr when you stay on a game in the List view.");
    c.set("settings.library.videos.empty.next", "Turn on Video previews and open a game in the List view.");
    c.set("settings.library.videos.delete", "Delete video");
    c.set("settings.library.videos.deleteAll", "Delete all videos");
    c.set("settings.library.videos.one.title", "Delete this video?");
    c.set("settings.library.videos.one.body", "{1} ({2}) is removed from the card. It is downloaded again when you stay on the game.");
    c.set("settings.library.videos.all.title", "Delete all videos?");
    c.set("settings.library.videos.all.body", "{1} videos ({2}) are removed from the card. Each one is downloaded again when you stay on its game.");
}

}  // namespace library
}  // namespace patsgui
