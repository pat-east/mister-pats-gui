#include "app/settings/StandardPagesDemo.h"

#include <algorithm>
#include <memory>

#include "app/settings/StandardPages.h"
#include "app/settings/WizardFlowPage.h"

namespace patsgui {
namespace {

using ui::Text;

std::unique_ptr<SettingsPage> own(SettingsPage *p) { return std::unique_ptr<SettingsPage>(p); }

std::string longText() {
    std::string text = "# Version 0.9.0\n\n**New:** a text viewer for long content.\n\n";
    for (int i = 1; i <= 40; ++i)
        text += "- Example change number " + std::to_string(i) + " with a sentence long enough to wrap onto a second line when the page is narrow.\n";
    return text;
}

// A small wizard flow (WizardFlowPage): a text step, a single choice, work that runs, and one Result layout for the outcome.
// The second choice fails, to show "Try again".
std::unique_ptr<WizardFlowPage> makeDemoFlow(PageStack &pages) {
    enum { kIntro, kChoose, kBusy, kDone, kFailed };
    struct Data { int target = 0; float seconds = 0.0f; };
    std::shared_ptr<Data> data(new Data);
    std::shared_ptr<ui::ChooseList> list(new ui::ChooseList(ui::ChooseMode::Single));
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    WizardFlowPage *self = flow.get();
    (void)pages;

    std::unique_ptr<ui::FnPage> intro(new ui::FnPage(ui::WizardPage::Kind::Step, Text::data("Connect to a target")));
    intro->onDraw = [](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        std::vector<ui::WizardParagraph> paras(2);
        paras[0].text = Text::data("This flow asks for one choice and then does the work.");
        paras[1].text = Text::data("Nothing is changed before you start.");
        paras[1].tier = ui::TextTier::Muted;
        ui::drawWizardParagraphs(p, t, body, paras);
    };
    intro->onHints = []() { return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true)}; };
    intro->onEvent = [](ui::WizardEvent e, ui::Wizard &w) { if (e == ui::WizardEvent::Confirm) w.go(kChoose); };
    flow->add(kIntro, std::move(intro));

    std::unique_ptr<ui::FnPage> choose(new ui::FnPage(ui::WizardPage::Kind::Step, Text::data("Choose a target")));
    choose->onEnter = [list, data](ui::Wizard &) {
        std::vector<ui::ChooseItem> items(3);
        const char *names[3] = {"First target", "Second target (fails)", "Third target"};
        for (int i = 0; i < 3; ++i) items[size_t(i)].title = Text::data(names[i]);
        list->setItems(items);
        list->setCurrent(data->target);
    };
    choose->onDraw = [list](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) { list->draw(p, t, body); };
    choose->onHints = []() { return std::vector<ui::Hint>{ui::hint(ui::HintKey::UpDown, "hint.move"), ui::hint(ui::HintKey::A, "hint.choose", true)}; };
    choose->onEvent = [list, data](ui::WizardEvent e, ui::Wizard &w) {
        if (list->handle(e) != ui::ChooseOutcome::Chosen) return;
        data->target = list->cursor();
        w.go(kBusy);
    };
    flow->add(kChoose, std::move(choose));

    std::unique_ptr<ui::BusyPage> busy(new ui::BusyPage(Text::data("Connect to a target")));
    busy->status = Text::data("Connecting");
    busy->backTo = ui::kPrevious;
    ui::BusyPage *bp = busy.get();
    busy->onEnter = [data, bp](ui::Wizard &) { data->seconds = 0.0f; bp->fraction = 0.0f; };
    busy->onUpdate = [data, bp, self](float dt, ui::Wizard &w) {
        data->seconds += dt;
        bp->fraction = std::min(1.0f, data->seconds / 2.0f);
        if (data->seconds >= 2.0f) {
            if (data->target != 1) self->toast(Text::data("Connected"));
            w.go(data->target == 1 ? kFailed : kDone);
        }
        return true;
    };
    flow->add(kBusy, std::move(busy));

    std::unique_ptr<ui::ResultPage> done(new ui::ResultPage(Text::data("Connect to a target")));
    ui::WizardResult ok;
    ok.tone = ui::Tone::Done;
    ok.headline = Text::data("Connected");
    ok.changed = Text::data("The target is saved and used from now on.");
    done->set(ok);
    flow->add(kDone, std::move(done));

    std::unique_ptr<ui::ResultPage> failed(new ui::ResultPage(Text::data("Connect to a target")));
    ui::WizardResult bad;
    bad.tone = ui::Tone::Error;
    bad.headline = Text::data("Could not connect");
    bad.changed = Text::data("Nothing was changed.");
    bad.next = Text::data("Choose another target, or try again.");
    bad.details = Text::data("no answer after 10 seconds");
    bad.retry = kChoose;
    failed->set(bad);
    flow->add(kFailed, std::move(failed));
    return flow;
}

}  // namespace

std::vector<std::string> standardPageDemoIds() {
    return {"choice", "confirm", "result.done", "result.error", "progress", "text", "text.masked", "facts", "viewer",
            "viewer.loading", "viewer.error", "wizard"};
}

bool openStandardPageDemo(const std::string &id, PageStack &pages) {
    if (id == "choice") {
        ChoiceSpec spec;
        spec.title = Text::data("Choose a region");
        for (const char *name : {"Africa", "America", "Asia", "Europe", "Pacific"}) spec.items.push_back({Text::data(name), Text()});
        spec.current = 3;
        pages.push(own(new ChoicePage(spec)));
    } else if (id == "confirm") {
        ConfirmSpec spec;
        spec.title = Text::data("Install the package?");
        spec.body = Text::data("This downloads and verifies the official package and changes only what it owns.");
        spec.confirmHint = Text::data("Install package");
        pages.push(own(new ConfirmPage(spec)));
    } else if (id == "result.done") {
        ResultSpec spec;
        spec.kind = ResultSpec::Done;
        spec.title = Text::data("Wi-Fi saved");
        spec.status = Text::data("Saved");
        spec.body = Text::data("Restart MiSTer to connect automatically.");
        pages.push(own(new ResultPage(spec)));
    } else if (id == "result.error") {
        ResultSpec spec;
        spec.kind = ResultSpec::Error;
        spec.title = Text::data("Could not save the login");
        spec.body = Text::data("Nothing was changed. The settings file could not be written.");
        spec.whatToDo = Text::data("Check that the card is not full and try again.");
        spec.details = Text::data("write: No space left on device");
        spec.onRetry = [](PageStack &) {};
        pages.push(own(new ResultPage(spec)));
    } else if (id == "progress") {
        ProgressSpec spec;
        spec.title = Text::data("Installing updates");
        spec.status = Text::data("Downloading 3 of 12 files");
        spec.detail = Text::data("2 installed, 0 failed");
        spec.fraction = 0.25f;
        std::unique_ptr<ProgressPage> page(new ProgressPage(spec));
        page->onUpdate = [](ProgressPage &self, PageStack &) {   // a stand-in for polling a service
            static float f = 0.25f;
            f += 0.01f;
            self.setFraction(f > 1.0f ? 0.0f : f);
        };
        pages.push(std::move(page));
    } else if (id == "wizard") {
        WizardFlowPage::launch(pages, makeDemoFlow(pages), 0);
    } else if (id == "text" || id == "text.masked") {
        TextInputSpec spec;
        spec.entry.title = Text::data(id == "text" ? "Network name" : "Password");
        spec.entry.masked = id == "text.masked";
        spec.entry.value = id == "text" ? "my-network" : "";
        spec.validate = [](const std::string &v) { return v.empty() ? Text::data("Enter at least one character.") : Text(); };
        pages.push(own(new TextInputPage(spec)));
    } else if (id == "facts") {
        InfoSpec spec;
        spec.title = Text::data("System information");
        pages.push(InfoPage::facts(spec, []() {
            std::vector<ui::KeyValueEntry> e;
            ui::KeyValueEntry h;
            h.kind = ui::KeyValueKind::Heading;
            h.label = Text::data("Machine");
            e.push_back(h);
            ui::KeyValueEntry row;
            row.label = Text::data("Processor");
            row.value = Text::data("Example CPU, 2 cores");
            e.push_back(row);
            row.label = Text::data("Memory");
            row.value = Text::data("212 MB of 1024 MB");
            e.push_back(row);
            ui::KeyValueEntry ok = row;
            ok.label = Text::data("Disc package");
            ok.value = Text::data("v0.7.0 verified");
            ok.hasStatus = true;
            ok.tone = ui::Tone::Done;
            e.push_back(ok);
            return e;
        }, 2.0f));
    } else if (id == "viewer" || id == "viewer.loading" || id == "viewer.error") {
        InfoSpec spec;
        spec.title = Text::data("Changelog");
        spec.subtitle = Text::data("Full project changelog, fetched from GitHub");
        spec.actionHint = id == "viewer" ? Text::data("Install update") : Text();
        std::unique_ptr<InfoPage> page = InfoPage::viewer(spec);
        if (id == "viewer") page->setText(longText());
        else if (id == "viewer.loading") page->setLoading(Text::data("Loading the full changelog from GitHub"));
        else { page->setError(Text::data("Could not load the changelog: no connection"), true); page->onRetry = [](PageStack &) {}; }
        pages.push(std::move(page));
    } else {
        return false;
    }
    return true;
}

}  // namespace patsgui
