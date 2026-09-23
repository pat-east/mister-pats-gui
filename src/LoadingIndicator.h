#pragma once

#include <string>

#include "Geometry.h"

class Canvas;
class Theme;

// The progress panel shared by every screen that has to build something before it can show
// content — the game database scan, the box art scraper, and a big system (or the merged "All
// games" list) resolving its artwork. One look wherever a wait is unavoidable, rather than a
// different ad-hoc message per screen.
namespace LoadingIndicator {

// `area` is the block it draws into, left-aligned from its top corner — callers centre or
// place that block themselves. `fraction` is clamped to 0..1; pass 0 for a track with nothing
// filled yet rather than a negative number, since a bar that starts filled and jumps back to
// empty reads as something going wrong.
void draw(Canvas &canvas, Theme &theme, const Rect &area, const std::string &title,
          float fraction, const std::string &status, const std::string &counts);

} // namespace LoadingIndicator
