#pragma once

#include <functional>
#include <string>
#include <vector>

#include "GameDatabase.h"
#include "Screen.h"

// The Arcade tab: three rows, Home-style — Games, Manufacturers, Categories — over one
// underlying list of games (see docs/ARCADE.md, "Decision: a dedicated Arcade tab").
//
// What Home's rows do not have is that each row's title is itself a stop, to the left of its
// first tile, and it is where a row starts. Confirming on it opens the whole dimension in a
// full view; Right steps off onto the tiles, where Confirm launches a game, or opens one
// group's games straight from the preview. That is what makes "open the full view"
// discoverable by someone who has never been told about it: it is where the cursor already is.
class ArcadeScreen : public Screen {
public:
    enum class Dimension { Games, Manufacturers, Categories };

    using OpenFullHandler = std::function<void(Dimension)>;
    using OpenGroupHandler = std::function<void(Dimension, const DatabaseGroup &)>;

    ArcadeScreen(Context &context, OpenFullHandler onOpenFull, OpenGroupHandler onOpenGroup);

    // Re-reads the game list and both group catalogues. Cheap enough for every visit to the
    // tab: one path list and two small catalogue files.
    void refresh();

    // Whether Back should step from a tile to its row's title rather than leave the tab —
    // asked by App, which otherwise treats Back on any tab but Home as "go Home".
    bool wantsBack() const;

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;
    std::string favoritePath() const override;

    // The kind of rows this screen builds, worth testing without a canvas.
    size_t rowCount() const { return rows_.size(); }
    int activeRow() const { return activeRow_; }
    int cursorInActiveRow() const { return rows_.empty() ? -1 : rows_[size_t(activeRow_)].cursor; }
    size_t itemsInRow(size_t row) const { return row < rows_.size() ? rows_[row].items.size() : 0; }

private:
    struct Item {
        Game game;               // Games row
        DatabaseGroup group;     // Manufacturers / Categories rows
        bool artworkResolved = false;
    };

    struct Row {
        Dimension dimension = Dimension::Games;
        std::string title;
        std::vector<Item> items;
        std::vector<float> focus;
        float titleFocus = 0.0f;
        int cursor = -1;   // -1 is the title
        int scroll = 0;
    };

    struct Metrics {
        int tileWidth = 0;
        int tileHeight = 0;
        int captionGame = 0;
        int captionGroup = 0;
        int growX = 0;
        int growY = 0;
        int rowHeight = 0;
    };
    Metrics metrics() const;

    Row &active() { return rows_[size_t(activeRow_)]; }
    const Row &active() const { return rows_[size_t(activeRow_)]; }

    void launch();
    void toggleFavorite();
    void jumpLetter(int direction);
    void renderRow(Canvas &canvas, const Rect &area, Row &row, bool isActive);
    int perPage(const Row &row, int laneWidth) const;
    void settleScroll(Row &row, int laneWidth);

    Context &context_;
    OpenFullHandler onOpenFull_;
    OpenGroupHandler onOpenGroup_;

    const GameSystem *system_ = nullptr;
    std::vector<Row> rows_;
    int activeRow_ = 0;
    int pageScroll_ = 0;
};
