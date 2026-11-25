#include "app/Tab.h"

#include "ui-framework/kit/Text.h"

namespace patsgui {

namespace {

struct Registry {
    TabFactory factories[int(Tab::Count)];
    StringsFn strings[int(Tab::Count)] = {};
};

// Function-local, so registration from other files' static initializers is safe in any order.
Registry &registry() {
    static Registry r;
    return r;
}

}  // namespace

TabRegistrar::TabRegistrar(Tab tab, TabFactory factory, StringsFn strings) {
    registry().factories[int(tab)] = std::move(factory);
    registry().strings[int(tab)] = strings;
}

ui::Hint holdFavoriteHint(bool isFavorite) {
    return ui::hint(ui::HintKey::X, isFavorite ? "hint.unfavorite" : "hint.favorite", false, true);
}

ui::Hint holdRemoveHint() {
    return ui::hint(ui::HintKey::B, "hint.removeRecent", false, true);
}

const TabFactory &tabFactory(Tab tab) { return registry().factories[int(tab)]; }

void registerTabStrings() {
    for (StringsFn fn : registry().strings)
        if (fn) fn(ui::Catalog::get());
}

}  // namespace patsgui
