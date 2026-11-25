#pragma once

#include <string>
#include <vector>

#include "ui-framework/core/Painter.h"
#include "ui-framework/kit/Icon.h"
#include "ui-framework/kit/Text.h"
#include "ui-framework/kit/Theme.h"

namespace ui {

// ---- Icon: a small closed set of glyphs; each renderer draws them itself (the CPU renderer with lines)
enum class IconId { None, Dot, Check, Cross, ArrowUp, Alert, Clock, Network, Wifi, Search, Drive, Download };
void drawIcon(Painter &p, IconId icon, const Rect &box, Color color, int stroke);

// ---- Surface: filled surface roles and the page background
enum class SurfaceRole { Card, Raised, Faint };
void drawPage(Painter &p, const Theme &t, const Rect &area);
void drawSurface(Painter &p, const Theme &t, const Rect &r, SurfaceRole role, int radius);

// ---- Label: one line of text in a role and a tier; elides, never clips or overlaps
enum class Align { Left, Center, Right };
void drawLabel(Painter &p, const Theme &t, const Rect &area, const std::string &text, TextRole role,
               TextTier tier, Align align = Align::Left, bool bold = false);
void drawLabelColor(Painter &p, const Theme &t, const Rect &area, const std::string &text, TextRole role,
                    Color color, Align align = Align::Left, bool bold = false);
int labelWidth(Painter &p, const Theme &t, const std::string &text, TextRole role, bool bold = false);

// ---- Reading width: running text stays in a comfortable line length (about 70 characters) however wide its page
//      is; lists and tables may use the whole width. Text blocks on wide pages use `readingWidth` for their width.
inline int readingWidth(const Theme &t) { return t.px(900); }
inline Rect readingColumn(const Theme &t, const Rect &area) {
    const int w = std::min(area.w, readingWidth(t));
    return {area.x + (area.w - w) / 2, area.y, w, area.h};   // centred: the same padding left and right; the text stays left-aligned
}

// ---- TextBlock: wrapped paragraph with the shared line step; returns the height it used
std::vector<std::string> wrapText(Painter &p, const Theme &t, const std::string &text, TextRole role,
                                  bool bold, int width);
int drawTextBlock(Painter &p, const Theme &t, const Rect &area, const std::string &text, TextRole role,
                  TextTier tier, bool bold = false, int maxLines = 0);

// ---- FocusFrame and FocusMarker: the one focus cue
void drawFocusFrame(Painter &p, const Theme &t, const Rect &r, int radius);
void drawFocusMarker(Painter &p, const Theme &t, const Rect &row);

// ---- Checkbox: round on/off control; the dot, not the color, carries the state
void drawCheckbox(Painter &p, const Theme &t, const Rect &box, bool on);
int checkboxSize(const Theme &t, bool twoLine);

// ---- ProgressBar: one calm pill
void drawProgressBar(Painter &p, const Theme &t, const Rect &r, float fraction, bool onSurface);

// ---- Status: icon + word, tone only colors it (never color alone)
IconId iconForTone(Tone tone);
int statusWidth(Painter &p, const Theme &t, const std::string &word, TextRole role);
void drawStatus(Painter &p, const Theme &t, int x, int y, Tone tone, const std::string &word, TextRole role);

}  // namespace ui
