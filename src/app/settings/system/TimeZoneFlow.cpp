// Time zone: Classic's two-step chooser (SettingsScreen::handleTimeZoneChooser, renderTimeZoneChooser) as a Wizard: one Medium
// frame, a page per step. Region -A-> City -A-> saved (a toast, the flow ends). B on the city list goes back to the regions, B on
// the regions closes. The city list can hold 400+ rows: the triggers jump to the next / previous first letter (the only addition,
// the list scrolls with a scroll bar as before). A zone that cannot be saved, or no list at all, is a Result page (Classic toasted).
//
//   Region -(a region without cities)-> stays, status line     Region -A-> City -A, saved-> close
//                                                                         \-A, not saved-> Failed -A-> City

#include "app/settings/SystemService.h"
#include "app/settings/system/SystemFlows.h"

namespace patsgui {
namespace {

using sysflow::Line;

enum { kRegion = 0, kCity, kFailed };

struct Flow {
    SystemService *svc = nullptr;
    std::vector<std::string> regions, cities;
    std::string region;       // the region whose cities are listed
    int regionCursor = -1;    // the row the region list keeps when B comes back
    bool noList = false;      // the zone list could not be read
    Line line;                // the status line of the region page
    ui::ChooseList regionList{ui::ChooseMode::Single};
    ui::ChooseList cityList{ui::ChooseMode::Single};
};
using FlowPtr = std::shared_ptr<Flow>;

ui::Text key(const char *k) { return ui::Text::key(k); }
ui::Text title() { return key("settings.system.zone.title"); }

std::string currentRegion(const std::string &zone) {
    const size_t slash = zone.find('/');
    return slash == std::string::npos ? std::string() : zone.substr(0, slash);
}

void addRegionPage(WizardFlowPage &flow, const FlowPtr &f) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, title()));
    page->onEnter = [f](ui::Wizard &) {
        f->regions = f->svc->timeZoneRegions();
        const std::string now = currentRegion(f->svc->timeZone());
        std::vector<ui::ChooseItem> items;
        int current = -1;
        for (size_t i = 0; i < f->regions.size(); ++i) {
            ui::ChooseItem it;
            it.title = ui::Text::data(f->regions[i]);
            items.push_back(it);
            if (!now.empty() && f->regions[i] == now) current = int(i);
        }
        f->regionList.setItems(items);
        if (f->regionCursor >= 0) f->regionList.setCursor(f->regionCursor);   // coming back by B keeps the row
        else if (current >= 0) f->regionList.setCurrent(current);             // Classic: the cursor starts on the current region
        f->line = Line();
    };
    page->onDraw = [f](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        sysflow::drawHeadlineChoose(p, t, body, key("settings.system.zone.region.headline"), f->regionList, f->line);
    };
    page->onHints = [] {
        return std::vector<ui::Hint>{ui::hint(ui::HintKey::UpDown, "hint.move"), ui::hint(ui::HintKey::A, "hint.open", true)};
    };
    page->onEvent = [f](ui::WizardEvent e, ui::Wizard &w) {
        if (f->regionList.handle(e) != ui::ChooseOutcome::Chosen) return;
        const int at = f->regionList.cursor();
        if (at < 0 || at >= int(f->regions.size())) return;
        f->regionCursor = at;
        f->region = f->regions[size_t(at)];
        f->cities = f->svc->timeZonesIn(f->region);
        if (f->cities.empty()) {   // Classic: a toast and the region list stays
            f->line = Line{ui::Tone::Warning, key("settings.system.zone.noCities.word"), key("settings.system.zone.noCities")};
            return;
        }
        w.go(kCity);
    };
    flow.add(kRegion, std::move(page));
}

void addCityPage(WizardFlowPage &flow, const FlowPtr &f, WizardFlowPage *self) {
    std::unique_ptr<ui::FnPage> page(new ui::FnPage(ui::WizardPage::Kind::Step, title()));
    f->cityList.setJumpByLetter(true);
    page->onEnter = [f](ui::Wizard &) {
        if (f->cities.empty()) f->cities = f->svc->timeZonesIn(f->region);   // opened directly (host runner)
        const std::string now = f->svc->timeZone();
        const size_t slash = now.find('/');
        std::vector<ui::ChooseItem> items;
        int current = -1;
        for (size_t i = 0; i < f->cities.size(); ++i) {
            ui::ChooseItem it;
            it.title = ui::Text::data(f->cities[i]);
            items.push_back(it);
            if (slash != std::string::npos && now.substr(0, slash) == f->region && f->cities[i] == now.substr(slash + 1)) current = int(i);
        }
        f->cityList.setItems(items);
        f->cityList.setCursor(0);
        if (current >= 0) f->cityList.setCurrent(current);   // the zone in use is marked and the cursor starts there
    };
    page->onDraw = [f](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
        sysflow::drawHeadlineChoose(p, t, body, ui::Text::data(ui::format("settings.system.zone.city.headline", {f->region})),
                                    f->cityList, Line());
    };
    page->onHints = [f] {
        std::vector<ui::Hint> h{ui::hint(ui::HintKey::UpDown, "hint.move")};
        if (f->cityList.items().size() > 12) h.push_back(ui::hint(ui::HintKey::Triggers, "hint.jump"));
        h.push_back(ui::hint(ui::HintKey::A, "hint.choose", true));
        return h;
    };
    page->onEvent = [f, self](ui::WizardEvent e, ui::Wizard &w) {
        if (f->cityList.handle(e) != ui::ChooseOutcome::Chosen) return;
        const int at = f->cityList.cursor();
        if (at < 0 || at >= int(f->cities.size())) return;
        const std::string chosen = f->region + "/" + f->cities[size_t(at)];
        if (f->svc->setTimeZone(chosen)) {   // Classic: the toast, both pages close
            self->toast(ui::Text::data(ui::format("settings.system.zone.set", {chosen})));
            w.close();
        } else {
            w.go(kFailed);
        }
    };
    flow.add(kCity, std::move(page));
}

void addFailedPage(WizardFlowPage &flow, const FlowPtr &f) {
    flow.add(kFailed, sysflow::resultPage(title(), [f](ui::WizardResult &r) {
        r.tone = ui::Tone::Error;
        if (f->noList) {
            r.headline = key("settings.system.zone.noRegions.headline");
            r.changed = key("settings.system.zone.noRegions.changed");
            r.next = key("settings.system.zone.noRegions.next");
        } else {
            r.headline = key("settings.system.zone.failed.headline");
            r.changed = key("settings.system.zone.failed.changed");
            r.next = key("settings.system.zone.failed.next");
            r.retry = kCity;
        }
    }));
}

FlowPtr newFlow(SystemService &service) {
    FlowPtr f = std::make_shared<Flow>();
    f->svc = &service;
    return f;
}

std::unique_ptr<WizardFlowPage> makeFlow(const FlowPtr &f) {
    std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));
    addRegionPage(*flow, f);
    addCityPage(*flow, f, flow.get());
    addFailedPage(*flow, f);
    return flow;
}

}  // namespace

void openTimeZoneFlow(SystemService &service, PageStack &pages) {
    FlowPtr f = newFlow(service);
    f->noList = service.timeZoneRegions().empty();
    WizardFlowPage::launch(pages, makeFlow(f), f->noList ? kFailed : kRegion);
}

// For the host runner (see SystemFlows.h): "timezone", "timezone.city" (Europe), "timezone.failed", "timezone.nolist".
bool openTimeZoneStep(SystemService &service, PageStack &pages, const std::string &step) {
    FlowPtr f = newFlow(service);
    ui::PageId first = kRegion;
    if (step == "city") { f->region = "Europe"; first = kCity; }
    else if (step == "failed") { f->region = "Europe"; first = kFailed; }
    else if (step == "nolist") { f->noList = true; first = kFailed; }
    else if (!step.empty()) return false;
    WizardFlowPage::launch(pages, makeFlow(f), first);
    return true;
}

void registerTimeZoneStrings(ui::Catalog &c) {
    c.set("settings.system.zone.title", "Time zone");
    c.set("settings.system.zone.region.headline", "Choose a region");
    c.set("settings.system.zone.city.headline", "Choose a city in {1}");
    c.set("settings.system.zone.noCities.word", "No cities");
    c.set("settings.system.zone.noCities", "This region has no cities. Choose another region.");
    c.set("settings.system.zone.set", "Clock time zone set to {1}");
    c.set("settings.system.zone.noRegions.headline", "Could not load the time zones");
    c.set("settings.system.zone.noRegions.changed", "Nothing was changed.");
    c.set("settings.system.zone.noRegions.next", "MiSTer's time-zone list could not be read. Try again later.");
    c.set("settings.system.zone.failed.headline", "Could not save the time zone");
    c.set("settings.system.zone.failed.changed", "The time zone was not changed.");
    c.set("settings.system.zone.failed.next", "Check that MiSTer can write its system settings, then try again.");
}

}  // namespace patsgui
