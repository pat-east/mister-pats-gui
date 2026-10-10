#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "GamesScreen.h"
#include "OnScreenKeyboard.h"

// Search input above the regular game grid.
class SearchScreen : public Screen {
public:
    explicit SearchScreen(Context &context);
    void reload();
    bool wantsBack() const { return keyboardVisible_; }
    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;
    std::string favoritePath() const override {
        return keyboardVisible_ ? std::string() : results_.favoritePath();
    }
    bool selectedGame(const GameSystem *&system, Game &game) const override {
        return results_.selectedGame(system, game);
    }

private:
    struct Entry { const GameSystem *system; Game game; };
    static bool matchesQuery(const std::string &gameName, const std::string &systemName,
                             const std::string &query);
    void submit();

    Context &context_;
    OnScreenKeyboard keyboard_{64};
    GamesScreen results_;
    std::vector<Entry> entries_;
    std::vector<std::pair<const GameSystem *, Game>> matches_;
    size_t buildSystemPosition_ = 0;
    size_t buildPathPosition_ = 0;
    std::vector<std::string> buildPaths_;
    std::string submittedQuery_;
    bool indexReady_ = false;
    bool keyboardVisible_ = false;
    bool inputFocused_ = true;
    bool hasSubmittedSearch_ = false;
    bool submitPending_ = false;
};
