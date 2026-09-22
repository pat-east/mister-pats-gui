#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Screen.h"

// The home tab: what you reached for last and what you marked, as two horizontal rows.
// Rows scroll sideways, the page scrolls down. Everything else lives under Games.
class HomeScreen : public Screen {
public:
    explicit HomeScreen(Context &context);

    void refresh();

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area) override;
    void handle(Action action) override;
    std::string hints() const override;
    bool incremental() const override { return true; }

private:
    struct Item {
        const GameSystem *system = nullptr;
        Game game;
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
    bool rowChanged(const Row &row, bool active) const;

    // Asks the cache for whatever is currently visible in this row, independent of whether
    // the row is about to be repainted. Called every frame so a decode can still land and
    // finish on a resting screen — see the note above the call site.
    void requestImages(Row &row, int laneWidth);

    Context &context_;
    std::vector<Row> rows_;
    int activeRow_ = 0;
    int pageScroll_ = 0;
    int rowHeight_ = 0;
    int paintedPageScroll_ = -1;
    uint64_t paintedImages_ = 0;
};
