#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/Library.h"

namespace patsgui {

// Generic fake data with generated art, so tabs can be built and rendered on the host without a device.
// Titles are "Sample title 0001", systems "Example system A" and so on. Deterministic.
class SampleLibrary : public Library {
public:
    SampleLibrary();

    // Test hooks for rendering states: the disc in the drive.
    void setDisc(DiscStatus::Media media, DiscStatus::Console console = DiscStatus::Console::PlayStation);
    // Whether the Disc tab is in the tab row (the device: a drive and the Disc package are there). Default: yes.
    void setDiscTabAvailable(bool available) { discTab_ = available; }
    bool discTabAvailable() override { return discTab_; }
    void setFavorite(const GameRef &game, bool favorite) { if (favorite) favorites_.insert(key(game)); else favorites_.erase(key(game)); }

    bool standingDown() const override { return standDown_; }
    void update(float deltaSeconds) override;
    uint64_t revision() const override { return revision_; }
    void refresh() override {}
    bool isSample() const override { return true; }

    std::vector<SystemInfo> systems() override;
    void setSystemHidden(const SystemId &system, bool hidden) override;
    Art systemArt(const SystemId &system, ArtSize size) override;

    int gameCount(const SystemId &system, Works filter = Works::All) override;
    std::vector<GameInfo> games(const SystemId &system, int offset, int count, Works filter = Works::All) override;
    int jumpLetter(const SystemId &system, int fromIndex, int direction, Works filter = Works::All) override;
    Art gameArt(const GameRef &game, ArtSize size, bool priority = false) override;
    Art gameOptionsArt(const GameRef &game, ArtSize size, bool priority = false) override;
    Art gameArtFit(const GameRef &game, int width, int height, bool priority = false) override;
    // The Interface settings that reach the covers (the real adapter reads them from Preferences): "Show box art" and
    // "Load small boxarts in grid-view". The host runner sets them from the sample Interface service.
    void setCoverSettings(bool showBoxArt, bool smallGrid) { showBoxArt_ = showBoxArt; smallGrid_ = smallGrid; }
    // The sizes gameArt() was asked for since the counter was reset (a test of "Load small boxarts").
    int askedGrid = 0, askedSmall = 0;
    int arcadeView() override { return arcadeView_; }
    void setArcadeView(int view) { arcadeView_ = view; }

    std::vector<GameInfo> favorites() override;
    std::vector<GameInfo> recents() override;
    bool toggleFavorite(const GameRef &game) override;
    void removeRecent(const GameRef &game) override;

    void setSearch(const std::string &query, const SystemId &scope) override { setSearch(query, scope, 0); }
    SearchState searchState() override { return searchState(0); }
    void setSearch(const std::string &query, const SystemId &scope, int owner) override;
    SearchState searchState(int owner) override;
    bool matchesSearch(const std::string &title, const std::string &systemName, const std::string &query) override;

    // Games tab (additive): generated screenshots, a status-only preview, a remembered view per scope.
    ScreenshotState gameScreenshots(const GameRef &game) override;
    Art gameScreenshot(const GameRef &game, int index, ArtSize size, bool priority = false) override;
    // The preview behaves like Classic's: a countdown, then a generated moving test pattern plays while a view
    // keeps calling focusPreview() every frame; a frame without the call stops it.
    void focusPreview(const GameRef &game, int slotWidth, int slotHeight) override;
    PreviewState preview() override;
    ui::ImagePtr previewFrame() override { return previewImage_; }
    uint64_t previewFrameSerial() const override { return previewSerial_; }
    int homeView() override { return homeView_; }
    void setHomeView(int view) override { homeView_ = view; }
    int gamesView(bool oneSystem) override { return gamesViews_[oneSystem ? 1 : 0]; }
    void setGamesView(bool oneSystem, int view) override { gamesViews_[oneSystem ? 1 : 0] = view; }

    GameDetail detail(const GameRef &game) override;
    LaunchResult launch(const GameRef &game, const std::string &versionId = std::string()) override;

    DiscStatus disc() override { return disc_; }
    Art discArt(ArtSize size) override;
    Art discScreenshot(int index, ArtSize size) override;
    ArcadeScanState arcadeScan() override { return scan_; }
    void startArcadeScan() override { scan_.phase = ArcadeScanState::Phase::Done; scan_.progress = 1.0f; scan_.checked = scan_.total; ++revision_; }
    // Test hook: a scan in progress (phase Running at this fraction) or idle (before it ran).
    void setArcadeScan(ArcadeScanState::Phase phase, float progress);

    void startDisc() override { standDown_ = true; }
    void rescanDisc() override {}
    void reloadDiscArt() override {}

private:
    bool discTab_ = true;
    bool standDown_ = false;   // a game or disc was started
    struct Sys { std::string id, name, group; int count; bool hidden; bool arcade; };
    static std::string key(const GameRef &g) { return g.system + "/" + g.id; }
    const Sys *find(const SystemId &id) const;
    GameInfo make(const Sys &s, int index) const;
    // The arcade check's result for a sample game: only the arcade system is checked; some games have no
    // core, no ROM zip or a bad CRC.
    bool checkedGame(const Sys &s) const { return s.arcade && scan_.phase == ArcadeScanState::Phase::Done; }
    // The games of a scope in title order (system id breaks ties); empty scope = all systems.
    struct Item { int system; int index; };
    const std::vector<Item> &list(const SystemId &scope, Works filter);
    ui::ImagePtr art(const std::string &seed, int w, int h, bool icon);

    std::vector<Sys> systems_;
    std::set<std::string> favorites_;
    std::vector<GameRef> recents_;
    std::map<std::string, ui::ImagePtr> artCache_;
    std::map<int, SearchState> searches_;   // one per caller (owner)
    DiscStatus disc_;
    uint64_t revision_ = 1;
    uint64_t previewSerial_ = 0;
    bool previewTouched_ = false, previewActive_ = false;
    std::string previewKey_;
    float previewTime_ = 0.0f, previewFrameTime_ = 0.0f;
    int previewSlotW_ = 0, previewSlotH_ = 0;
    ui::ImagePtr previewImage_;
    void paintPreviewFrame();
    int gamesViews_[2] = {0, 0};
    bool showBoxArt_ = true, smallGrid_ = false;
    int arcadeView_ = 0;
    Art coverArt(const GameRef &game, ArtSize size);
    int homeView_ = 0;
    ArcadeScanState scan_;
    std::map<std::string, std::vector<Item>> lists_;
};

}  // namespace patsgui
