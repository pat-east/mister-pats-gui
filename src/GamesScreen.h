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
    // True while entries_ is still being built up from a queued path list. A caller deciding
    // whether to (re)start a load must check this too — `empty()` alone stays true for a
    // system large enough that this takes a while, and re-starting would throw the progress
    // away and begin again.
    bool loading() const { return loading_; }
    const std::string &title() const { return title_; }

    // Public so a launch can be triggered without a controller (see App::Options).
    void launchCurrent() { launch(); }

    GameView view() const { return view_; }
    void setView(GameView view);

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
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

    // Turns queued paths into Entry objects a few milliseconds at a time instead of all at
    // once — see the note on buildBudgetMs_ below for why.
    void buildStep();

    Context &context_;
    std::vector<Entry> entries_;
    std::vector<float> focus_;
    std::string title_;
    bool favoritesMode_ = false;
    bool allMode_ = false;
    const GameSystem *system_ = nullptr;

    // Resolving a game's artwork paths costs real `stat()` calls, and a big system is
    // thousands of them — building every Entry in one go before the first frame is what used
    // to make opening one look like a hang. reload() now only queues the raw paths, already in
    // their final display order (sortKey costs no I/O — see Library::nameFor — so working that
    // out for everything up front is cheap even for the whole library at once). buildStep()
    // then turns a slice of pending_ into real entries each frame, appending as it goes: since
    // the order is already right, nothing has to wait for the slowest entry before it can be
    // shown.
    struct PendingItem {
        const GameSystem *system;
        std::string path;
        std::string sortKey;
    };
    std::vector<PendingItem> pending_;
    size_t pendingIndex_ = 0;
    bool loading_ = false;

    GameView view_ = GameView::Grid;
    GridView grid_;
    int cursor_ = 0;
    int scrollRow_ = 0;
    float detailFade_ = 0.0f;
    std::string detailPath_;
};
