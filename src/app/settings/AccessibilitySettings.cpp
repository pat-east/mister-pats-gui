#include "app/settings/AccessibilitySettings.h"

#include "app/settings/StandardPages.h"

namespace patsgui {
namespace {

using namespace menu;

PATSGUI_OPTIONS(kSizes, {"settings.opt.standard", "Standard"}, {"settings.opt.larger", "Larger"},
     {"settings.opt.largest", "Largest"});

// The Classic rows of this category, in Classic's order (src/legacy/SettingsScreen.cpp buildRows()).
const MenuCategory &table() {
    static const MenuCategory category = [] {
        MenuCategory access{"accessibility", "Accessibility", Cond::Always, {}};
        access.rows = {
            switchedBy(status("accessibility.grotesk", "Space Grotesk font",
                              "Use the Space Grotesk typeface. If its files are missing, choose this row to download them.",
                              "Classic: toggles the font (On/Off), or downloads it when missing; value is the font status"),
                       Cond::FontAvailable, 1),
            cycle("accessibility.fontsize", "Font size",
                  "Change text size across the interface. The new size takes effect immediately.", kSizes, PATSGUI_COUNT(kSizes)),
            cycle("accessibility.hintsize", "Hint size",
                  "Change the size of the button hints at the bottom left: Standard, Larger or Largest. Long hints shrink a little if they would not fit.",
                  kSizes, PATSGUI_COUNT(kSizes)),
            toggle("accessibility.contrast", "High contrast",
                   "Black background, brighter text, a highlight color of your choice for the selection and progress, and white borders around the search bar and the Settings lists. With White as the highlight color the extra borders are left out.",
                   false),
            because(enabledWhen([] {
                MenuRow r = cycle("accessibility.contrastcolor", "High contrast color",
                                  "Choose the color High contrast uses for the selection outline, progress bars and highlighted values: White, Pink, Yellow, Cyan, Lime Green, Electric Blue, Orange or Lavender. Pick the one you see best. Turn on High contrast first.",
                                  nullptr, 0);
                r.source = OptionSource::HighlightColors;
                r.initial = 2;   // Yellow, the framework default
                return r;
            }(), Cond::HighContrastOn), "Turn on High contrast first."),
        };
        return access;
    }();
    return category;
}

void strings(ui::Catalog &c) {
    registerTable(c, table());
}

}  // namespace

AccessibilityService *AccessibilitySettings::service() const {
    return attached() ? usable(env().services.accessibilityService) : nullptr;
}

std::vector<VisibleRow> AccessibilitySettings::rows(const MenuState &state) const { return visibleRows(table(), state); }

namespace {

// Font size and Hint size cycle Standard, Larger, Largest = 100, 115, 130 percent (Classic: 100 -> 115 -> 130 -> 100).
constexpr int kPercents[3] = {100, 115, 130};

int indexOfPercent(int percent) { return percent == 130 ? 2 : percent == 115 ? 1 : 0; }   // anything else shows "Standard"

}  // namespace

// The real value of every switch and cycling row, as Classic's value lambdas read it from Preferences.
int AccessibilitySettings::current(const std::string &rowId) const {
    AccessibilityService *s = service();
    if (!s) return kNoValue;
    if (rowId == "accessibility.grotesk") return s->fontEnabled() ? 1 : 0;
    if (rowId == "accessibility.fontsize") return indexOfPercent(s->fontSizeScale());
    if (rowId == "accessibility.hintsize") return indexOfPercent(s->hintSizeScale());
    if (rowId == "accessibility.contrast") return s->highContrast() ? 1 : 0;
    if (rowId == "accessibility.contrastcolor") {
        const int index = s->highContrastColor();
        return index >= 0 && index < 8 ? index : 2;   // Theme::highContrastColorName: out of range reads as Yellow
    }
    return kNoValue;
}

// A on the Space Grotesk row while it is a status text (font missing, downloading, failed): App::activateSpaceGrotesk().
bool AccessibilitySettings::activate(const std::string &rowId, PageStack &) {
    AccessibilityService *s = service();
    if (!s || rowId != "accessibility.grotesk") return false;
    s->activateFont();
    return true;
}

// The tab has already stepped the value in memory; these are the same setters Classic's lambdas call.
void AccessibilitySettings::changed(const std::string &rowId, int value) {
    if (rowId == "accessibility.contrast") env().state.highContrast = value != 0;   // the color row follows at once
    AccessibilityService *s = service();
    if (!s) return;
    if (rowId == "accessibility.grotesk") s->activateFont();   // the row is a switch while the font is installed
    else if (rowId == "accessibility.fontsize") s->setFontSizeScale(kPercents[value < 0 ? 0 : value > 2 ? 2 : value]);
    else if (rowId == "accessibility.hintsize") s->setHintSizeScale(kPercents[value < 0 ? 0 : value > 2 ? 2 : value]);
    else if (rowId == "accessibility.contrast") s->setHighContrast(value != 0);
    else if (rowId == "accessibility.contrastcolor") s->setHighContrastColor(value);
}

// Every frame, as SettingsScreen::update() does for the High contrast color row (1037-1041) and as the row's value
// lambdas do at draw time: the live state goes into MenuState (the color row's condition, the font row's kind and text).
bool AccessibilitySettings::update(float deltaSeconds) {
    AccessibilityService *s = service();
    if (!s) return false;
    s->update(deltaSeconds);
    MenuState &state = env().state;
    bool changed = false;
    const bool contrast = s->highContrast();
    if (state.highContrast != contrast) { state.highContrast = contrast; changed = true; }
    // Classic draws a switch exactly when the status text is "On" or "Off" (SettingsScreen::renderOptions 2319).
    const std::string text = s->fontStatus();
    const bool isSwitch = text == "On" || text == "Off";
    if (state.fontAvailable != isSwitch) { state.fontAvailable = isSwitch; changed = true; }
    std::string &slot = state.status["accessibility.grotesk"];
    if (slot != text) { slot = text; changed = true; }
    std::string notice;
    float seconds = 0.0f;
    while (s->takeNotice(notice, seconds)) {
        env().pages.toast(notice, seconds);
        changed = true;
    }
    return changed;
}

PATSGUI_REGISTER_SETTINGS_CATEGORY(AccessibilitySettings, strings);

}  // namespace patsgui
