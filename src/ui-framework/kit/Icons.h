#pragma once

#include <string>

#include "ui-framework/core/Painter.h"

namespace ui {

// The icon set: SVG files (Bootstrap Icons, MIT) embedded by src/ui-framework/tools/embed-icons.py and rendered by the
// framework's SVG renderer (src/ui-framework/core/Svg.h) into cached coverage masks.
struct IconSource {
    const char *name;   // the file name without ".svg", e.g. "house"
    const char *svg;
};
extern const IconSource kIconSources[];
extern const int kIconSourceCount;

// The licence text of the icon set (MIT), embedded with the icons so it ships with every build.
extern const char kBootstrapIconsLicense[];

// The mask of a named icon fitted into width x height pixels (aspect kept); cached. Null when the name is
// unknown. Use BootstrapIcon (kit/Icon.h) to draw one.
ImagePtr iconMask(const std::string &name, int width, int height);

}  // namespace ui
