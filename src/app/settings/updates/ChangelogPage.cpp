#include "app/settings/updates/ChangelogPage.h"

#include "app/settings/StandardPages.h"

namespace patsgui {
namespace {

class ChangelogPage : public SettingsPage {
public:
    explicit ChangelogPage(UpdatesService &service) : service_(service) {
        InfoSpec spec;
        spec.title = ui::Text::key("settings.updates.changelog.title");
        spec.subtitle = ui::Text::key("settings.updates.changelog.subtitle");
        page_ = InfoPage::viewer(spec);
        page_->onRetry = [this](PageStack &stack) {   // Classic: A on the error view -> onShowChangelog_()
            if (service_.startChangelog()) lastView_ = View::None;   // scroll back to the top once the new text is there
            else stack.toast(ui::Text::key("settings.updates.changelog.busy"));
        };
        sync();
    }

    PagePresentation presentation() const override { return PagePresentation::Content; }
    bool update(float dt, PageStack &stack) override {
        const bool moved = sync();
        return page_->update(dt, stack) || moved;
    }
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override { page_->draw(p, t, content); }
    bool handle(Event event, PageStack &stack) override { return page_->handle(event, stack); }
    std::vector<ui::Hint> hints() const override { return page_->hints(); }

private:
    enum class View { None, Loading, Error, Text };

    // Classic render order: loading, error, then the lines. The text is prepared again only when the generation
    // changes (setUpdateSnapshot), which also puts the scroll back to 0.
    bool sync() {
        const UpdateInfo info = service_.snapshot();
        View view = info.changelogLoading ? View::Loading : !info.changelogError.empty() ? View::Error : View::Text;
        if (view == lastView_ && !(view == View::Error && info.changelogError != lastError_) &&
            !(view == View::Text && info.changelogGeneration != lastGeneration_))
            return false;
        lastView_ = view;
        lastError_ = info.changelogError;
        lastGeneration_ = info.changelogGeneration;
        if (view == View::Loading) page_->setLoading(ui::Text::key("settings.updates.changelog.loading"));
        else if (view == View::Error)
            page_->setError(ui::Text::key("settings.updates.changelog.error"), ui::Text::data(info.changelogError), true);
        else page_->setText(info.changelog);
        return true;
    }

    UpdatesService &service_;
    std::unique_ptr<InfoPage> page_;
    View lastView_ = View::None;
    std::string lastError_;
    uint32_t lastGeneration_ = 0;
};

}  // namespace

std::unique_ptr<SettingsPage> makeChangelogPage(UpdatesService &service) {
    return std::unique_ptr<SettingsPage>(new ChangelogPage(service));
}

}  // namespace patsgui
