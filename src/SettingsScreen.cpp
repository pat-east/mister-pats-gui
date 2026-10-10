#include "SettingsScreen.h"

#include <algorithm>
#include <cstdio>

#include "Canvas.h"
#include "GamesScreen.h"

SettingsScreen::SettingsScreen(Context &context, std::function<void()> onReload,
                               std::function<void()> onBuildDatabase,
                               std::function<void()> onFetchArtwork,
                               std::function<void()> onDownloadSystemIcons,
                               std::function<std::string()> systemIconDownloadStatus,
                               std::function<void()> onManageSystems,
                               std::function<void()> onManageControllers,
                               std::function<void()> onManageArcade,
                               std::function<void()> onToggleGamesTab,
                               std::function<void()> onToggleArcadeTab,
                               std::function<void()> onCycleDefaultView,
                               std::function<void()> onStartMisterCore,
                               std::function<void()> onToggleAutomaticCheck,
                               std::function<void()> onCheckUpdates,
                               std::function<void()> onConfirmUpdate,
                               std::function<void()> onInstallUpdate,
                               std::function<void()> onCancelUpdate,
                               std::function<void()> onDismissInstallSuccess,
                               std::function<void()> onShowInstallSuccess,
                               std::function<void()> onRemovePinnedVersion,
                               std::function<void(bool)> onRestartAfterUpdate)
    : context_(context), onReload_(std::move(onReload)),
      onBuildDatabase_(std::move(onBuildDatabase)),
      onFetchArtwork_(std::move(onFetchArtwork)),
      onDownloadSystemIcons_(std::move(onDownloadSystemIcons)),
      systemIconDownloadStatus_(std::move(systemIconDownloadStatus)),
      onManageSystems_(std::move(onManageSystems)),
      onManageControllers_(std::move(onManageControllers)),
      onManageArcade_(std::move(onManageArcade)),
      onToggleGamesTab_(std::move(onToggleGamesTab)),
      onToggleArcadeTab_(std::move(onToggleArcadeTab)),
      onCycleDefaultView_(std::move(onCycleDefaultView)),
      onStartMisterCore_(std::move(onStartMisterCore)),
      onToggleAutomaticCheck_(std::move(onToggleAutomaticCheck)),
      onCheckUpdates_(std::move(onCheckUpdates)),
      onConfirmUpdate_(std::move(onConfirmUpdate)),
      onInstallUpdate_(std::move(onInstallUpdate)),
      onCancelUpdate_(std::move(onCancelUpdate)),
      onDismissInstallSuccess_(std::move(onDismissInstallSuccess)),
      onShowInstallSuccess_(std::move(onShowInstallSuccess)),
      onRemovePinnedVersion_(std::move(onRemovePinnedVersion)),
      onRestartAfterUpdate_(std::move(onRestartAfterUpdate)) {
    buildRows();
}

void SettingsScreen::buildRows() {
    categories_.clear();

    categories_.push_back({"Library", {}});
    categories_.push_back({"Controllers", {}});
    categories_.push_back({"Interface", {}});
    categories_.push_back({"Updates", {}});
    categories_.push_back({"System", {}});
    auto &library = categories_[0].rows;

    // Library and content — building/refreshing it, then managing what it shows. Grouped
    // together and in this order because that is the order they actually depend on each
    // other: a database has to exist before its art can be fetched/prepared or systems can be
    // hidden, and "Reload" only ever re-reads what the other two just produced.
    library.push_back({"Build game database", [] { return std::string("A"); },
                     [this] { onBuildDatabase_(); }});

    library.push_back({"Prepare box art", [] { return std::string("A"); },
                     [this] { onFetchArtwork_(); }});

    library.push_back({"Download System-Icons",
                     [this] { return systemIconDownloadStatus_(); },
                     [this] { onDownloadSystemIcons_(); }});

    library.push_back({"Reload library", [] { return std::string("A"); },
                     [this] {
                         // The only path that walks the game volume, and only on request.
                         context_.notify("Scanning directories - this puts load on the drive", 6.0f);
                         context_.library.setScanningAllowed(true);
                         onReload_();
                         context_.library.setScanningAllowed(false);
                         context_.notify("Library reloaded");
                     }});

    library.push_back({"Manage systems", [] { return std::string("A"); },
                     [this] { onManageSystems_(); }});

    library.push_back({"Manage Arcade", [] { return std::string("A"); },
                     [this] { onManageArcade_(); }});

    // Hardware.
    categories_[1].rows.push_back({"Controllers", [] { return std::string("A"); },
                     [this] { onManageControllers_(); }});

    // How the interface itself looks and navigates.
    auto &interface = categories_[2].rows;
    interface.push_back({"Show Games menu item",
                     [this] { return std::string(context_.preferences.showGamesTab() ? "On" : "Off"); },
                     [this] { onToggleGamesTab_(); }});

    interface.push_back({"Show Arcade menu item",
                     [this] { return std::string(context_.preferences.showArcadeTab() ? "On" : "Off"); },
                     [this] { onToggleArcadeTab_(); }});

    interface.push_back({"Show box art",
                     [this] { return std::string(context_.preferences.showBoxArt() ? "On" : "Off"); },
                     [this] {
                         context_.preferences.setShowBoxArt(!context_.preferences.showBoxArt());
                     }});

    interface.push_back({"Default view",
                     [this] {
                         return std::string(
                             displayNameForGameView(gameViewFromName(context_.preferences.defaultView())));
                     },
                     [this] { onCycleDefaultView_(); }});

    // This project itself.
    categories_[3].rows.push_back({"Check for updates on GitHub",
                     [this] { return std::string(context_.preferences.checkForUpdates() ? "On" : "Off"); },
                     [this] { onToggleAutomaticCheck_(); }});
    buildUpdateRows();

    categories_[4].rows.push_back({"Start MiSTer Core", [] { return std::string("A"); },
                     [this] { onStartMisterCore_(); }});
}

void SettingsScreen::buildUpdateRows() {
    auto &rows = categories_[3].rows;
    rows.resize(1);
    rows.push_back({"Running GUI", [this] {
        return std::string("v") + updateSnapshot_.runningVersion;
    }, {}, false});
    rows.push_back({"Startup selection", [this] {
        return updateSnapshot_.pinned
            ? std::string("Pinned v") + updateSnapshot_.selectedVersion
            : std::string("Newest v") + updateSnapshot_.selectedVersion;
    }, {}, false});
    if (updateSnapshot_.pinned &&
        updateSnapshot_.newestInstalledVersion != updateSnapshot_.selectedVersion)
        rows.push_back({"Newest installed", [this] {
            return std::string("v") + updateSnapshot_.newestInstalledVersion;
        }, {}, false});
    const UpdatePhase phase = updateSnapshot_.phase;
    if (updateSnapshot_.pinned) {
        const bool enabled = phase == UpdatePhase::Idle || phase == UpdatePhase::Current ||
                             phase == UpdatePhase::Available || phase == UpdatePhase::Blocked ||
                             phase == UpdatePhase::Error || phase == UpdatePhase::RestartRequired;
        rows.push_back({"Remove pinned version", [] { return std::string("A"); },
                        [this] { onRemovePinnedVersion_(); }, enabled});
    }
    if (phase == UpdatePhase::WaitingForInternet || phase == UpdatePhase::Checking) {
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
    } else if (phase == UpdatePhase::Downloading || phase == UpdatePhase::Verifying ||
               phase == UpdatePhase::Installing) {
        rows.push_back({"Update progress", [this] {
            const auto &s = updateSnapshot_;
            if (s.phase == UpdatePhase::Verifying) return std::string("Verifying…");
            if (s.phase == UpdatePhase::Installing) return std::string("Installing…");
            if (s.bytesTotal) return std::to_string(s.bytesDone * 100 / s.bytesTotal) + "%";
            return SystemInfo::formatBytes(s.bytesDone);
        }, {}, false});
        if (phase != UpdatePhase::Installing)
            rows.push_back({"B Cancel download", {}, {}, false});
    } else if (phase == UpdatePhase::RestartRequired) {
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
        if (!updateSnapshot_.successPromptVisible)
            rows.push_back({"Restart options", [] { return std::string("A"); },
                            [this] { onShowInstallSuccess_(); }});
    } else {
        rows.push_back({"Check now", [] { return std::string("A"); },
                        [this] { onCheckUpdates_(); }});
    }
    if (phase == UpdatePhase::Available && updateSnapshot_.canInstall)
        rows.push_back({"Update now — v" + updateSnapshot_.targetVersion + " available",
                        [] { return std::string("A"); },
                        [this] { onConfirmUpdate_(); }});
    if (phase == UpdatePhase::Blocked || phase == UpdatePhase::Error ||
        (phase == UpdatePhase::Available && !updateSnapshot_.canInstall))
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
    if (phase == UpdatePhase::Blocked)
        for (const std::string &path : updateSnapshot_.repairPaths)
            rows.push_back({path, {}, {}, false});
    if (phase == UpdatePhase::Current)
        rows.push_back({updateSnapshot_.statusText, {}, {}, false});
}

void SettingsScreen::setUpdateSnapshot(const UpdateSnapshot &snapshot) {
    const bool rebuild = snapshot.phase != updateSnapshot_.phase ||
                         snapshot.targetVersion != updateSnapshot_.targetVersion ||
                         snapshot.canInstall != updateSnapshot_.canInstall ||
                         snapshot.runningVersion != updateSnapshot_.runningVersion ||
                         snapshot.selectedVersion != updateSnapshot_.selectedVersion ||
                         snapshot.newestInstalledVersion != updateSnapshot_.newestInstalledVersion ||
                         snapshot.pinned != updateSnapshot_.pinned ||
                         snapshot.statusText != updateSnapshot_.statusText ||
                         snapshot.successPromptVisible != updateSnapshot_.successPromptVisible ||
                         snapshot.pinRemoved != updateSnapshot_.pinRemoved ||
                         snapshot.repairPaths != updateSnapshot_.repairPaths;
    updateSnapshot_ = snapshot;
    if (!rebuild || categories_.size() < 4) return;
    buildUpdateRows();
    if (categoryCursor_ == 3) {
        const int count = int(categories_[3].rows.size());
        if (!count) { optionCursor_ = 0; optionsFocused_ = false; }
        else optionCursor_ = std::max(0, std::min(optionCursor_, count - 1));
    }
    if (snapshot.phase == UpdatePhase::Confirming) notesScroll_ = 0;
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
    out.push_back({"Networking", ""});
    out.push_back({"Ethernet", system_.ethernetAddress()});
    out.push_back({"Wi-Fi", system_.wifiAddress()});

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
    system_.update(deltaSeconds);
}

void SettingsScreen::handle(Action action) {
    if (updateSnapshot_.phase == UpdatePhase::RestartRequired &&
        updateSnapshot_.successPromptVisible) {
        switch (action) {
        case Action::Confirm: onRestartAfterUpdate_(false); break;
        case Action::Back: onDismissInstallSuccess_(); break;
        case Action::FaceXPress:
            if (updateSnapshot_.pinned) onRestartAfterUpdate_(true);
            break;
        default: break;
        }
        return;
    }
    if (updateSnapshot_.phase == UpdatePhase::Confirming) {
        switch (action) {
        case Action::Confirm: onInstallUpdate_(); break;
        case Action::Back: onCancelUpdate_(); break;
        case Action::Up: notesScroll_ = std::max(0, notesScroll_ - 1); break;
        case Action::Down: ++notesScroll_; break;
        default: break;
        }
        return;
    }
    if (categories_.empty()) return;
    switch (action) {
    case Action::Up:
        if (optionsFocused_) {
            if (optionCursor_ > 0) --optionCursor_;
        } else if (categoryCursor_ > 0) {
            --categoryCursor_;
            optionCursor_ = std::min(optionCursor_, int(categories_[size_t(categoryCursor_)].rows.size()) - 1);
        }
        break;
    case Action::Down:
        if (optionsFocused_) {
            if (optionCursor_ + 1 < int(categories_[size_t(categoryCursor_)].rows.size())) ++optionCursor_;
        } else if (categoryCursor_ + 1 < int(categories_.size())) {
            ++categoryCursor_;
            optionCursor_ = std::min(optionCursor_, int(categories_[size_t(categoryCursor_)].rows.size()) - 1);
        }
        break;
    case Action::Left:
        optionsFocused_ = false;
        break;
    case Action::Right:
        optionsFocused_ = true;
        break;
    case Action::Back:
        optionsFocused_ = false;
        break;
    case Action::Confirm:
        if (!optionsFocused_) {
            optionsFocused_ = true;
        } else {
            const auto &rows = categories_[size_t(categoryCursor_)].rows;
            if (!rows.empty() && rows[size_t(optionCursor_)].enabled &&
                rows[size_t(optionCursor_)].activate) {
                const auto activate = rows[size_t(optionCursor_)].activate;
                activate();
            }
        }
        break;
    default:
        break;
    }
}

void SettingsScreen::renderCategories(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    const int rowHeight = std::min(theme.px(64), std::max(theme.px(48), area.h / 7));

    for (size_t i = 0; i < categories_.size(); ++i) {
        const Rect frame{area.x, area.y + int(i) * rowHeight, area.w, rowHeight - theme.px(8)};
        if (frame.bottom() > area.bottom()) break;

        const bool selected = int(i) == categoryCursor_;
        const bool active = selected && !optionsFocused_;
        canvas.fillRoundedRect(frame, theme.px(8),
                               active ? theme.surfaceHi :
                               selected ? theme.surface.withAlpha(190) : theme.surface.withAlpha(110));
        if (active) {
            canvas.fillRoundedRect({frame.x, frame.y, theme.px(4), frame.h}, theme.px(2),
                                   theme.accent);
        }

        const int textY = frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
        const int left = frame.x + theme.px(18);
        const int right = frame.right() - theme.px(12);
        const std::string label = theme.regular().elide(
            categories_[i].label, theme.sizeBody(), std::max(0, right - left));
        theme.regular().draw(canvas, left, textY, label, theme.sizeBody(),
                             selected ? theme.textPrimary : theme.textMuted);
    }
}

void SettingsScreen::renderOptions(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    const auto &rows = categories_[size_t(categoryCursor_)].rows;
    const int rowHeight = std::min(theme.px(64),
                                   std::max(theme.px(48), area.h / 7));
    const int visible = std::max(1, area.h / rowHeight);
    const int first = std::max(0, optionCursor_ - visible + 1);

    for (size_t i = size_t(first); i < rows.size(); ++i) {
        const Row &row = rows[i];
        const Rect frame{area.x, area.y + (int(i) - first) * rowHeight,
                         area.w, rowHeight - theme.px(8)};
        if (frame.bottom() > area.bottom()) break;

        const bool active = optionsFocused_ && int(i) == optionCursor_;
        if (int(i) == optionCursor_) {
            canvas.fillRoundedRect(frame, theme.px(8),
                                   active ? theme.surfaceHi : theme.surface.withAlpha(190));
        }
        if (active) {
            canvas.fillRoundedRect({frame.x, frame.y, theme.px(4), frame.h}, theme.px(2),
                                   theme.accent);
        }

        const int textY = frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
        const std::string value = row.value ? row.value() : std::string();
        const int left = frame.x + theme.px(18);
        const int right = frame.right() - theme.px(14);
        const int valueWidth = value.empty() ? 0 : theme.regular().measure(value, theme.sizeBody());
        const int labelWidth = std::max(0, right - left -
                                             (value.empty() ? 0 : valueWidth + theme.px(10)));
        const std::string label = theme.regular().elide(row.label, theme.sizeBody(), labelWidth);
        theme.regular().draw(canvas, left, textY, label, theme.sizeBody(),
                             row.enabled && (active || int(i) == optionCursor_)
                                 ? theme.textPrimary : theme.textMuted);

        if (!value.empty()) {
            theme.regular().draw(canvas, right - valueWidth, textY, value,
                                 theme.sizeBody(), theme.accent);
        }
    }
}

void SettingsScreen::renderInfo(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;

    canvas.fillRoundedRect(area, theme.radius(), theme.surface.withAlpha(110));

    const Rect body = area.inset(theme.px(24));
    int y = body.y;

    const int factSize = theme.sizeBody();
    const int lineStep = theme.regular().lineHeight(factSize) + theme.px(3);
    const int headingStep = theme.bold().lineHeight(theme.sizeBody()) + theme.px(8);
    const int columnGap = theme.px(16);
    const std::vector<Fact> allFacts = facts();
    int labelColumnWidth = 0;
    for (const Fact &fact : allFacts) {
        if (fact.label.empty() || fact.value.empty()) continue;
        labelColumnWidth = std::max(labelColumnWidth,
                                    theme.regular().measure(fact.label, factSize));
    }
    labelColumnWidth = std::min(labelColumnWidth, body.w * 38 / 100);

    for (const Fact &fact : allFacts) {
        if (y + lineStep > body.bottom()) break;

        if (fact.label.empty() && fact.value.empty()) {   // spacer between groups
            y += theme.px(6);
            continue;
        }

        if (fact.value.empty()) {                          // group heading
            theme.bold().draw(canvas, body.x, y, fact.label, theme.sizeBody(),
                              theme.textPrimary);
            y += headingStep;
            continue;
        }

        const std::string label =
            theme.regular().elide(fact.label, factSize, labelColumnWidth);
        theme.regular().draw(canvas, body.x, y, label, factSize,
                             theme.textMuted.withAlpha(170));

        const int valueX = body.x + labelColumnWidth + columnGap;
        const std::string value = theme.regular().elide(
            fact.value, factSize, std::max(0, body.right() - valueX));
        theme.regular().draw(canvas, valueX, y, value, factSize,
                             theme.textPrimary);
        y += lineStep;
    }
}

void SettingsScreen::renderUpdateConfirmation(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(205));
    const int width = std::min(area.w - theme.px(40), theme.px(850));
    const int height = std::min(area.h - theme.px(40), theme.px(600));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                     width, height};
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    const int margin = theme.px(28);
    const int left = panel.x + margin;
    const int right = panel.right() - margin;
    int y = panel.y + margin;
    theme.bold().draw(canvas, left, y, "Install v" + updateSnapshot_.targetVersion + "?",
                      theme.sizeHeading(), theme.textPrimary);
    y += theme.bold().lineHeight(theme.sizeHeading()) + theme.px(12);
    theme.regular().draw(canvas, left, y,
                         updateSnapshot_.pinned
                             ? "The version pin stays active after download."
                             : "Restart MiSTer after installation to use the update.",
                         theme.sizeBody(), theme.textPrimary);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(8);
    if (updateSnapshot_.databaseRebuildHint) {
        theme.regular().draw(canvas, left, y,
            theme.regular().elide("Game database format changed: rebuild it after restart.",
                                  theme.sizeBody(), right - left),
            theme.sizeBody(), theme.accent);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(8);
    }
    y += theme.px(10);
    theme.bold().draw(canvas, left, y, "RELEASE NOTES", theme.sizeSmall(), theme.textMuted);
    y += theme.bold().lineHeight(theme.sizeSmall()) + theme.px(10);
    const int lineHeight = theme.regular().lineHeight(theme.sizeBody()) + theme.px(3);
    std::vector<std::string> lines;
    std::string line;
    const std::string &notes = updateSnapshot_.releaseNotes;
    for (size_t pos = 0; pos < notes.size();) {
        const unsigned char first = (unsigned char)notes[pos];
        const size_t count = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
        const std::string glyph = notes.substr(pos, count);
        pos += count;
        if (glyph == "\n") { lines.push_back(line); line.clear(); continue; }
        std::string next = line + glyph;
        if (!line.empty() && theme.regular().measure(next, theme.sizeBody()) > right - left) {
            lines.push_back(line);
            line = glyph;
        } else line = next;
    }
    lines.push_back(line);
    const int maxLines = std::max(0, (panel.bottom() - margin - theme.px(48) - y) / lineHeight);
    notesScroll_ = std::min(notesScroll_, std::max(0, int(lines.size()) - maxLines));
    for (int i = notesScroll_; i < int(lines.size()) && i < notesScroll_ + maxLines; ++i) {
        theme.regular().draw(canvas, left, y, lines[size_t(i)], theme.sizeBody(), theme.textPrimary);
        y += lineHeight;
    }
    theme.regular().draw(canvas, left, panel.bottom() - margin - lineHeight,
        "A Install   B Cancel   ↑/↓ Scroll", theme.sizeBody(), theme.accent);
}

void SettingsScreen::renderUpdateSuccess(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    canvas.fillRect(area, theme.shadow.withAlpha(205));
    const int width = std::min(area.w - theme.px(40), theme.px(900));
    const int height = std::min(area.h - theme.px(40),
                                theme.px(updateSnapshot_.pinned ? 390 : 330));
    const Rect panel{area.x + (area.w - width) / 2, area.y + (area.h - height) / 2,
                     width, height};
    canvas.dropShadow(panel, theme.radius(), theme.px(24), theme.shadow.withAlpha(140));
    canvas.fillRoundedRect(panel, theme.radius(), theme.surfaceHi);
    canvas.strokeRoundedRect(panel, theme.radius(), theme.px(2), theme.accent.withAlpha(160));

    const int margin = theme.px(30);
    const int left = panel.x + margin;
    const int textWidth = panel.w - 2 * margin;
    int y = panel.y + margin;
    const std::string title = updateSnapshot_.pinRemoved
        ? "Version pin removed"
        : "v" + updateSnapshot_.targetVersion + " installed";
    theme.bold().draw(canvas, left, y, title,
                      theme.sizeHeading(), theme.textPrimary);
    y += theme.bold().lineHeight(theme.sizeHeading()) + theme.px(12);
    theme.regular().draw(canvas, left, y,
                         updateSnapshot_.pinRemoved
                             ? "The newest installed GUI is now selected for the next start."
                             : "The verified GUI library is ready. Choose what happens next.",
                         theme.sizeBody(), theme.textPrimary);
    y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(22);

    const std::string startVersion = updateSnapshot_.selectedVersion;
    const std::string runningVersion = updateSnapshot_.runningVersion;
    const auto line = [&](const std::string &label, const std::string &detail) {
        theme.bold().draw(canvas, left, y, label, theme.sizeBody(), theme.accent);
        y += theme.bold().lineHeight(theme.sizeBody()) + theme.px(3);
        theme.regular().draw(canvas, left + theme.px(20), y,
                             theme.regular().elide(detail, theme.sizeBody(),
                                                   textWidth - theme.px(20)),
                             theme.sizeBody(), theme.textPrimary);
        y += theme.regular().lineHeight(theme.sizeBody()) + theme.px(13);
    };
    line("A  Restart MiSTer", "Starts v" + startVersion +
         (updateSnapshot_.pinned ? " (version pin stays active)." : "."));
    line("B  Not now", "Continue running v" + runningVersion + ".");
    if (updateSnapshot_.pinned)
        line("X  Remove pin and restart MiSTer",
             "Starts the newest installed library, v" +
                 updateSnapshot_.newestInstalledVersion + ".");
}

void SettingsScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;

    const int headerHeight = theme.px(58);
    theme.bold().draw(canvas, area.x, area.y, "Settings", theme.sizeHeading(),
                      theme.textPrimary);

    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    const int gap = theme.gap();
    const int columnWidth = std::max(0, (body.w - 2 * gap) / 3);
    const Rect categories{body.x, body.y, columnWidth, body.h};
    const Rect options{categories.right() + gap, body.y, columnWidth, body.h};
    const Rect info{options.right() + gap, body.y, body.right() - options.right() - gap, body.h};
    canvas.fillRoundedRect(categories, theme.radius(), theme.surface.withAlpha(70));
    canvas.fillRoundedRect(options, theme.radius(), theme.surface.withAlpha(70));

    const int sectionHeight = theme.px(34);
    const int sectionTextY = body.y + (sectionHeight - theme.bold().lineHeight(theme.sizeSmall())) / 2;
    theme.bold().draw(canvas, categories.x + theme.px(10), sectionTextY, "CATEGORIES",
                      theme.sizeSmall(), theme.textMuted);
    theme.bold().draw(canvas, options.x + theme.px(10), sectionTextY,
                      categories_[size_t(categoryCursor_)].label, theme.sizeSmall(), theme.textMuted);
    theme.bold().draw(canvas, info.x + theme.px(24), sectionTextY, "SYSTEM",
                      theme.sizeSmall(), theme.textMuted);

    const Rect listArea{categories.x + theme.px(10), categories.y + sectionHeight,
                        categories.w - theme.px(20), categories.h - sectionHeight};
    const Rect optionArea{options.x + theme.px(10), options.y + sectionHeight,
                          options.w - theme.px(20), options.h - sectionHeight};
    renderCategories(canvas, listArea);
    renderOptions(canvas, optionArea);
    if (info.w > theme.px(200)) {
        renderInfo(canvas, {info.x, info.y + sectionHeight, info.w, info.h - sectionHeight});
    }
    if (updateSnapshot_.phase == UpdatePhase::Confirming)
        renderUpdateConfirmation(canvas, area);
    if (updateSnapshot_.phase == UpdatePhase::RestartRequired &&
        updateSnapshot_.successPromptVisible)
        renderUpdateSuccess(canvas, area);
}

std::string SettingsScreen::hints() const {
    if (updateSnapshot_.phase == UpdatePhase::Confirming)
        return "A Install   B Cancel   ↑/↓ Scroll";
    if (updateSnapshot_.phase == UpdatePhase::RestartRequired &&
        updateSnapshot_.successPromptVisible)
        return updateSnapshot_.pinned
            ? "A Restart   B Not now   X Remove pin + restart"
            : "A Restart   B Not now";
    if (updateSnapshot_.phase == UpdatePhase::Downloading ||
        updateSnapshot_.phase == UpdatePhase::Verifying)
        return "B Cancel download   ←/→ Columns   LB/RB Tabs";
    return "A Select   ←/→ Columns   B Back   LB/RB Tabs";
}
