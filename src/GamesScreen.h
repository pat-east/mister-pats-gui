#pragma once

#include <string>
#include <vector>

#include "GridView.h"
#include "Screen.h"

enum class GameView { List, BoxartLarge, Grid, BoxartSmall, Compact, kCount };

// Shows a list of games in one of four presentations. Also used for the favourites tab,
// which is the same view over a different source.
class GamesScreen : public Screen {
public:
    explicit GamesScreen(Context &context);

    void showSystem(const GameSystem &system);
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

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    struct Entry {
        const GameSystem *system = nullptr;
        Game game;
    };

    const Entry *current() const;
    void renderList(Canvas &canvas, const Rect &area);
    void renderGrid(Canvas &canvas, const Rect &area);
    void renderHeader(Canvas &canvas, const Rect &area);
    void renderDetail(Canvas &canvas, const Rect &area, const Entry &entry);
    void toggleFavorite();
    void launch();
    void moveCursor(int dx, int dy);
    void jumpLetter(int direction);

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
};
