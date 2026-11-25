#include "app/settings/SettingsCategory.h"

#include <algorithm>
#include <cstring>

#include "ui-framework/kit/Palettes.h"

namespace patsgui {
namespace {

struct Entry {
    SettingsCategoryFactory factory;
    SettingsStringsFn strings;
};

// Function-local, so registration from other files' static initializers is safe in any order.
std::vector<Entry> &registry() {
    static std::vector<Entry> entries;
    return entries;
}

// The first-level order of Classic's Settings screen. A category with another id goes last.
const char *const kOrder[] = {"library", "controllers", "interface", "accessibility", "updates", "system", "developer"};

int rankOf(const char *id) {
    for (int i = 0; i < int(sizeof(kOrder) / sizeof(kOrder[0])); ++i)
        if (std::strcmp(kOrder[i], id) == 0) return i;
    return int(sizeof(kOrder) / sizeof(kOrder[0]));
}

}  // namespace

SettingsCategoryRegistrar::SettingsCategoryRegistrar(SettingsCategoryFactory factory, SettingsStringsFn strings) {
    registry().push_back({std::move(factory), strings});
}

std::vector<std::unique_ptr<SettingsCategory>> createSettingsCategories() {
    std::vector<std::unique_ptr<SettingsCategory>> out;
    for (const Entry &e : registry()) out.push_back(e.factory());
    std::stable_sort(out.begin(), out.end(), [](const std::unique_ptr<SettingsCategory> &a,
                                                const std::unique_ptr<SettingsCategory> &b) {
        return rankOf(a->id()) < rankOf(b->id());
    });
    return out;
}

void registerSettingsCategoryStrings(ui::Catalog &c) {
    for (const Entry &e : registry())
        if (e.strings) e.strings(c);
}

// The generic value text of a row (what SettingsTab drew itself before the categories existed).
ui::Text SettingsCategory::valueOf(const VisibleRow &r, const MenuState &state, int current) const {
    const MenuRow &m = *r.row;
    if (!r.enabled && m.disabledValue) return ui::Text::key(m.disabledValue);
    if (r.kind != RowKind::Value) return ui::Text::data(std::string());
    if (m.source == OptionSource::ColorThemes || m.source == OptionSource::HighlightColors) {
        int count = 0;
        const int i = std::max(0, std::min(current, optionCount(m) - 1));
        if (m.source == OptionSource::ColorThemes) return ui::Text::data(ui::builtinColorThemes(count)[i].name);
        return ui::Text::data(ui::highlightColors(count)[i].name);
    }
    if (m.options) return ui::Text::key(m.options[std::max(0, std::min(current, m.optionCount - 1))].key);
    const auto it = state.status.find(m.id);
    if (it != state.status.end()) return ui::Text::data(it->second);
    return m.value ? ui::Text::key(m.value) : ui::Text::data(std::string());
}

}  // namespace patsgui
