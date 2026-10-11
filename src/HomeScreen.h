#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Screen.h"
#include "BoxartFadeIn.h"
#include "GameSearch.h"
#include "GamesScreen.h"

// The home tab: a horizontal recent row followed by a scrollable favorites grid.
class HomeScreen : public Screen {
public:
    explicit HomeScreen(Context &context);

    void refresh();
    void invalidateSearchIndex();
    void setSearchView(GameView view) { searchResults_.setView(view); }
    void setSearchViewChangedCallback(std::function<void(GameView)> callback) {
        searchResults_.setViewChangedCallback(std::move(callback));
    }

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;
    std::string favoritePath() const override;
    bool selectedGame(const GameSystem *&system, Game &game) const override;
    bool usesStartAction() const override { return search_.usesStartAction(); }
    bool hasBackOverlay() const override { return search_.wantsBack(); }
    bool incremental() const override { return true; }

private:
    struct Item {
        const GameSystem *system = nullptr;
        Game game;
        BoxartFadeIn boxartFadeIn;
    };

    struct Row {
        std::string title;
        std::vector<Item> items;
        std::vector<float> focus;
        int cursor = 0;
        int scroll = 0;
        std::string emptyHint;

        // State as last painted, so an unchanged row is left alone.
        std::vector<float> paintedFocus;
        int paintedScroll = -1;
        size_t paintedCount = size_t(-1);
        bool paintedActive = false;
    };

    // Everything the row layout needs, worked out once from the tile itself rather than
    // guessed at in two places.
    struct Metrics {
        int tileWidth = 0;
        int tileHeight = 0;
        int caption = 0;     // room below the picture for the title and the platform line
        int growX = 0;       // how far a focused tile reaches sideways
        int growY = 0;       // and vertically
        int rowHeight = 0;
    };
    Metrics metrics() const;

    const Item *current() const;
    void jumpLetter(Row &row, int direction);
    void launch();
    void toggleFavorite();
    void renderRow(Canvas &canvas, const Rect &area, Row &row, bool active);
    void renderFavorites(Canvas &canvas, const Rect &body, int sectionY, const Metrics &m);
    bool rowChanged(const Row &row, bool active) const;
    void beginSearch(const std::string &query);
    void buildSearchIndex();
    void showSearchResults();

    // Asks the cache for whatever is currently visible in this row, independent of whether
    // the row is about to be repainted. Called every frame so a decode can still land and
    // finish on a resting screen — see the note above the call site.
    bool requestImages(Row &row, int laneWidth, bool active);
    bool requestFavoriteImages(const Rect &body, int gridY, const Metrics &m);

    Context &context_;
    GameSearch search_;
    GamesScreen searchResults_;
    std::vector<Row> rows_;
    std::vector<std::pair<const GameSystem *, Game>> searchIndex_;
    size_t searchBuildSystem_ = 0;
    size_t searchBuildPath_ = 0;
    std::vector<std::string> searchBuildPaths_;
    std::string pendingSearchQuery_;
    bool searchIndexReady_ = false;
    bool searchBuildPending_ = false;
    bool keyboardWasVisible_ = false;
    int activeRow_ = 0;
    int pageScroll_ = 0;
    int favoriteColumns_ = 1;
    int paintedPageScroll_ = -1;
    uint64_t paintedImages_ = 0;
    bool needsPaint_ = true;
};
