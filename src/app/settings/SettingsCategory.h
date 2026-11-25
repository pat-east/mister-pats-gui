#pragma once

// A Settings category: its rows (the Classic rows with their conditions, kinds, values and enable rules), what
// happens when a row is activated or changed, how it polls its services, and the pages it can open. SettingsTab
// only hosts the categories: it knows no row and no service. The seven categories (Library, Controllers,
// Interface, Accessibility, Updates, System, Developer) each live in their own files and register themselves:
//
//   // src/app/settings/LibrarySettings.cpp
//   class LibrarySettings : public SettingsCategory { ... };
//   PATSGUI_REGISTER_SETTINGS_CATEGORY(LibrarySettings, registerLibraryStrings);
//
// Finishing or extending a category therefore touches only its own files (src/app/settings/<Category>Settings.*,
// src/app/settings/<Category>Service.h, src/app/settings/Sample<Category>Service.*, src/legacy/bridge/settings/App<Category>Settings.*
// and any files it adds under src/app/settings/<category>/). See src/app/settings/PORTING.md.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/Library.h"
#include "app/SettingsMenu.h"
#include "app/settings/SettingsPage.h"
#include "app/settings/SettingsServices.h"

namespace patsgui {

// What a category works with. Created and owned by SettingsTab; passed to attach() once.
struct SettingsEnv {
    SettingsServices &services;   // the seam; every member may be null (use usable())
    Library &library;             // the games library (Manage Consoles, Arcade game status)
    MenuState &state;             // shared state of the conditions (Cond::*); a category refreshes its own fields in update()
    PageStack &pages;             // open a page: env().pages.push(...)  (activate() also receives it)
    // Shell navigation hooks (empty = the host has none). Library: "Arcade game status" opens the Arcade tab's status
    // screen (Services::openArcadeStatus, set by the shell).
    std::function<void()> openArcadeStatus;
};

class SettingsCategory {
public:
    virtual ~SettingsCategory() = default;

    // "library", "controllers", "interface", "accessibility", "updates", "system", "developer". The label key is
    // settings.cat.<id>; the order of the categories is fixed (SettingsCategory.cpp).
    virtual const char *id() const = 0;

    void attach(SettingsEnv &env) { env_ = &env; }

    // False hides the whole category (Classic: Developer is always shown today).
    virtual bool shown(const MenuState &state) const { (void)state; return true; }

    // The rows with every condition applied, headings included (usually visibleRows(table(), state), plus the
    // category's own conditions). Called every frame: build from static tables and cached state, never from I/O.
    virtual std::vector<VisibleRow> rows(const MenuState &state) const = 0;

    // The text of a Value row. The default is the generic one: the option label, a theme or color name, the status
    // text kept in state.status[rowId], else the row's fixed value key. Override for live texts
    // ("Version running now"). `current` is the option index or switch value (see current()).
    virtual ui::Text valueOf(const VisibleRow &row, const MenuState &state, int current) const;

    // The real value of a Switch (0/1) or a cycling Value row (option index) from the service, or kNoValue when
    // there is no service: the tab then keeps the value in memory, as the first level does.
    static constexpr int kNoValue = -1;
    virtual int current(const std::string &rowId) const { (void)rowId; return kNoValue; }

    // A on an Action row, or on a Value row that has no option list (a status that opens something). Return true
    // when handled; false lets the SettingsActions seam run (the empty seam of the first level).
    virtual bool activate(const std::string &rowId, PageStack &pages) { (void)rowId; (void)pages; return false; }

    // A switch was toggled (value 0/1) or a choice moved to option `value`. The tab has already stored the value
    // in memory; write it through the service here (the same Preferences setters Classic calls).
    virtual void changed(const std::string &rowId, int value) { (void)rowId; (void)value; }

    // Once per frame while the Settings tab is shown (also while a page is open): poll the services, copy what
    // the conditions and values need into env().state (including state.status[rowId]). Returns true when
    // something changed and the screen needs a redraw. Never blocks; cheap when idle.
    virtual bool update(float deltaSeconds) { (void)deltaSeconds; return false; }

    // What Classic's App does on EVERY tab (App::update polls the update snapshot every frame): `background()` runs once
    // per frame while the Settings tab is NOT shown; `wantsAttention()` asks the shell to show the Settings tab (the
    // restart prompt after an install); `footerNote()` is a short text for the shell's footer ("v0.9.0 available";
    // empty = none). All three default to nothing.
    virtual bool background(float deltaSeconds) { (void)deltaSeconds; return false; }
    virtual bool wantsAttention() const { return false; }
    virtual ui::Text footerNote() const { return ui::Text(); }

    // The Settings tab became the active tab / is being left (refresh at once, stop what only runs while shown).
    virtual void onShow() {}
    virtual void onHide() {}

    // First-level overrides that apply wherever the cursor is (Classic: B cancels a running update download).
    // backActivates: a row id that B activates instead of going back (consuming Back); empty = no override.
    // hintsHook: fill `hints` and return true to replace the first-level help line.
    virtual std::string backActivates() const { return std::string(); }
    virtual bool hintsHook(std::vector<ui::Hint> &hints) const { (void)hints; return false; }

    // Pages the host runner can open directly for tests (`--category <id> --page <pageId>`), and the same code
    // path a row uses. The ids are short ("timezone", "wifi.networks").
    virtual std::vector<std::string> pageIds() const { return {}; }
    virtual bool openPage(const std::string &pageId, PageStack &pages) { (void)pageId; (void)pages; return false; }

protected:
    SettingsEnv &env() const { return *env_; }
    bool attached() const { return env_ != nullptr; }

private:
    SettingsEnv *env_ = nullptr;
};

using SettingsCategoryFactory = std::function<std::unique_ptr<SettingsCategory>()>;
using SettingsStringsFn = void (*)(ui::Catalog &);

// Registers the category before main(); use the macro in the category's .cpp. The strings function registers
// the category's labels, help texts, options and page texts (registerTable() / registerOptions()).
struct SettingsCategoryRegistrar {
    SettingsCategoryRegistrar(SettingsCategoryFactory factory, SettingsStringsFn strings);
};

#define PATSGUI_REGISTER_SETTINGS_CATEGORY(CategoryClass, stringsFn) \
    static ::patsgui::SettingsCategoryRegistrar patsguiSettingsRegistrar_##CategoryClass( \
        []() { return std::unique_ptr<::patsgui::SettingsCategory>(new CategoryClass()); }, stringsFn)

// One instance of every registered category, in the fixed order of the first level.
std::vector<std::unique_ptr<SettingsCategory>> createSettingsCategories();
// Calls the strings function of every registered category.
void registerSettingsCategoryStrings(ui::Catalog &c);

}  // namespace patsgui
