#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "GridView.h"
#include "Artwork.h"
#include "Screen.h"

enum class GameView { List, Grid, BoxartSmall, kCount };

// The stable, lowercase names used on the command line (--view) and in preferences.txt, as
// opposed to GamesScreen's display names, which are for the settings row and can change
// without breaking a saved setting.
GameView gameViewFromName(const std::string &name);
const char *nameForGameView(GameView view);

// The label shown on a tile's presentation, and in Settings' "Default view" row.
const char *displayNameForGameView(GameView view);

// Shows a list of games in one of three presentations. Also used for the favourites tab,
// which is the same view over a different source.
class GamesScreen : public Screen {
public:
    explicit GamesScreen(Context &context);

    // `title` replaces the system's name in the header, for a filtered view such as one
    // Arcade manufacturer — "Arcade › Manufacturers › Capcom" — over a system that is still
    // called "Arcade" everywhere else, favourites and history included.
    void showSystem(const GameSystem &system, const std::string &title = std::string());
    void showFavorites();

    // Every game of every system in one grid, sorted by title across system boundaries so
    // the letter jump works over the whole library rather than within one console.
    void showAllGames();
    void reload();

    bool empty() const { return entries_.empty(); }
    const std::string &title() const { return title_; }

    // Public so a launch can be triggered without a controller (see App::Options).
    void launchCurrent() { launch(); }

    GameView view() const { return view_; }
    void setView(GameView view);

    // Grid tiles are repainted only when their content or focus changes. The list detail
    // panel still uses a full redraw because its background fade changes continuously.
    bool incremental() const override {
        return view_ != GameView::List && !entries_.empty() && context_.background;
    }

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;
    std::string favoritePath() const override;

private:
    // A stub (path + name, no I/O — see Library::makeStub) until it is actually about to be
    // drawn. Every entry in even the whole 10,500-game library can be built this way in a
    // handful of milliseconds; only artworkResolved marks whether the expensive part
    // (Library::resolveArtwork, real `stat()` calls) has happened yet for this one.
    struct Entry {
        const GameSystem *system = nullptr;
        Game game;
        bool artworkResolved = false;
        ArtworkVariant artworkVariant = ArtworkVariant::Full;
    };

    const Entry *current() const;
    void renderList(Canvas &canvas, const Rect &area);
    void renderGrid(Canvas &canvas, const Rect &area, bool fullRedraw);
    void renderHeader(Canvas &canvas, const Rect &area);
    void renderDetail(Canvas &canvas, const Rect &area, const Entry &entry);
    void toggleFavorite();
    void launch();
    void moveCursor(int dx, int dy);
    void jumpLetter(int direction);

    // Resolves artwork for one entry if it has not been, or was resolved for the other size
    // variant. Idempotent and cheap to call every frame for the same entry once it is done.
    void ensureArtwork(Entry &entry, ArtworkVariant variant);

    Context &context_;
    std::vector<Entry> entries_;
    std::vector<float> focus_;
    std::string title_;
    bool favoritesMode_ = false;
    bool allMode_ = false;
    const GameSystem *system_ = nullptr;

    GameView view_ = GameView::Grid;
    GridView grid_;
    int cursor_ = 0;
    int scrollRow_ = 0;
    float detailFade_ = 0.0f;
    std::string detailPath_;

    std::vector<float> paintedFocus_;
    std::vector<const Image *> paintedArtwork_;
    int paintedScrollRow_ = -1;
    size_t paintedCount_ = size_t(-1);
    int paintedColumns_ = -1;
    int paintedTileWidth_ = -1;
    int paintedTileHeight_ = -1;
    bool needsPaint_ = true;
};
