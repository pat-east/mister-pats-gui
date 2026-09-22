#pragma once

#include <string>

#include "Geometry.h"
#include "Image.h"
#include "Theme.h"

class Canvas;

// The one tile used for systems, games and boxart alike. Only the content differs, so the
// whole interface stays visually consistent.
class Tile {
public:
    struct Content {
        std::string label;
        std::string sublabel;
        ImagePtr image;            // nullptr falls back to a typographic tile
        bool favorite = false;
        bool showLabel = true;
        bool coverArt = false;     // fit artwork inside instead of filling the tile
        bool nameIsInside = false; // set by draw(); the placeholder reads it
    };

    // `focus` is the animated 0..1 highlight weight.
    static void draw(Canvas &canvas, Theme &theme, const Rect &frame, const Content &content,
                     float focus);

    // The tile grows when focused; callers use this to reserve space.
    static Rect focusedFrame(const Rect &frame, float focus);

    // How far a focused tile of this size reaches past its own frame. Layouts must leave
    // this much room, or the highlight is clipped along the edge it grew into.
    static int focusMarginX(const Rect &frame);
    static int focusMarginY(const Rect &frame);

    // Height a tile needs below the picture for its caption, and for the second line when
    // there is one. Layouts that reserve less end up drawing the second line outside
    // themselves, where it is clipped away.
    static int captionHeight(Theme &theme, bool showLabel, bool hasSublabel);

    // Everything the tile may paint for this cell, focus and shadow included.
    // Screens that redraw incrementally restore exactly this area.
    static Rect footprint(Theme &theme, const Rect &frame);

private:
    static void drawPlaceholder(Canvas &canvas, Theme &theme, const Rect &body,
                                const Content &content, float focus);
};
