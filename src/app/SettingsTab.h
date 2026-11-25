#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/SettingsMenu.h"
#include "app/Tab.h"
#include "app/settings/SettingsCategory.h"
#include "app/settings/SettingsPage.h"
#include "app/settings/SettingsServices.h"
#include "ui-framework/kit/List.h"

namespace patsgui {

// The first-level seam that is left for a host without services (documented in src/app/README.md). Every callback is
// optional. A row is handled by its category first (src/app/settings/SettingsCategory.h); only what the category
// does not handle reaches these callbacks. Row ids are the ids of the category tables.
struct SettingsActions {
    // A on a row that does something beyond changing its value: an Action row, or a Value row that holds a
    // status (Time zone, TheGamesDB, ...). Later this opens the submenu or flow named in MenuRow::later.
    std::function<void(const std::string &rowId)> activate;
    // A switch was toggled (value 0/1) or a choice moved to option `value` (index in the row's option list).
    std::function<void(const std::string &rowId, int value)> changed;
};

// Settings tab: categories on the left, the rows of the focused category in the middle, what the focused
// row does on the right. It hosts the categories and the PageStack; it knows no row and no service.
class SettingsTab : public TabView {
public:
    explicit SettingsTab(Services &services);
    ~SettingsTab() override;

    bool update(float deltaSeconds) override;
    void onShow() override;
    void onHide() override;
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event) override;
    std::vector<ui::Hint> hints() const override;
    bool locked() const override;
    bool dialogOpen() const override;
    bool wantsAttention() const override;
    ui::Text footerNote() const override;

    void setActions(const SettingsActions &actions) { actions_ = actions; }
    // What the conditions and live values read. The host calls this when something changed.
    void setState(const MenuState &state);
    const MenuState &state() const { return state_; }
    // Host runner and tests.
    void focusRowsForTest(int category, int row) { catCursor_ = category; rowCursor_ = row; inRows_ = true; }
    // Host runner and tests: open a category's page by id (`--category <id> --page <pageId>`), list them
    // ("system/reboot"), or put the cursor on a category's rows.
    bool openPageForTest(const std::string &categoryId, const std::string &pageId);
    std::vector<std::string> pageIdsForTest() const;
    void focusCategoryForTest(const std::string &categoryId);
    PageStack &pages() { return pages_; }
    // Tests (src/app/tests): activate a row by its id as A on it would (opens its page or flow). False: no such category.
    bool activateRowForTest(const std::string &categoryId, const std::string &rowId);

private:
    void drawPanes(ui::Painter &p, const ui::Theme &t, const ui::Rect &content);
    std::vector<SettingsCategory *> categories() const;
    SettingsCategory *focusedCategory() const;
    std::vector<VisibleRow> rows() const;
    int memory(const MenuRow &row) const;
    ui::Text label(const VisibleRow &row) const;
    ui::Text valueOf(const VisibleRow &row) const;
    bool selectable(const std::vector<VisibleRow> &list, int index) const;
    int firstSelectable() const;
    void clampRow();
    void enterRows();
    void move(int delta);
    void activate();
    void activateRow(SettingsCategory &cat, const std::string &id);

    SettingsServices fallback_;       // all null: used when the host gives no services
    SettingsServices &services_;
    MenuState state_;
    PageStack pages_;
    SettingsEnv env_;
    std::vector<std::unique_ptr<SettingsCategory>> cats_;
    bool shown_ = false;
    SettingsActions actions_;
    std::map<std::string, int> memory_;
    int catCursor_ = 0;
    int rowCursor_ = 0;
    bool inRows_ = false;
    ui::ListLayout catLayout_, rowLayout_;
};

}  // namespace patsgui
