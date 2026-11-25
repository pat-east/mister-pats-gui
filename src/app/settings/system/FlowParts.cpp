#include <algorithm>

#include "app/settings/system/SystemFlows.h"

namespace patsgui {
namespace sysflow {

void drawHeadlineChoose(ui::Painter &p, const ui::Theme &t, const ui::Rect &body, const ui::Text &headline, ui::ChooseList &list,
                        const Line &line) {
    const ui::Rect col = ui::wizardTextColumn(t, body);
    const int statusH = ui::wizardStatusHeight(p, t);
    const int headH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Heading));
    const std::string question = headline.str();
    const int lines = std::max(1, std::min<int>(2, int(ui::wrapText(p, t, question, ui::TextRole::Heading, true, col.w).size())));
    const int questionH = lines * (headH + t.px(ui::Space::lineExtra));
    ui::drawTextBlock(p, t, {col.x, body.y, col.w, questionH}, question, ui::TextRole::Heading, ui::TextTier::Primary, true, 2);
    const int top = body.y + questionH + t.px(16);
    list.draw(p, t, {body.x, top, body.w, std::max(0, body.bottom() - statusH - t.px(8) - top)});
    ui::drawWizardStatus(p, t, {body.x, body.bottom() - statusH, body.w, statusH}, line.tone, line.word, line.text);
}

std::unique_ptr<ui::ResultPage> resultPage(const ui::Text &title, std::function<void(ui::WizardResult &)> fill,
                                           std::function<void()> onFinished) {
    std::unique_ptr<ui::ResultPage> page(new ui::ResultPage(title));
    ui::ResultPage *raw = page.get();
    page->onEnter = [raw, fill](ui::Wizard &) {
        ui::WizardResult r;
        fill(r);
        raw->set(r);
    };
    page->onFinished = std::move(onFinished);
    return page;
}

}  // namespace sysflow

bool openSystemFlowStepForTest(SystemService &service, PageStack &pages, const std::string &id,
                               const std::shared_ptr<bool> &backupReporting) {
    const size_t dot = id.find('.');
    const std::string flow = id.substr(0, dot), step = dot == std::string::npos ? std::string() : id.substr(dot + 1);
    if (flow == "timezone") return openTimeZoneStep(service, pages, step);
    if (flow == "backup") return openBackupStep(service, pages, step, backupReporting);
    if (flow == "icons") return openIconsStep(service, pages, step);
    if (flow == "discinstall") return openDiscInstallStep(service, pages, step);
    if (flow == "reboot") return openRebootStep(service, pages, step);
    return false;
}

void registerSystemFlowStrings(ui::Catalog &c) {
    c.set("settings.system.hint.install", "Install");
    c.set("settings.system.hint.restart", "Restart");
    registerTimeZoneStrings(c);
    registerBackupStrings(c);
    registerIconsStrings(c);
    registerDiscInstallStrings(c);
    registerRebootStrings(c);
}
}  // namespace patsgui
