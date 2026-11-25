#pragma once

// The new GUI's view of the games library: one pure-virtual interface, plain value types, no threads.
//
// Rules (see src/app/README.md):
//   - Everything is called on the GUI thread. Implementations do their slow work (decoding, scanning,
//     searching) in small steps inside update(); no call here may block for a visible time.
//   - Art never blocks: a picture that is not loaded yet comes back as {nullptr, loading=true}; ask again
//     on a later frame (revision() changes when something arrived).
//   - Ids are opaque strings. Never parse them.

#include <cstdint>
#include <string>
#include <vector>

#include "ui-framework/core/Image.h"

namespace patsgui {

using SystemId = std::string;   // stable per system; also the key of a system's hidden flag

// A game is addressed by its system and its id (unique inside the system).
struct GameRef {
    SystemId system;
    std::string id;
    bool empty() const { return id.empty(); }
};

// Art sizes. Each is a maximum box in design pixels at 1080p; the picture keeps its proportions and is
// never enlarged. They match the Classic GUI's prepared variants.
enum class ArtSize { Small, Grid, Home, Detail };
struct ArtBounds { int width, height; };
inline ArtBounds artBounds(ArtSize size) {
    switch (size) {
        case ArtSize::Small:  return {128, 128};
        case ArtSize::Grid:   return {245, 245};
        case ArtSize::Home:   return {178, 178};
        case ArtSize::Detail: return {689, 624};
    }
    return {245, 245};
}

// A picture slot. image is set once loaded; loading is true while it may still arrive. Both empty
// means "there is no art": draw the placeholder and do not ask again soon.
struct Art {
    ui::ImagePtr image;
    bool loading = false;
};

struct SystemInfo {
    SystemId id;
    std::string name;        // display name
    std::string group;       // "Consoles", "Computers", ... (may be empty)
    int gameCount = -1;      // -1 while not known yet
    bool hidden = false;
    bool isArcade = false;
    // ---- Systems tab additions (defaults keep other tabs and implementations unchanged)
    int order = 0;           // position in the Classic catalog (grouped order); equal values keep name order
    bool noCore = false;     // no core is installed for it: the tile says so (Classic "no core")
};

struct GameInfo {
    GameRef ref;
    std::string title;
    std::string systemName;  // display name of its system
    bool favorite = false;
    int versionCount = 1;    // > 1: several ROM versions share this title

    // Metadata from the arcade check (the .mra files and what the drives hold for them). Empty / false
    // for a game the check has not seen; `checked` says whether it has. Cheap: no detail() call needed
    // to fill a table row.
    std::string year, manufacturer, category;
    bool checked = false;     // the arcade check knows this game
    bool works = false;       // "might work": core found, ROM zip found and its CRCs match
    bool coreFound = false;
    bool romFound = false;
    bool romCrcOk = false;
};

// Which games a list shows by the arcade check: everything, the ones that might work, or the checked ones
// that do not. A game the check has not seen counts as neither Working nor Broken.
enum class Works { All, Working, Broken };

struct GameVersion {
    std::string id;
    std::string label;       // e.g. "Europe"; may be empty
};

struct GameDetail {
    GameInfo info;
    std::string file;        // file name of the representative ROM
    std::vector<GameVersion> versions;   // empty when there is only one
    // Metadata that exists today. Empty strings are unknown; nothing is invented.
    std::string year, players, rating, overview;
    std::string coreFile;    // the core file the arcade check resolved (e.g. an .rbf); empty = none found
    std::string romFile;     // the ROM zip(s) the game wants, as named by its .mra
    // manufacturer, category and the check flags are in info.
};

// The arcade check: a walk over the drives' .mra files. It never runs on its own (it touches the drives);
// the Arcade tab starts it when the user asks.
struct ArcadeScanState {
    enum class Phase { Idle, Running, Done };
    Phase phase = Phase::Idle;
    float progress = 0.0f;   // 0..1; 0 while still finding files
    int total = 0;           // files to check (0 until known)
    int checked = 0;         // files looked at so far (Classic's "Checking Arcade game n of m")
    int working = 0;         // "might work" so far, counted over the entries (Classic's header and progress)
    int entries = 0;         // .mra files that were read so far (the m of Classic's "n of m might work")
};

enum class LaunchStatus { Started, NeedsVersion, Failed };
struct LaunchResult {
    LaunchStatus status = LaunchStatus::Failed;
    std::string error;       // plain words, for a dialog; empty unless Failed
};

struct SearchState {
    std::string query;
    bool done = true;                // false while the implementation is still working through games
    std::vector<GameInfo> results;   // so far: the first kMaxSearchResults matches in title order
    bool more = false;               // matches beyond those exist (shown as "500+"); false while all matches so far fit
    static constexpr int kMaxSearchResults = 500;
};

// ---- Games tab value types (additive)
// Screenshots of one game, looked up online only for the game whose Game options are open.
struct ScreenshotState {
    enum class Phase { Idle, Searching, Ready, Missing };
    Phase phase = Phase::Idle;
    int count = 0;           // screenshots available (Ready)
};
// The video preview of the game a list view or Game options shows. Everything is optional: with the
// feature off, `status` is empty and nothing plays.
struct PreviewState {
    std::string status;      // the one line under the picture ("Looking for a video...", ...); data, not a key
    bool playing = false;    // a video is playing: draw previewFrame() in place of the box art
};

// Read-only snapshot of the optical drive and the disc in it.
struct DiscStatus {
    enum class Media { NoDrive, NoDisc, Checking, Readable, Unsupported, Unreadable };
    enum class Console { Unknown, PlayStation, Saturn };
    enum class ArtState { Idle, Searching, Ready, Missing };
    Media media = Media::NoDrive;
    Console console = Console::Unknown;
    std::string device, model;
    std::string title, identifier, region;   // as read from the disc
    std::string error;                       // technical reason for Unreadable (secondary line)
    bool coreReady = false;                  // the core and package needed for this disc are installed
    bool packageBusy = false;                // the package is being updated
    bool launching = false;                  // a launch is under way (re-check before start)
    ArtState artState = ArtState::Idle;
    std::string year, players, rating, coop, overview;
    int screenshotCount = 0;
    bool canStart() const { return media == Media::Readable && coreReady && !packageBusy && !launching; }
    uint64_t generation = 0;                 // changes when another disc is seen
    // ---- Disc tab additions (defaults keep other code compiling)
    // What is missing when coreReady is false: the FPGA core file for this console, or the scoped
    // Physical Disc package (main, MGL, config). None when ready or when it does not matter.
    enum class Blocker { None, CoreMissing, PackageMissing };
    Blocker blocker = Blocker::None;
    // The core and package are ready but no MGL path is known for this console: Classic's Start button looks ready and A says
    // "The Physical Disc MGL is unavailable." The tab answers A with that text in a dialog.
    bool mglMissing = false;
};

class Library {
public:
    virtual ~Library() = default;

    // Call once per frame before drawing. Does bounded work and may change revision().
    virtual void update(float deltaSeconds) = 0;
    // Changes whenever something the GUI shows may have changed (art arrived, favorites, systems,
    // search progress, disc state). The host redraws when it differs from the value it last drew.
    virtual uint64_t revision() const = 0;
    // Re-reads favorites, recents, hidden flags and the system list (called when the GUI opens).
    virtual void refresh() = 0;
    // False for the sample implementation: the tabs may show a "sample data" note.
    virtual bool isSample() const { return false; }

    // ---- systems
    // Hidden systems are included with hidden=true so a settings tab can show them; the Systems tab
    // filters. Systems without games are not listed. Sorted by display name.
    virtual std::vector<SystemInfo> systems() = 0;
    // ---- Systems tab: the "Show Arcade in Consoles" preference. When true the Systems tab also lists
    // arcade systems (hidden flags still apply). Default false.
    virtual bool showArcadeInSystems() { return false; }
    virtual void setSystemHidden(const SystemId &system, bool hidden) = 0;
    virtual Art systemArt(const SystemId &system, ArtSize size) = 0;

    // ---- games of a system (window access; the title order is alphabetical and stable).
    // An EMPTY system id means all systems: every game of every system, sorted by title across systems
    // (the Games tab's "all games" list). `filter` narrows by the arcade check (see Works).
    virtual int gameCount(const SystemId &system, Works filter = Works::All) = 0;
    virtual std::vector<GameInfo> games(const SystemId &system, int offset, int count,
                                        Works filter = Works::All) = 0;
    // Index of the first game of the previous (-1) or next (+1) first letter after `fromIndex`
    // ("0-9" counts as one letter); `fromIndex` itself when there is none.
    virtual int jumpLetter(const SystemId &system, int fromIndex, int direction,
                           Works filter = Works::All) = 0;
    virtual Art gameArt(const GameRef &game, ArtSize size, bool priority = false) = 0;
    // The cover of the Game options panel. Classic's Game options ignore Settings > Interface > "Show box art" (App::
    // openGameActions requests the cover itself), while the tiles and the list's detail panel honor it: an implementation
    // gates gameArt() with the setting and leaves this call free. Default: gameArt().
    virtual Art gameOptionsArt(const GameRef &game, ArtSize size, bool priority = false) { return gameArt(game, size, priority); }
    // The cover sized to a given box (real pixels), for a detail panel: Classic requests the cover at the size of the
    // panel's picture slot (GamesScreen::renderDetail), so the picture fills the slot whatever the screen and text
    // size. Gated by "Show box art" like gameArt(). Default: the large fixed size.
    virtual Art gameArtFit(const GameRef &game, int width, int height, bool priority = false) {
        (void)width; (void)height;
        return gameArt(game, ArtSize::Detail, priority);
    }

    // ---- favorites and recents (small lists, returned whole; recents newest first)
    virtual std::vector<GameInfo> favorites() = 0;
    // Home's favorites strip. Classic's Home and its Favorites tab build their lists by different rules (a favorite whose file
    // is gone is shown by Home and not by the tab), so an implementation may answer differently; the default is favorites().
    virtual std::vector<GameInfo> homeFavorites() { return favorites(); }
    virtual std::vector<GameInfo> recents() = 0;
    // Returns the new state.
    virtual bool toggleFavorite(const GameRef &game) = 0;
    virtual void removeRecent(const GameRef &game) = 0;

    // ---- Favorites tab (additive; defaults keep other implementations compiling)
    // The view the Favorites tab shows when it opens (0 Grid, 1 Grid-Small, 2 Grid-List) and the call that
    // remembers the user's choice. Whether it is remembered at all is the implementation's rule.
    virtual int favoritesView() { return 0; }
    virtual void setFavoritesView(int view) { (void)view; }

    // ---- Home tab (additive): the view of the search results, remembered like the others (0 Grid, 1 Small, 2 List).
    virtual int homeView() { return 0; }
    virtual void setHomeView(int view) { (void)view; }
    // True when Settings > Default view is "Individual" (each screen remembers its own view). Classic's Home names the view
    // button in its help only then (HomeScreen::hints); with a global default the view is the setting's and Y is not offered.
    virtual bool viewsIndividual() { return true; }

    // ---- Arcade tab (additive): the view its list opens with (0 Grid, 1 Small, 2 List). Classic (App::init, cycleDefaultView):
    // the global Game view, and Grid when the setting is Individual (the Arcade list has no view of its own to remember).
    virtual int arcadeView() { return 0; }

    // ---- Games tab (additive; defaults keep other tabs and implementations unchanged)
    // The view the Games tab opens with (0 Grid, 1 Grid-Small, 2 Grid-List) for all games or for one system,
    // and the call that remembers the user's choice. Whether it is remembered is the implementation's rule.
    virtual int gamesView(bool oneSystem) { (void)oneSystem; return 0; }
    virtual void setGamesView(bool oneSystem, int view) { (void)oneSystem; (void)view; }
    // Screenshots of one game (Game options). The online lookup starts on the first call for a game.
    virtual ScreenshotState gameScreenshots(const GameRef &game) { (void)game; return ScreenshotState(); }
    virtual Art gameScreenshot(const GameRef &game, int index, ArtSize size, bool priority = false) {
        (void)game; (void)index; (void)size; (void)priority; return Art();
    }
    // The video preview. focusPreview() is called every frame while a view shows the game (and nothing
    // else is open over it); a frame without the call stops the preview. The slot is the size of the
    // picture area: a video larger than that is not played and never scaled.
    virtual void focusPreview(const GameRef &game, int slotWidth, int slotHeight) {
        (void)game; (void)slotWidth; (void)slotHeight;
    }
    virtual PreviewState preview() { return PreviewState(); }
    virtual ui::ImagePtr previewFrame() { return ui::ImagePtr(); }
    // Changes for every new video frame that previewFrame() holds, and ONLY for that: a new frame does not
    // change revision(). The host then redraws just the video (TabView::redrawPreview) instead of the whole
    // screen. Everything else about the preview (status text, playing or not) still changes revision().
    virtual uint64_t previewFrameSerial() const { return 0; }

    // ---- search. setSearch() starts (or replaces) a search; the answer fills in through searchState().
    // An empty system id searches every visible system. A search belongs to its caller (`owner`, any number a
    // caller picks for itself; src/app/SearchHeader gives each header its own): two callers never replace each
    // other's search or read each other's answer. An empty query ends the caller's search and frees it.
    // The calls without an owner are one shared slot (owner 0), kept for implementations and callers that
    // do not need more.
    virtual void setSearch(const std::string &query, const SystemId &scope) = 0;
    virtual SearchState searchState() = 0;
    virtual void setSearch(const std::string &query, const SystemId &scope, int owner) {
        (void)owner; setSearch(query, scope);
    }
    virtual SearchState searchState(int owner) { (void)owner; return searchState(); }
    // The search rule, for a list the tab holds itself (the favorites): does a game match the query? The same
    // rule as the searches above (Classic's: every word of the query starts a word of the title or the system).
    virtual bool matchesSearch(const std::string &title, const std::string &systemName, const std::string &query) {
        return title.find(query) != std::string::npos || systemName.find(query) != std::string::npos;
    }

    // ---- one game
    virtual GameDetail detail(const GameRef &game) = 0;
    // Starts the game. versionId may be empty; with several versions and no recent choice the result
    // is NeedsVersion and the tab asks the user, then calls again with the chosen version.
    // True once a game or a disc was started and the host is handing the screen to the core (after launch() answered
    // Started, or startDisc() got through). The shell plays the launch sound from it when it owns the sounds.
    virtual bool standingDown() const { return false; }
    virtual LaunchResult launch(const GameRef &game, const std::string &versionId = std::string()) = 0;

    // ---- arcade check
    virtual ArcadeScanState arcadeScan() = 0;
    // Starts (or restarts) the check; progress shows in arcadeScan(), results fill GameInfo/GameDetail.
    virtual void startArcadeScan() = 0;
    // ---- additions of the Arcade tab (defaults keep other implementations compiling)
    // Stops a running check at the user's request (Classic: B on the status screen cancels the scan).
    // arcadeScan() then reports Idle; a later startArcadeScan() starts over.
    virtual void cancelArcadeScan() {}

    // ---- disc
    virtual DiscStatus disc() = 0;
    virtual Art discArt(ArtSize size) = 0;
    virtual Art discScreenshot(int index, ArtSize size) = 0;
    // Starts the inserted disc after a fresh re-check (takes a few frames; disc().launching is true).
    virtual void startDisc() = 0;
    // Asks the drive for a fresh look and, if wanted, fetches the artwork again.
    virtual void rescanDisc() = 0;
    virtual void reloadDiscArt() = 0;
    // Whether the Disc tab is in the tab row (Classic: a drive is present and the Disc package is installed and set
    // up). The shell asks every frame, so answer from memory. When it turns false while the tab is active, the shell
    // returns to Home.
    virtual bool discTabAvailable() { return true; }
    // ---- Disc tab addition: abandons a start that is still re-checking (the Classic screen does this
    // when the user leaves the tab). Default: nothing to cancel.
    virtual void cancelDiscLaunch() {}
};

}  // namespace patsgui
