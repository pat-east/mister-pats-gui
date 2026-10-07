#include "ArcadeGamesScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Alphabet.h"
#include "Canvas.h"
#include "LoadingIndicator.h"

namespace {

struct Column {
    const char *title;
    int weight;  // out of 100, of the table's own width
};

// Sums to 100. Name gets the most room since it is what a person is actually scanning the
// list for; the two file-name columns come next since those are what tells someone which
// exact ROM zip or core file to go looking for.
const Column kColumns[] = {
    {"Works?", 6},   {"Name", 19},      {"Year", 5},     {"Manufacturer", 12},
    {"Category", 10}, {"Core?", 6},     {"Core file", 13}, {"ROM?", 6},
    {"CRC?", 6},      {"ROM file", 17},
};

constexpr size_t kColumnCount = sizeof(kColumns) / sizeof(kColumns[0]);

// A short, thick, anti-aliased line — stamped rather than swept with Bresenham, since a check
// or cross at table-row size is only a handful of pixels long either way and this keeps the
// edges soft without a general-purpose line rasteriser this project does not otherwise need.
void drawLine(Canvas &canvas, float x0, float y0, float x1, float y1, float thickness,
             Color color) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float length = std::sqrt(dx * dx + dy * dy);
    const int steps = std::max(1, int(length * 2.0f));
    const float half = thickness / 2.0f;

    for (int i = 0; i <= steps; ++i) {
        const float t = float(i) / float(steps);
        const float cx = x0 + dx * t;
        const float cy = y0 + dy * t;

        const int minX = int(std::floor(cx - half)), maxX = int(std::ceil(cx + half));
        const int minY = int(std::floor(cy - half)), maxY = int(std::ceil(cy + half));

        for (int py = minY; py <= maxY; ++py) {
            for (int px = minX; px <= maxX; ++px) {
                const float ddx = float(px) + 0.5f - cx, ddy = float(py) + 0.5f - cy;
                const float dist = std::sqrt(ddx * ddx + ddy * ddy);
                if (dist > half + 0.75f) continue;
                const float coverage = std::min(1.0f, std::max(0.0f, half + 0.5f - dist));
                canvas.blendPixel(px, py, color, uint8_t(coverage * 255));
            }
        }
    }
}

// Drawn from scratch rather than through the font — this project's own precedent for a small
// status glyph (the favourite star, see Tile.cpp) is plain ASCII, and a real check/cross glyph
// is not something either the bundled TrueType font or the built-in bitmap fallback is known
// to carry. A vector shape this small renders identically either way.
void drawCheck(Canvas &canvas, const Rect &cell, Color color) {
    const float s = float(std::min(cell.w, cell.h));
    const float cx = float(cell.x) + cell.w / 2.0f, cy = float(cell.y) + cell.h / 2.0f;
    const float thickness = std::max(2.0f, s * 0.11f);

    drawLine(canvas, cx - s * 0.26f, cy + s * 0.02f, cx - s * 0.06f, cy + s * 0.22f, thickness,
             color);
    drawLine(canvas, cx - s * 0.06f, cy + s * 0.22f, cx + s * 0.28f, cy - s * 0.24f, thickness,
             color);
}

void drawCross(Canvas &canvas, const Rect &cell, Color color) {
    const float s = float(std::min(cell.w, cell.h));
    const float cx = float(cell.x) + cell.w / 2.0f, cy = float(cell.y) + cell.h / 2.0f;
    const float thickness = std::max(2.0f, s * 0.11f);
    const float r = s * 0.24f;

    drawLine(canvas, cx - r, cy - r, cx + r, cy + r, thickness, color);
    drawLine(canvas, cx - r, cy + r, cx + r, cy - r, thickness, color);
}

} // namespace

ArcadeGamesScreen::ArcadeGamesScreen(Context &context, std::function<void()> onClose)
    : context_(context), onClose_(std::move(onClose)) {}

const char *ArcadeGamesScreen::filterLabel(Filter filter) {
    switch (filter) {
    case Filter::All: return "All";
    case Filter::Working: return "Might work";
    case Filter::Broken: return "Broken";
    default: return "";
    }
}

void ArcadeGamesScreen::refresh() {
    scan_.start();
    sorted_.clear();
    haveSorted_ = false;
    filter_ = Filter::All;
    visible_.clear();
    focus_.clear();
    cursor_ = 0;
    scroll_ = 0;
}

void ArcadeGamesScreen::ensureSorted() {
    if (haveSorted_ || !scan_.finished()) return;

    sorted_ = scan_.entries();
    std::sort(sorted_.begin(), sorted_.end(), [](const ArcadeEntry &a, const ArcadeEntry &b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    haveSorted_ = true;
    rebuildVisible();
}

void ArcadeGamesScreen::rebuildVisible() {
    visible_.clear();
    for (int i = 0; i < int(sorted_.size()); ++i) {
        const bool works = sorted_[size_t(i)].mightWork();
        if (filter_ == Filter::Working && !works) continue;
        if (filter_ == Filter::Broken && works) continue;
        visible_.push_back(i);
    }

    focus_.assign(visible_.size(), 0.0f);
    cursor_ = 0;
    scroll_ = 0;
}

void ArcadeGamesScreen::update(float deltaSeconds) {
    if (scan_.running()) scan_.step();
    ensureSorted();

    const float speed = std::min(1.0f, deltaSeconds * 9.0f);
    for (size_t i = 0; i < focus_.size(); ++i) {
        const float target = (int(i) == cursor_) ? 1.0f : 0.0f;
        focus_[i] += (target - focus_[i]) * speed;
    }
}

void ArcadeGamesScreen::jumpLetter(int direction) {
    if (visible_.empty()) return;

    const int count = int(visible_.size());
    const int target = Alphabet::jump(cursor_, count, direction, [this](int i) -> const std::string & {
        return sorted_[size_t(visible_[size_t(i)])].name;
    });
    if (target == cursor_) return;

    cursor_ = target;
    const char letter = Alphabet::initial(sorted_[size_t(visible_[size_t(cursor_)])].name);
    context_.notify(letter == '#' ? std::string("0–9") : std::string(1, letter), 1.2f);
}

void ArcadeGamesScreen::launch() {
    if (cursor_ < 0 || cursor_ >= int(visible_.size())) return;

    const ArcadeEntry &entry = sorted_[size_t(visible_[size_t(cursor_)])];
    if (context_.launcher.launchArcade(entry.mraPath, entry.name)) {
        context_.history.remember("Arcade", entry.mraPath, entry.name);
        context_.notify("Starting " + entry.name);
        context_.standDown = true;
    } else {
        context_.showError("Could not start " + entry.name, context_.launcher.lastError());
    }
}

void ArcadeGamesScreen::handle(Action action) {
    switch (action) {
    case Action::Up:
        if (cursor_ > 0) --cursor_;
        break;
    case Action::Down:
        if (cursor_ + 1 < int(visible_.size())) ++cursor_;
        break;
    case Action::JumpPrev:
        jumpLetter(-1);
        break;
    case Action::JumpNext:
        jumpLetter(1);
        break;
    case Action::Confirm:
        launch();
        break;
    case Action::CycleView:
        filter_ = Filter(( int(filter_) + 1) % int(Filter::kCount));
        rebuildVisible();
        context_.notify(std::string("Showing: ") + filterLabel(filter_), 1.6f);
        break;
    case Action::Back:
        onClose_();
        break;
    default:
        break;
    }
}

void ArcadeGamesScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    theme.bold().draw(canvas, area.x, area.y, "Arcade Games", theme.sizeHeading(),
                      theme.textPrimary);

    // Live for as long as anything has actually been checked yet — waiting for the whole scan
    // to finish before saying anything is exactly the "where did the numbers go" complaint a
    // diagnostic screen exists to avoid.
    const size_t checked = scan_.entries().size();
    if (checked) {
        size_t mightWork = 0;
        for (const ArcadeEntry &entry : scan_.entries())
            if (entry.mightWork()) ++mightWork;

        char summary[96];
        if (haveSorted_) {
            std::snprintf(summary, sizeof(summary), "%zu of %zu might work  ·  showing: %s",
                         mightWork, checked, filterLabel(filter_));
        } else {
            std::snprintf(summary, sizeof(summary), "%zu of %zu checked so far, %zu look ok",
                         checked, scan_.total(), mightWork);
        }
        const int width = theme.regular().measure(summary, theme.sizeBody());
        theme.regular().draw(canvas, area.right() - width, area.y + theme.px(10), summary,
                             theme.sizeBody(), theme.textMuted.withAlpha(160));
    }

    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    if (!scan_.finished()) {
        const Rect panel = body.inset(theme.px(20));
        LoadingIndicator::draw(canvas, theme, panel, "Checking arcade games", scan_.progress(),
                               scan_.statusLine(), std::string());
        return;
    }

    if (sorted_.empty()) {
        theme.regular().drawCentered(canvas, body, "No .mra files found under _Arcade",
                                     theme.sizeHeading(), theme.textMuted);
        return;
    }

    if (visible_.empty()) {
        theme.regular().drawCentered(canvas, body,
                                     std::string("Nothing in \"") + filterLabel(filter_) + "\"",
                                     theme.sizeHeading(), theme.textMuted);
        return;
    }

    renderTable(canvas, body);
}

void ArcadeGamesScreen::renderTable(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;

    std::vector<int> columnX;
    std::vector<int> columnW;
    int x = area.x;
    for (const Column &column : kColumns) {
        columnX.push_back(x);
        const int w = area.w * column.weight / 100;
        columnW.push_back(w);
        x += w;
    }

    const int headerRowHeight = theme.px(36);
    const Rect headerRow{area.x, area.y, area.w, headerRowHeight};
    canvas.fillRoundedRect(headerRow, theme.px(4), theme.surface.withAlpha(180));
    for (size_t c = 0; c < kColumnCount; ++c) {
        const int textY = headerRow.y + (headerRow.h - theme.bold().lineHeight(theme.sizeSmall())) / 2;
        theme.bold().draw(canvas, columnX[c] + theme.px(10), textY, kColumns[c].title,
                          theme.sizeSmall(), theme.textMuted);
    }

    const Rect list{area.x, area.y + headerRowHeight + theme.px(4), area.w,
                    area.h - headerRowHeight - theme.px(4)};
    const int rowHeight = theme.px(38);
    const int visibleRows = std::max(1, list.h / rowHeight);

    if (cursor_ < scroll_) scroll_ = cursor_;
    if (cursor_ >= scroll_ + visibleRows) scroll_ = cursor_ - visibleRows + 1;
    const int maxScroll = std::max(0, int(visible_.size()) - visibleRows);
    scroll_ = std::min(std::max(0, scroll_), maxScroll);

    const int size = theme.sizeSmall();

    canvas.pushClip(list);
    for (int row = scroll_; row < std::min(int(visible_.size()), scroll_ + visibleRows); ++row) {
        const ArcadeEntry &entry = sorted_[size_t(visible_[size_t(row)])];
        const Rect rowArea{list.x, list.y + (row - scroll_) * rowHeight, list.w,
                           rowHeight - theme.px(4)};
        const float focus = focus_[size_t(row)];

        if (focus > 0.01f) {
            canvas.fillRoundedRect(rowArea, theme.px(6),
                                   Color::lerp(theme.surface, theme.surfaceHi, focus)
                                       .withAlpha(uint8_t(220 * focus)));
            canvas.fillRoundedRect({rowArea.x, rowArea.y, theme.px(3), rowArea.h}, theme.px(1),
                                   theme.accent.withAlpha(uint8_t(255 * focus)));
        } else if (row % 2 == 1) {
            canvas.fillRoundedRect(rowArea, theme.px(6), theme.surface.withAlpha(60));
        }

        const int textY = rowArea.y + (rowArea.h - theme.regular().lineHeight(size)) / 2;

        auto drawFlag = [&](size_t col, bool value) {
            const Rect cell{columnX[col], rowArea.y, columnW[col], rowArea.h};
            if (value) drawCheck(canvas, cell, theme.accent);
            else drawCross(canvas, cell, theme.warning);
        };
        auto drawText = [&](size_t col, const std::string &text, Color color) {
            const int maxWidth = columnW[col] - theme.px(16);
            const std::string elided = theme.regular().elide(text, size, maxWidth);
            theme.regular().draw(canvas, columnX[col] + theme.px(10), textY, elided, size, color);
        };

        drawFlag(0, entry.mightWork());
        drawText(1, entry.name, focus > 0.01f ? theme.textPrimary : theme.textMuted.withAlpha(230));
        drawText(2, entry.year, theme.textMuted);
        drawText(3, entry.manufacturer, theme.textMuted);
        drawText(4, entry.category, theme.textMuted);
        drawFlag(5, entry.coreFound);
        drawText(6, entry.coreFile, theme.textMuted.withAlpha(190));
        drawFlag(7, entry.romFound);
        drawFlag(8, entry.romCrcOk);
        drawText(9, entry.romName, theme.textMuted.withAlpha(190));
    }
    canvas.popClip();
}

std::string ArcadeGamesScreen::hints() const {
    if (!scan_.finished()) return "B Back";

    char buffer[112];
    std::snprintf(buffer, sizeof(buffer),
                 "A Start   Up/Down Row   L2/R2 Letter   Y Filter: %s   B Back",
                 filterLabel(filter_));
    return buffer;
}
