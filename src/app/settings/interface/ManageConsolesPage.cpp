#include "app/settings/interface/ManageConsolesPage.h"

#include <algorithm>

#include "app/EmptyPages.h"
#include "app/settings/StandardPages.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace {

// Classic's list is at most 700 design pixels wide (SystemVisibilityScreen::render).
constexpr int kListWidth = 700;

class ManageConsolesPage : public SettingsPage {
public:
    explicit ManageConsolesPage(InterfaceService &service) : service_(service), systems_(service.systemsWithGames()) {}

    PagePresentation presentation() const override { return PagePresentation::Content; }

    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override {
        size_t hidden = 0;
        for (const InterfaceService::SystemRow &s : systems_) hidden += s.hidden ? 1 : 0;

        // The row, the title and the help all say "Consoles" (Classic's word); the list sits on the faint panel of the panes.
        const ui::Rect body = drawSettingsPageFrame(
            p, t, content, ui::Text::key("settings.interface.consoles.title"),
            ui::Text::data(ui::format("settings.interface.consoles.count",            // Classic: "%zu of %zu shown"
                                      {std::to_string(systems_.size() - hidden), std::to_string(systems_.size())})));

        if (systems_.empty()) {   // Classic: the centered text "No systems yet"
            ui::drawEmptyPage(p, t, body, emptyPage("settings.interface.consoles.empty.title",
                                                    "settings.interface.consoles.empty.why",
                                                    "settings.interface.consoles.empty.next"));
            return;
        }

        const ui::Density density = ui::Density::Dense;
        const ui::Rect listArea{body.x, body.y, std::min(body.w, t.px(kListWidth)), body.h};
        layout_.configure({listArea.x, listArea.y, listArea.w - ui::scrollbarGutter(t), listArea.h}, ui::listItemFrameHeight(t, false, density),
                          ui::listItemPitch(t, false, density));
        layout_.ensureVisible(cursor_);

        p.pushClip(listArea.inset(-t.px(4)));
        const int first = layout_.scroll();
        const int last = std::min(int(systems_.size()), first + layout_.visibleRows());
        for (int i = first; i < last; ++i) {
            ui::ListItemSpec spec;
            spec.kind = ui::ListItemKind::Switch;       // On = shown (Classic: the pill "Shown" / "Hidden")
            spec.title = ui::Text::data(systems_[size_t(i)].name);
            spec.checked = !systems_[size_t(i)].hidden;
            ui::ListItemState state;
            state.cursor = i == cursor_;
            ui::drawListItem(p, t, layout_.row(i), spec, state);
        }
        ui::drawScrollbar(p, t, listArea, int(systems_.size()), layout_.visibleRows(), layout_.scroll());
        p.popClip();
    }

    // Classic (SystemVisibilityScreen::handle): Up and Down move without wrapping, A toggles, B closes; nothing else
    // does anything (Left and Right are not page keys here).
    bool handle(Event event, PageStack &stack) override {
        const int count = int(systems_.size());
        switch (event) {
            case Event::Up: cursor_ = ui::ListLayout::move(cursor_, count, -1); return true;
            case Event::Down: cursor_ = ui::ListLayout::move(cursor_, count, 1); return true;
            case Event::Confirm:
                if (cursor_ >= 0 && cursor_ < count) {
                    InterfaceService::SystemRow &row = systems_[size_t(cursor_)];
                    service_.setSystemHidden(row.name, !row.hidden);   // HiddenSystems::toggle: saved at once
                    row.hidden = !row.hidden;
                }
                return true;
            case Event::Back: stack.pop(); return true;
            default: return true;   // locked, as Classic's screen owns every input while it is open
        }
    }

    // Classic: "A Show or hide this console   B Back"; now "A Select" (the closed vocabulary) and a Move hint (Hints.md: the D-pad is listed wherever it moves)
    std::vector<ui::Hint> hints() const override {
        std::vector<ui::Hint> h{ui::hint(ui::HintKey::UpDown, "hint.move")};
        if (!systems_.empty()) h.push_back(ui::hint(ui::HintKey::A, "hint.select", true));
        h.push_back(ui::hint(ui::HintKey::B, "hint.back", true));
        return h;
    }

private:
    InterfaceService &service_;
    std::vector<InterfaceService::SystemRow> systems_;
    int cursor_ = 0;
    ui::ListLayout layout_;
};

}  // namespace

std::unique_ptr<SettingsPage> makeManageConsolesPage(InterfaceService &service) {
    return std::unique_ptr<SettingsPage>(new ManageConsolesPage(service));
}

}  // namespace patsgui
