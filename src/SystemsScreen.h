#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "GridView.h"
#include "Screen.h"

// Tile grid of every system that actually has games.
class SystemsScreen : public Screen {
public:
    using OpenHandler = std::function<void(const GameSystem &)>;

    SystemsScreen(Context &context, OpenHandler onOpen);

    void refresh();

    // True while the system list is still being built up.
    bool scanning() const { return scanned_ < total_; }

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;
    bool incremental() const override { return true; }

    const GameSystem *current() const;

private:
    Context &context_;
    OpenHandler onOpen_;

    std::vector<const GameSystem *> systems_;
    std::vector<float> focus_;
    GridView grid_;
    int cursor_ = 0;
    int scrollRow_ = 0;

    // Only an explicitly requested live directory scan uses this paced path. The saved
    // database and Console Mode index are read immediately in refresh().
    void scanStep(float deltaSeconds);
    size_t scanned_ = 0;
    size_t total_ = 0;
    float settleDelay_ = 0.0f;

    // Asks the cache for every visible icon regardless of whether this frame is about to
    // repaint — see the note above the call site in render().
    void requestImages(int first, int last);

    // Focus values as they were last painted, so only tiles that actually moved
    // are redrawn. Everything else stays untouched in the canvas.
    std::vector<float> paintedFocus_;
    int paintedScrollRow_ = -1;
    size_t paintedCount_ = 0;
    uint64_t paintedImages_ = 0;
};
