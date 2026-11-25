#pragma once

#include <functional>

#include "Screen.h"
#include "ui-framework/kit/Theme.h"

// What every Settings adapter (src/legacy/bridge/settings/App<Category>Settings.*) is built from. Classic objects come from
// `context` (and `context.settingsHost` for what only App owns); the two callbacks reach the new GUI.
struct AdapterEnv {
    Context &context;
    // The framework theme of the running new GUI; nullptr before its first frame. A theme setter changes it
    // (setDarkMode, setColorTheme, setHighContrast, setTextScale, setHintScale) as Classic's setters change
    // Context::theme, then calls invalidate().
    std::function<ui::Theme *()> uiTheme;
    // Ask the new GUI to redraw (a value changed on its own).
    std::function<void()> invalidate;
};
