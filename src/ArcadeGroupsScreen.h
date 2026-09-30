#pragma once

#include <functional>
#include <string>
#include <vector>

#include "GameDatabase.h"
#include "GridView.h"
#include "Screen.h"

// The full view behind the Arcade tab's Manufacturers and Categories rows: every group as a
// tile with its name and game count. Confirming one opens that group's games.
//
// Only grouped names here, never games — so no artwork, and nothing shared with GamesScreen
// beyond the low-level Tile and GridView both are built on. The games themselves come from
// GamesScreen, unmodified, once a group is chosen.
class ArcadeGroupsScreen : public Screen {
public:
    using OpenHandler = std::function<void(const DatabaseGroup &)>;

    ArcadeGroupsScreen(Context &context, OpenHandler onOpen);

    // `manufacturers` picks which of the two catalogues to show.
    void show(bool manufacturers);

    bool manufacturers() const { return manufacturers_; }

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

    size_t size() const { return groups_.size(); }

private:
    void jumpLetter(int direction);

    Context &context_;
    OpenHandler onOpen_;

    bool manufacturers_ = true;
    std::vector<DatabaseGroup> groups_;
    std::vector<float> focus_;
    GridView grid_;
    int cursor_ = 0;
    int scrollRow_ = 0;
};
