#pragma once

#include "Screen.h"
#include "app/Look.h"

// Reads the Classic settings that decide how the new GUI looks (Preferences: text size, hint size, high contrast and
// its color, color theme, dark mode, face-button style; the Classic Theme's resolved font files) into the framework's LookSettings.
// One reader for every hosted framework screen (PatsGuiScreen, WireframesScreen). The color theme is mapped from
// Classic's theme index to the framework theme index (see the table in LookBridge.cpp).
patsgui::LookSettings classicLook(const Context &context);

// Classic color theme index -> index in the framework's themes (ui::builtinColorThemes): the same index, the framework
// carries Classic's 22 themes in Classic's order. -1 when the index is out of range (the look then shows theme 0).
int frameworkColorTheme(int classicIndex);
