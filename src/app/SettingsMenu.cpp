#include "app/SettingsMenu.h"

#include <initializer_list>
#include <string>

#include "ui-framework/kit/Palettes.h"

namespace patsgui {
namespace {

bool phaseIs(const MenuState &s, std::initializer_list<UpdatePhase> phases) {
    for (UpdatePhase p : phases)
        if (s.phase == p) return true;
    return false;
}

}  // namespace

bool holds(Cond cond, const MenuState &s) {
    using P = UpdatePhase;
    switch (cond) {
        case Cond::Always: return true;
        case Cond::Never: return false;
        case Cond::DevBuild: return s.devBuild;
        case Cond::DiscDrive: return s.discDrive;
        case Cond::WifiAdapter: return s.wifiAdapter;
        case Cond::HighContrastOn: return s.highContrast;
        case Cond::Pinned: return s.pinned;
        case Cond::PinnedNewer: return s.pinned && s.newest != s.selected;
        case Cond::UseNewestAllowed:
            return phaseIs(s, {P::Idle, P::Current, P::Available, P::Blocked, P::Error, P::RestartRequired});
        case Cond::PhaseWaiting: return phaseIs(s, {P::WaitingForInternet, P::Checking});
        case Cond::PhaseBusy: return phaseIs(s, {P::Downloading, P::Verifying, P::Installing});
        case Cond::PhaseCancelable: return phaseIs(s, {P::Downloading, P::Verifying});
        case Cond::PhaseRestart: return s.phase == P::RestartRequired;
        case Cond::PhaseRestartNoPrompt: return s.phase == P::RestartRequired && !s.restartPromptVisible;
        case Cond::PhaseCheckable:
            return !phaseIs(s, {P::WaitingForInternet, P::Checking, P::Downloading, P::Verifying, P::Installing,
                                P::RestartRequired});
        case Cond::PhaseAvailInstall: return s.phase == P::Available && s.canInstall;
        case Cond::PhaseStatusLine:
            return phaseIs(s, {P::Blocked, P::Error}) || (s.phase == P::Available && !s.canInstall);
        case Cond::PhaseBlocked: return s.phase == P::Blocked;
        case Cond::PhaseCurrent: return s.phase == P::Current;
        case Cond::FontAvailable: return s.fontAvailable;
    }
    return true;
}

std::vector<VisibleRow> visibleRows(const MenuCategory &category, const MenuState &state) {
    std::vector<VisibleRow> out;
    for (const MenuRow &r : category.rows) {
        if (!holds(r.show, state)) continue;
        VisibleRow v;
        v.row = &r;
        v.kind = holds(r.switchWhen, state) ? RowKind::Switch : r.kind;
        v.enabled = holds(r.enabled, state);
        switch (r.dyn) {
            case Dyn::None: break;
            case Dyn::StatusText: v.label = state.statusText; break;
            case Dyn::InstallTarget:
                v.label = ui::format("settings.updates.installTarget", {state.target});
                break;
            case Dyn::RepairPaths:
                for (const std::string &path : state.repairPaths) {
                    v.label = path;
                    out.push_back(v);
                }
                continue;
        }
        out.push_back(v);
    }
    return out;
}

int optionCount(const MenuRow &row) {
    int count = 0;
    switch (row.source) {
        case OptionSource::Own: return row.optionCount;
        case OptionSource::ColorThemes: ui::builtinColorThemes(count); return count;
        case OptionSource::HighlightColors: ui::highlightColors(count); return count;
    }
    return 0;
}

void registerOptions(ui::Catalog &c, const MenuOption *options, int count) {
    for (int i = 0; i < count; ++i) c.set(options[i].key, options[i].english);
}

void registerTable(ui::Catalog &c, const MenuCategory &category) {
    c.set(std::string("settings.cat.") + category.id, category.label);
    for (const MenuRow &r : category.rows) {
        if (r.label) c.set(std::string("settings.") + r.id, r.label);
        if (r.help) c.set(std::string("settings.") + r.id + ".help", r.help);
        if (r.reason) c.set(std::string("settings.") + r.id + ".reason", r.reason);
        if (r.options) registerOptions(c, r.options, r.optionCount);
    }
}

}  // namespace patsgui
