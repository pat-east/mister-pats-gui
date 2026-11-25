#pragma once

#include <string>
#include <vector>

#include "ui-framework/kit/Theme.h"

namespace ui {

// The contrast of every pair of colors the components draw, for the current palette (docs/ACCESSIBILITY.md,
// rule 1.1): 4.5 : 1 for text, 3 : 1 for large or bold text and for parts of the interface (focus, borders,
// outlines). The registry is the list of pairs; a test runs it over every theme, mode and highlight.
struct ContrastCheck {
    std::string name;      // "textMuted on surface"
    float ratio = 0.0f;
    float minimum = 4.5f;
    bool ok() const { return ratio >= minimum; }
};


std::vector<ContrastCheck> checkContrast(const Theme &theme);

}  // namespace ui
