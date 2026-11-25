#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ArcadeScan.h"
#include "GameScreenshotService.h"
#include "Screen.h"
#include "app/Library.h"

// Implements the new GUI's library interface (src/app/Library.h) on top of the Classic GUI's Library,
// Favorites, History, HiddenSystems, ImageCache and the disc services. It holds no Classic behavior of
// its own: every call is a thin translation, and slow work (counting, searching) is done in small
// steps from update(). Classic objects are reached through Context; ids are names and ROM paths, so a
// library reload never leaves a dangling pointer.
class AppLibraryAdapter : public patsgui::Library {
public:
    explicit AppLibraryAdapter(Context &context);

    // True when the Classic library has systems, i.e. there is something to show.
    static bool ready(const Context &context);

    void update(float deltaSeconds) override;
    uint64_t revision() const override { return revision_; }
    void refresh() override;

    std::vector<patsgui::SystemInfo> systems() override;
    bool showArcadeInSystems() override;   // Systems tab: the Classic "Show Arcade in Consoles" preference
    void setSystemHidden(const patsgui::SystemId &system, bool hidden) override;
    patsgui::Art systemArt(const patsgui::SystemId &system, patsgui::ArtSize size) override;

    int gameCount(const patsgui::SystemId &system, patsgui::Works filter = patsgui::Works::All) override;
    std::vector<patsgui::GameInfo> games(const patsgui::SystemId &system, int offset, int count,
                                         patsgui::Works filter = patsgui::Works::All) override;
    int jumpLetter(const patsgui::SystemId &system, int fromIndex, int direction,
                   patsgui::Works filter = patsgui::Works::All) override;
    patsgui::Art gameArt(const patsgui::GameRef &game, patsgui::ArtSize size, bool priority = false) override;
    patsgui::Art gameOptionsArt(const patsgui::GameRef &game, patsgui::ArtSize size, bool priority = false) override;
    patsgui::Art gameArtFit(const patsgui::GameRef &game, int width, int height, bool priority = false) override;

    std::vector<patsgui::GameInfo> favorites() override;        // the Favorites tab's list (Classic GamesScreen favorites mode)
    std::vector<patsgui::GameInfo> homeFavorites() override;    // Home's strip (Classic HomeScreen::refresh)
    std::vector<patsgui::GameInfo> recents() override;
    bool toggleFavorite(const patsgui::GameRef &game) override;
    void removeRecent(const patsgui::GameRef &game) override;
    int favoritesView() override;                       // Favorites tab
    void setFavoritesView(int view) override;

    int homeView() override;                            // Home tab: the search results' view
    void setHomeView(int view) override;
    bool viewsIndividual() override;

    // Games tab: remembered view, screenshots of the Game options, the video preview.
    int gamesView(bool oneSystem) override;
    int arcadeView() override;                          // Arcade tab: the global Game view, Grid in Individual mode
    void setGamesView(bool oneSystem, int view) override;
    patsgui::ScreenshotState gameScreenshots(const patsgui::GameRef &game) override;
    patsgui::Art gameScreenshot(const patsgui::GameRef &game, int index, patsgui::ArtSize size,
                                bool priority = false) override;
    void focusPreview(const patsgui::GameRef &game, int slotWidth, int slotHeight) override;
    patsgui::PreviewState preview() override;
    ui::ImagePtr previewFrame() override { return previewImage_; }
    uint64_t previewFrameSerial() const override { return previewSerial_; }

    void setSearch(const std::string &query, const patsgui::SystemId &scope) override { setSearch(query, scope, 0); }
    patsgui::SearchState searchState() override { return searchState(0); }
    void setSearch(const std::string &query, const patsgui::SystemId &scope, int owner) override;
    patsgui::SearchState searchState(int owner) override;
    bool matchesSearch(const std::string &title, const std::string &systemName, const std::string &query) override;

    patsgui::GameDetail detail(const patsgui::GameRef &game) override;
    patsgui::LaunchResult launch(const patsgui::GameRef &game, const std::string &versionId = std::string()) override;

    patsgui::ArcadeScanState arcadeScan() override;
    void startArcadeScan() override;
    void cancelArcadeScan() override;

    patsgui::DiscStatus disc() override;
    patsgui::Art discArt(patsgui::ArtSize size) override;
    patsgui::Art discScreenshot(int index, patsgui::ArtSize size) override;
    void startDisc() override;
    void rescanDisc() override;
    void reloadDiscArt() override;
    void cancelDiscLaunch() override;
    bool standingDown() const override { return context_.standDown; }
    bool discTabAvailable() override { return discTabAvailable_; }

private:
    bool computeDiscTabAvailable() const;
    patsgui::Art coverArt(const patsgui::GameRef &game, patsgui::ArtSize size, bool priority);   // gameArt without the setting
    const GameSystem *system(const patsgui::SystemId &id) const;
    const Game *findGame(const GameSystem &system, const std::string &path) const;
    patsgui::GameInfo info(const GameSystem &system, const Game &game) const;
    bool organize() const;
    const ArcadeEntry *arcadeFor(const std::string &path) const;
    const ArcadeEntry *arcadeFor(const Game &game) const;
    void fillArcade(patsgui::GameInfo &info, const std::string &path) const;
    // A list across systems and/or filtered by the arcade check: (system index, game index) pairs in
    // title order, built once and cached. nullptr when the plain per-system list applies.
    struct Item { size_t system; size_t index; };
    const std::vector<Item> *itemsFor(const patsgui::SystemId &scope, patsgui::Works filter);
    const Game &gameAt(const Item &item) const;
    void dropCaches();
    // Classic Home's group lookup and tiles (see the favorites and recents section of the .cpp).
    const Game *groupFor(const GameSystem &system, const std::string &path);
    patsgui::GameInfo homeInfo(const GameSystem &system, const std::string &path, const Game *group);

    // Pictures: a converted copy per request, bounded; Classic's ImageCache does the decoding.
    patsgui::Art artFor(const std::string &key, const std::vector<std::string> &candidates, int w, int h,
                        bool priority);
    static ui::ImagePtr convert(const Image &image);

    void stepCounts();
    bool screenshotTimedOut() const;
    bool backgroundShot(const Game &game, const patsgui::GameRef &ref);
    void stepArcade();
    void stepSearch();
    void stepDiscLaunch();
    void pollDisc();

    Context &context_;
    uint64_t revision_ = 1;
    uint64_t previewSerial_ = 0;
    uint64_t libraryRevision_ = 0;
    uint64_t imageGeneration_ = 0;
    // System -> Download missing icons: when the set of installed system icons changes, the converted "sys|" pictures
    // (which may be a fallback icon) are dropped and the tabs ask again.
    float iconCheckTimer_ = 0.0f;
    float discTabTimer_ = 0.0f;
    bool discTabAvailable_ = false;   // Classic's App::visibleTabs() rule, read once a second (see update())
    size_t iconsMissing_ = size_t(-1);

    std::map<std::string, int> counts_;      // system name -> games, filled one system per frame
    std::unordered_map<std::string, ui::ImagePtr> converted_;

    // The arcade check: runs only when asked (it walks the drives), a few steps per frame.
    ArcadeScan arcadeScan_;
    size_t arcadeCounted_ = 0, arcadeWorking_ = 0;   // entries seen so far / of them "might work" (Classic's count)
    bool arcadeCancelled_ = false;   // the user stopped a running check: it is no longer stepped
    std::vector<ArcadeEntry> arcadeEntries_;
    std::unordered_map<std::string, size_t> arcadeIndex_;   // .mra path -> arcadeEntries_
    bool arcadeCollected_ = true;
    std::map<std::string, std::vector<Item>> lists_;
    std::map<std::string, std::unordered_map<std::string, size_t>> groupIndex_;   // system -> path (or version path) -> index in gamesOf(system, true)

    // Search work in progress: one slot per caller (owner), so Home, Games and the Arcade list never replace
    // each other's search. Only the first unfinished slot is stepped each frame (a few milliseconds).
    struct SearchSlot {
        patsgui::SearchState state;
        std::vector<std::string> systems;
        size_t system = 0, index = 0;   // where the walk stands (slot.systems, then the system's game list)
        bool includeSystem = false;
        struct Kept { std::string title; size_t system, index; };   // a match that is among the first kMaxSearchResults by title
        std::vector<Kept> kept;
        size_t matched = 0;             // every match so far, kept or not
    };
    void stepSearchSlot(SearchSlot &slot);
    std::map<int, SearchSlot> searches_;

    // Games tab: screenshots (the service and its thread start when the first lookup is asked for) and the
    // preview picture (one image reused for every frame).
    std::unique_ptr<GameScreenshotService> ownScreenshots_;   // only when the Context has none
    GameScreenshotService *screenshots_ = nullptr;
    std::string screenshotKey_;
    ui::ImagePtr previewImage_;
    int previewRevision_ = -1;
    size_t screenshotCount_ = 0;
    int screenshotPhase_ = -1;
    std::chrono::steady_clock::time_point screenshotStarted_{};
    bool screenshotTimeoutSeen_ = false;
    std::string previewKey_;
    Game previewGame_;
    bool previewFocused_ = false;

    // Disc.
    bool discWanted_ = false;
    bool installerBusy_ = false;
    std::chrono::steady_clock::time_point artSearchStarted_{};
    bool artTimeoutSeen_ = false;
    bool discPolled_ = false;   // the snapshots below have been read at least once
    DiscDriveService::Snapshot discSnap_;
    DiscCoreManager::Status coreSnap_;
    DiscArtworkService::Snapshot artSnap_;
    bool launching_ = false;
    uint64_t launchGeneration_ = 0, launchRevision_ = 0;
    std::chrono::steady_clock::time_point launchStarted_{};
};
