#include "ui-framework/kit/Primitives.h"


#include <algorithm>
#include <cmath>

namespace ui {

// ------------------------------------------------------------------ Icon

// The SVG icon behind a glyph id, or null for a glyph that is drawn directly.
static const char *svgName(IconId icon) {
    switch (icon) {
        case IconId::Check:   return "check-lg";
        case IconId::Cross:   return "x-lg";
        case IconId::ArrowUp: return "arrow-up";
        case IconId::Alert:   return "exclamation-circle";
        case IconId::Clock:   return "clock";
        case IconId::Network: return "ethernet";
        case IconId::Wifi:    return "wifi";
        case IconId::Search:  return "search";
        case IconId::Drive:   return "hdd";
        case IconId::Download: return "download";
        default:              return nullptr;
    }
}

void drawIcon(Painter &p, IconId icon, const Rect &box, Color color, int stroke) {
    if (const char *name = svgName(icon)) {
        const int size = std::min(box.w, box.h);
        const BootstrapIcon bootstrap(name, size, size);
        if (bootstrap.valid()) { bootstrap.drawIn(p, box, color); return; }
    }
    const int x = box.x, y = box.y, w = box.w, h = box.h;
    auto X = [&](float f) { return x + int(f * w); };
    auto Y = [&](float f) { return y + int(f * h); };
    switch (icon) {
        case IconId::None:
        case IconId::Search:   // SVG only (the glossary symbols of a centred page): nothing to draw without it
        case IconId::Drive:
        case IconId::Download:
            break;
        case IconId::Dot:
            p.fillRoundedRect({X(0.3f), Y(0.3f), w * 2 / 5, h * 2 / 5}, w / 5, color);
            break;
        case IconId::Check:
            p.drawLine(X(0.18f), Y(0.55f), X(0.40f), Y(0.76f), stroke, color);
            p.drawLine(X(0.40f), Y(0.76f), X(0.84f), Y(0.26f), stroke, color);
            break;
        case IconId::Cross:
            p.drawLine(X(0.24f), Y(0.24f), X(0.76f), Y(0.76f), stroke, color);
            p.drawLine(X(0.76f), Y(0.24f), X(0.24f), Y(0.76f), stroke, color);
            break;
        case IconId::ArrowUp:
            p.drawLine(X(0.5f), Y(0.82f), X(0.5f), Y(0.2f), stroke, color);
            p.drawLine(X(0.5f), Y(0.2f), X(0.24f), Y(0.46f), stroke, color);
            p.drawLine(X(0.5f), Y(0.2f), X(0.76f), Y(0.46f), stroke, color);
            break;
        case IconId::Alert:
            p.strokeRoundedRect({x + 1, y + 1, w - 2, h - 2}, w / 2, stroke, color);
            p.drawLine(X(0.5f), Y(0.26f), X(0.5f), Y(0.56f), stroke, color);
            p.fillRoundedRect({X(0.5f) - stroke / 2, Y(0.68f), std::max(2, stroke), std::max(2, stroke)}, stroke / 2, color);
            break;
        case IconId::Clock:
            p.strokeRoundedRect({x + 1, y + 1, w - 2, h - 2}, w / 2, stroke, color);
            p.drawLine(X(0.5f), Y(0.5f), X(0.5f), Y(0.24f), stroke, color);
            p.drawLine(X(0.5f), Y(0.5f), X(0.72f), Y(0.62f), stroke, color);
            break;
        case IconId::Wifi:
            // Fallback without the SVG: a dot with two arcs' chords above it.
            p.fillRoundedRect({X(0.5f) - stroke, Y(0.78f), 2 * stroke, 2 * stroke}, stroke, color);
            p.drawLine(X(0.28f), Y(0.56f), X(0.5f), Y(0.4f), stroke, color);
            p.drawLine(X(0.5f), Y(0.4f), X(0.72f), Y(0.56f), stroke, color);
            break;
        case IconId::Network:
            // Two linked boxes: a wired connection.
            p.strokeRoundedRect({X(0.08f), Y(0.12f), int(w * 0.36f), int(h * 0.30f)}, 2, stroke, color);
            p.strokeRoundedRect({X(0.56f), Y(0.58f), int(w * 0.36f), int(h * 0.30f)}, 2, stroke, color);
            p.drawLine(X(0.26f), Y(0.42f), X(0.26f), Y(0.73f), stroke, color);
            p.drawLine(X(0.26f), Y(0.73f), X(0.56f), Y(0.73f), stroke, color);
            break;
    }
}

// ------------------------------------------------------------------ Surface

void drawPage(Painter &p, const Theme &t, const Rect &area) {
    p.fillGradient(area, t.palette.background, t.palette.backgroundLo);
}

void drawSurface(Painter &p, const Theme &t, const Rect &r, SurfaceRole role, int radius) {
    switch (role) {
        case SurfaceRole::Card:   p.fillRoundedRect(r, radius, t.palette.surface); break;
        case SurfaceRole::Raised: p.fillRoundedRect(r, radius, t.palette.surfaceHi); break;
        case SurfaceRole::Faint:
            p.fillRoundedRect(r, radius, t.palette.surface.withAlpha(Alpha::surfaceFaint));
            break;
    }
}

// ------------------------------------------------------------------ Label

static FontRole fontFor(bool bold) { return bold ? FontRole::Bold : FontRole::Regular; }

int labelWidth(Painter &p, const Theme &t, const std::string &text, TextRole role, bool bold) {
    return p.measure(fontFor(bold), t.size(role), text);
}

void drawLabelColor(Painter &p, const Theme &t, const Rect &area, const std::string &text, TextRole role,
                    Color color, Align align, bool bold) {
    const int size = t.size(role);
    const std::string shown = p.elide(fontFor(bold), size, formatButtons(t.buttonLayout(), text), area.w);
    const int width = p.measure(fontFor(bold), size, shown);
    int x = area.x;
    if (align == Align::Center) x = area.x + (area.w - width) / 2;
    else if (align == Align::Right) x = area.right() - width;
    const int y = area.y + (area.h - p.lineHeight(fontFor(bold), size)) / 2;
    p.drawText(fontFor(bold), size, x, y, shown, color);
}

void drawLabel(Painter &p, const Theme &t, const Rect &area, const std::string &text, TextRole role,
               TextTier tier, Align align, bool bold) {
    drawLabelColor(p, t, area, text, role, t.text(tier), align, bold);
}

// ------------------------------------------------------------------ TextBlock

std::vector<std::string> wrapText(Painter &p, const Theme &t, const std::string &source, TextRole role,
                                  bool bold, int width) {
    const int size = t.size(role);
    const std::string text = formatButtons(t.buttonLayout(), source);   // button placeholders in the user's layout
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&]() {
        if (!line.empty()) lines.push_back(line);
        line.clear();
    };
    auto place = [&](const std::string &w) {
        const std::string candidate = line.empty() ? w : line + " " + w;
        if (p.measure(fontFor(bold), size, candidate) <= width || line.empty()) line = candidate;
        else { flush(); line = w; }
    };
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == ' ' || text[i] == '\n') {
            if (!word.empty()) place(word);
            word.clear();
            if (i < text.size() && text[i] == '\n') flush();
        } else {
            word += text[i];
        }
    }
    flush();
    return lines;
}

int drawTextBlock(Painter &p, const Theme &t, const Rect &area, const std::string &text, TextRole role,
                  TextTier tier, bool bold, int maxLines) {
    const int size = t.size(role);
    const int step = p.lineHeight(fontFor(bold), size) + t.px(Space::lineExtra);
    std::vector<std::string> lines = wrapText(p, t, text, role, bold, area.w);
    int y = area.y;
    int count = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (maxLines > 0 && int(i) >= maxLines) break;
        std::string shown = p.elide(fontFor(bold), size, lines[i], area.w);   // a too-long word is elided
        if (maxLines > 0 && int(i) == maxLines - 1 && i + 1 < lines.size())
            shown = p.elide(fontFor(bold), size, shown + " …", area.w);
        p.drawText(fontFor(bold), size, area.x, y, shown, t.text(tier));
        y += step;
        ++count;
    }
    return count * step;
}

// ------------------------------------------------------------------ Focus

void drawFocusFrame(Painter &p, const Theme &t, const Rect &r, int radius) {
    p.strokeRoundedRect(r, radius, std::max(1, t.px(Stroke::focus)), t.palette.focus);
}

void drawFocusMarker(Painter &p, const Theme &t, const Rect &row) {
    const int w = t.px(Space::marker);
    const int inset = std::max(1, t.px(2));
    p.fillRoundedRect({row.x + inset, row.y + inset, w, row.h - 2 * inset}, std::max(1, w / 2), t.palette.accent);
}

// ------------------------------------------------------------------ Checkbox

int checkboxSize(const Theme &t, bool twoLine) { return t.px(twoLine ? 24 : 20); }

void drawCheckbox(Painter &p, const Theme &t, const Rect &box, bool on) {
    p.strokeRoundedRect(box, box.w / 2, std::max(1, t.px(2)), t.palette.textMuted);
    if (on) {
        const int inset = box.w / 4;
        p.fillRoundedRect(box.inset(inset), (box.w - 2 * inset) / 2, t.palette.textPrimary);
    }
}

// ------------------------------------------------------------------ ProgressBar

void drawProgressBar(Painter &p, const Theme &t, const Rect &r, float fraction, bool onSurface) {
    const int radius = r.h / 2;
    p.fillRoundedRect(r, radius, onSurface ? t.palette.surfaceHi : t.palette.surface);
    fraction = std::max(0.0f, std::min(1.0f, fraction));
    const int filled = std::max(r.h, int(float(r.w) * fraction));
    p.fillRoundedRect({r.x, r.y, filled, r.h}, radius, t.palette.accent);
}

// ------------------------------------------------------------------ Status

IconId iconForTone(Tone tone) {
    switch (tone) {
        case Tone::Neutral: return IconId::Dot;
        case Tone::Done:    return IconId::Check;
        case Tone::Action:  return IconId::ArrowUp;
        case Tone::Warning: return IconId::Alert;
        case Tone::Error:   return IconId::Cross;
    }
    return IconId::Dot;
}

int statusWidth(Painter &p, const Theme &t, const std::string &word, TextRole role) {
    return t.px(20) + t.px(10) + p.measure(FontRole::Bold, t.size(role), word);
}

void drawStatus(Painter &p, const Theme &t, int x, int y, Tone tone, const std::string &word, TextRole role) {
    const int size = t.size(role);
    const int line = p.lineHeight(FontRole::Bold, size);
    const int icon = t.px(20);
    const Color color = t.tone(tone);
    drawIcon(p, iconForTone(tone), {x, y + (line - icon) / 2, icon, icon}, color, std::max(2, t.px(2)));
    p.drawText(FontRole::Bold, size, x + icon + t.px(10), y, word, color);
}

}  // namespace ui
