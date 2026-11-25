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
        int imageRevealPercent = 100;
        bool favorite = false;
        bool showLabel = true;
        bool coverArt = false;     // fit artwork inside instead of filling the tile
        bool nativeImageSize = false; // center cover art at its decoded pixel size
        bool keepImageOpaque = false; // avoid focus dimming for opaque system icons
        bool blackArtBackground = false; // match black-backed system icons
        bool compactPlaceholder = false; // use native small-boxart dimensions when art is missing
        int placeholderWidth = 0;
        int placeholderHeight = 0;
        bool nameIsInside = false; // set by draw(); the placeholder reads it
    };

    // `focus` is the animated 0..1 highlight weight.
    static void draw(Canvas &canvas, Theme &theme, const Rect &frame, const Content &content,
                     float focus);

    // The tile grows when focused; callers use this to reserve space.
    static Rect focusedFrame(const Rect &frame, float focus);

    // How far a focused tile of this size reaches past its own frame. Layouts must leave
    // this much room, or the highlight is clipped along the edge it grew into.
    // How far a tile's focus value moves toward its target in one frame. Focus on a tile is a
    // border (cover art does not grow), so it switches at once: easing it took ~0.4 s and made
    // moving between games feel laggy, and every frame of it repainted the tiles involved.
    static float focusStep(float /*deltaSeconds*/) { return 1.0f; }

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
