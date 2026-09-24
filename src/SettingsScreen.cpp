#include "SettingsScreen.h"

#include <cstdio>

#include "Canvas.h"
#include "GamesScreen.h"

SettingsScreen::SettingsScreen(Context &context, std::function<void()> onReload,
                               std::function<void()> onQuit,
                               std::function<void()> onBuildDatabase,
                               std::function<void()> onFetchArtwork,
                               std::function<void()> onManageSystems,
                               std::function<void()> onToggleGamesTab,
                               std::function<void()> onCycleDefaultView)
    : context_(context), onReload_(std::move(onReload)), onQuit_(std::move(onQuit)),
      onBuildDatabase_(std::move(onBuildDatabase)),
      onFetchArtwork_(std::move(onFetchArtwork)),
      onManageSystems_(std::move(onManageSystems)),
      onToggleGamesTab_(std::move(onToggleGamesTab)),
      onCycleDefaultView_(std::move(onCycleDefaultView)) {
    buildRows();
}

void SettingsScreen::buildRows() {
    rows_.clear();

    rows_.push_back({"Build game database", [] { return std::string("A"); },
                     [this] { onBuildDatabase_(); }});

    rows_.push_back({"Fetch box art", [] { return std::string("A"); },
                     [this] { onFetchArtwork_(); }});

    rows_.push_back({"Manage systems", [] { return std::string("A"); },
                     [this] { onManageSystems_(); }});

    rows_.push_back({"Show Games menu item",
                     [this] { return std::string(context_.preferences.showGamesTab() ? "On" : "Off"); },
                     [this] { onToggleGamesTab_(); }});

    rows_.push_back({"Default view",
                     [this] {
                         return std::string(
                             displayNameForGameView(gameViewFromName(context_.preferences.defaultView())));
                     },
                     [this] { onCycleDefaultView_(); }});

    rows_.push_back({"Reload library", [] { return std::string("A"); },
                     [this] {
                         // The only path that walks the game volume, and only on request.
                         context_.notify("Scanning directories - this puts load on the drive", 6.0f);
                         context_.library.setScanningAllowed(true);
                         onReload_();
                         context_.library.setScanningAllowed(false);
                         context_.notify("Library reloaded");
                     }});

    rows_.push_back({"Quit", [] { return std::string("A"); }, [this] { onQuit_(); }});
}

std::vector<SettingsScreen::Fact> SettingsScreen::facts() const {
    std::vector<Fact> out;
    char buffer[128];

    out.push_back({"Machine", ""});

    std::snprintf(buffer, sizeof(buffer), "%s, %d cores", system_.cpuModel().c_str(),
                  system_.cores());
    out.push_back({"Processor", buffer});

    if (system_.cpuBusy() >= 0.0f) {
        std::snprintf(buffer, sizeof(buffer), "%.0f%%  ·  load %.2f", system_.cpuBusy() * 100.0f,
                      system_.loadAverage());
        out.push_back({"In use", buffer});
    }

    if (system_.memoryTotalBytes()) {
        std::snprintf(buffer, sizeof(buffer), "%s of %s",
                      SystemInfo::formatBytes(system_.memoryUsedBytes()).c_str(),
                      SystemInfo::formatBytes(system_.memoryTotalBytes()).c_str());
        out.push_back({"Memory", buffer});
    }

    out.push_back({"Running for", SystemInfo::formatDuration(system_.uptimeSeconds())});

    out.push_back({"", ""});
    out.push_back({"Storage", ""});
    for (const SystemInfo::Volume &volume : system_.volumes()) {
        const uint64_t used = volume.totalBytes - volume.freeBytes;
        std::snprintf(buffer, sizeof(buffer), "%s of %s free",
                      SystemInfo::formatBytes(volume.freeBytes).c_str(),
                      SystemInfo::formatBytes(volume.totalBytes).c_str());
        (void)used;
        out.push_back({volume.path, buffer});
    }

    out.push_back({"", ""});
    out.push_back({"Library", ""});

    if (context_.library.usingDatabase()) {
        const GameDatabase &db = context_.library.database();
        std::snprintf(buffer, sizeof(buffer), "%zu games in %zu systems", db.totalGames(),
                      db.systems().size());
        out.push_back({"Database", buffer});

        for (const std::string &missing : db.missingRoots())
            out.push_back({"Drive missing", missing});
    } else {
        out.push_back({"Database", "not built"});
    }

    out.push_back({"Systems listed", std::to_string(context_.library.systems().size())});
    out.push_back({"Favorites", std::to_string(context_.favorites.size())});

    const size_t indexed = context_.library.index().size();
    if (indexed) out.push_back({"Console Mode index", std::to_string(indexed) + " games"});

    out.push_back({"", ""});
    out.push_back({"Display", ""});
    out.push_back({"Resolution", framebufferInfo_});

    Font &font = context_.theme.bold();
    if (!font.usingTrueType()) {
        out.push_back({"Font", "built-in typeface"});
    } else {
        const std::string &path = font.path();
        const size_t slash = path.find_last_of('/');
        out.push_back({"Font", slash == std::string::npos ? path : path.substr(slash + 1)});
    }

    out.push_back({"Images cached", std::to_string(context_.images.size())});
    return out;
}

void SettingsScreen::update(float deltaSeconds) {
    focus_ += (1.0f - focus_) * std::min(1.0f, deltaSeconds * 9.0f);
    system_.update(deltaSeconds);
}

void SettingsScreen::handle(Action action) {
    switch (action) {
    case Action::Up:
        if (cursor_ > 0) --cursor_;
        break;
    case Action::Down:
        if (cursor_ + 1 < int(rows_.size())) ++cursor_;
        break;
    case Action::Confirm:
        if (rows_[size_t(cursor_)].activate) rows_[size_t(cursor_)].activate();
        break;
    default:
        break;
    }
}

void SettingsScreen::renderActions(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    const int rowHeight = theme.px(64);

    for (size_t i = 0; i < rows_.size(); ++i) {
        const Row &row = rows_[i];
        const Rect frame{area.x, area.y + int(i) * rowHeight, area.w, rowHeight - theme.px(8)};
        if (frame.bottom() > area.bottom()) break;

        const bool active = int(i) == cursor_;
        canvas.fillRoundedRect(frame, theme.px(8),
                               active ? theme.surfaceHi : theme.surface.withAlpha(150));
        if (active) {
            canvas.fillRoundedRect({frame.x, frame.y, theme.px(4), frame.h}, theme.px(2),
                                   theme.accent);
        }

        const int textY = frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
        theme.regular().draw(canvas, frame.x + theme.px(22), textY, row.label, theme.sizeBody(),
                             active ? theme.textPrimary : theme.textMuted);

        const std::string value = row.value ? row.value() : std::string();
        if (!value.empty()) {
            const int width = theme.regular().measure(value, theme.sizeBody());
            theme.regular().draw(canvas, frame.right() - theme.px(22) - width, textY, value,
                                 theme.sizeBody(), theme.accent);
        }
    }
}

void SettingsScreen::renderInfo(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;

    canvas.fillRoundedRect(area, theme.radius(), theme.surface.withAlpha(110));

    const Rect body = area.inset(theme.px(26));
    int y = body.y;

    const int lineStep = theme.regular().lineHeight(theme.sizeBody()) + theme.px(9);
    const int headingStep = theme.bold().lineHeight(theme.sizeBody()) + theme.px(12);

    for (const Fact &fact : facts()) {
        if (y + lineStep > body.bottom()) break;

        if (fact.label.empty() && fact.value.empty()) {   // spacer between groups
            y += theme.px(12);
            continue;
        }

        if (fact.value.empty()) {                          // group heading
            theme.bold().draw(canvas, body.x, y, fact.label, theme.sizeBody(),
                              theme.textPrimary);
            y += headingStep;
            continue;
        }

        theme.regular().draw(canvas, body.x, y, fact.label, theme.sizeBody(),
                             theme.textMuted.withAlpha(170));

        const std::string value =
            theme.regular().elide(fact.value, theme.sizeBody(), body.w / 2);
        const int width = theme.regular().measure(value, theme.sizeBody());
        theme.regular().draw(canvas, body.right() - width, y, value, theme.sizeBody(),
                             theme.textPrimary);
        y += lineStep;
    }
}

void SettingsScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    theme.bold().draw(canvas, area.x, area.y, "Settings", theme.sizeHeading(),
                      theme.textPrimary);

    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    // Actions on the left, readings on the right. The left column only needs to be as wide
    // as its longest action, so the information pane gets the rest.
    const int actionsWidth = std::min(body.w * 42 / 100, theme.px(620));
    const Rect actions{body.x, body.y, actionsWidth, body.h};
    const Rect info{actions.right() + theme.gap(), body.y, body.w - actionsWidth - theme.gap(),
                    body.h};

    renderActions(canvas, actions);
    if (info.w > theme.px(200)) renderInfo(canvas, info);
}

std::string SettingsScreen::hints() const { return "A Select   LB/RB Tabs"; }
