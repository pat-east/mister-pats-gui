#pragma once

#include <string>

#include "ui-framework/render/soft/Canvas.h"

namespace ui {

// Host presenter: writes the canvas as a PNG file (tests, the demo's screenshots, documentation).
bool writePng(const Canvas &canvas, const std::string &path);

}  // namespace ui
