#pragma once

#include <memory>
#include <string>

#include "ui-framework/core/Painter.h"

namespace ui {

// An icon: a small picture of fixed size that is drawn in one color (docs/components/Icon.md). It is a
// coverage mask underneath, so the same icon takes the focus color, the muted color or a warning tone.
// Kinds of icons derive from this class; BootstrapIcon is the first.
class Icon {
public:
    virtual ~Icon() = default;

    int width() const { return width_; }
    int height() const { return height_; }

    // Draws the icon with its top-left corner at (x, y).
    virtual void draw(Painter &p, int x, int y, Color color) const = 0;

    // Draws the icon centered in `box`.
    void drawIn(Painter &p, const Rect &box, Color color) const {
        draw(p, box.x + (box.w - width_) / 2, box.y + (box.h - height_) / 2, color);
    }

protected:
    Icon(int width, int height) : width_(width), height_(height) {}

private:
    int width_;
    int height_;
};

using IconPtr = std::shared_ptr<const Icon>;

// A Bootstrap Icons icon by name ("chevron-right", "house", ...), rendered from its SVG at the given
// size. The SVG is drawn once per name and size and cached; an unknown name draws nothing and reports
// valid() == false.
class BootstrapIcon : public Icon {
public:
    BootstrapIcon(const std::string &name, int width, int height);

    const std::string &name() const { return name_; }
    bool valid() const { return mask_ != nullptr; }

    void draw(Painter &p, int x, int y, Color color) const override;

private:
    std::string name_;
    ImagePtr mask_;
};

inline IconPtr makeBootstrapIcon(const std::string &name, int width, int height) {
    return IconPtr(new BootstrapIcon(name, width, height));
}

}  // namespace ui
