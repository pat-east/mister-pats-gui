#pragma once

// The two pad drawings of Classic's ControllersScreen, composed from framework primitives (rounded rectangles and
// labels, colors from the theme): the schematic with one part lit for the wizard's current step (renderPadDiagram) and the
// live view of everything the pad does for the input test (renderInputTest). Application-level: it knows controller parts,
// the framework stays generic. Button names follow the user's layout (Theme::buttonLayout).

#include "app/settings/ControllersService.h"
#include "app/settings/controllers/PadLogic.h"
#include "ui-framework/kit/Primitives.h"

namespace patsgui {
namespace controllers {

// The schematic pad for the wizard, centered in `area` (at most 560 design px wide), part `highlight` lit.
void drawWizardDiagram(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, PadHighlight highlight);

// The live pad of the input test in `area`: D-pad, face buttons, shoulders, triggers (bars where analog), Select/Start/Home,
// two sticks. The geometry shrinks to fit `area` when it is short (never grows).
void drawInputTestPad(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, const PadSnapshot &pad);

}  // namespace controllers
}  // namespace patsgui
