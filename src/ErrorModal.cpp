#include "ErrorModal.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "Canvas.h"
#include "Theme.h"

namespace ErrorModal {

namespace {

// Font has elide() for shortening one line, not wrapping several — the modal's message is
// free-form (a file path, a system name) and short enough that a plain greedy word-wrap is
// all this needs.
std::vector<std::string> wrap(Font &font, const std::string &text, int size, int maxWidth) {
    std::vector<std::string> lines;
    std::istringstream words(text);
    std::string word, line;

    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (font.measure(candidate, size) <= maxWidth || line.empty()) {
            line = candidate;
        } else {
            lines.push_back(line);
            line = word;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

} // namespace

void draw(Canvas &canvas, Theme &theme, const Rect &area, const std::string &title,
         const std::string &message) {
    canvas.fillRect(area, theme.shadow.withAlpha(150));

    const int panelWidth = std::min(theme.px(620), area.w - theme.px(80));
    const int textWidth = panelWidth - 2 * theme.px(36);

    const std::vector<std::string> lines = wrap(theme.regular(), message, theme.sizeBody(), textWidth);
    const int lineHeight = theme.regular().lineHeight(theme.sizeBody());

    const int panelHeight = theme.px(36) * 2 + theme.regular().lineHeight(theme.sizeHeading()) +
                            theme.px(20) + int(lines.size()) * lineHeight + theme.px(28) +
                            theme.regular().lineHeight(theme.sizeSmall());

    const Rect panel{area.x + (area.w - panelWidth) / 2, area.y + (area.h - panelHeight) / 2,
                     panelWidth, panelHeight};

    canvas.dropShadow(panel, theme.radius(), theme.px(24), theme.shadow.withAlpha(140));
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    canvas.strokeRoundedRect(panel, theme.radius(), theme.px(2), theme.warning.withAlpha(160));

    int y = panel.y + theme.px(36);
    theme.bold().draw(canvas, panel.x + theme.px(36), y, title, theme.sizeHeading(), theme.warning);
    y += theme.regular().lineHeight(theme.sizeHeading()) + theme.px(20);

    for (const std::string &line : lines) {
        theme.regular().draw(canvas, panel.x + theme.px(36), y, line, theme.sizeBody(),
                             theme.textPrimary);
        y += lineHeight;
    }

    y += theme.px(28);
    theme.regular().draw(canvas, panel.x + theme.px(36), y, "A / B  Dismiss", theme.sizeSmall(),
                         theme.textMuted);
}

} // namespace ErrorModal
