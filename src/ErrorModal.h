#pragma once

#include <string>

class Canvas;
class Theme;
struct Rect;

// A small, reusable modal for a failure the user needs to actually notice and dismiss on
// purpose — a drive that went away mid-session, a launch that could not find its game, a
// scan or scrape that failed outright. Not for routine status; see Context::notify for the
// quiet bottom-bar line a passing "View: Grid" or "Library reloaded" belongs on instead.
//
// Stateless — App owns the current title/message and whether one is showing (see
// Context::showError), and calls draw() over whatever screen is underneath after that
// screen's own render, then routes Confirm/Back to dismiss it before anything else sees them.
namespace ErrorModal {

void draw(Canvas &canvas, Theme &theme, const Rect &area, const std::string &title,
         const std::string &message);

} // namespace ErrorModal
