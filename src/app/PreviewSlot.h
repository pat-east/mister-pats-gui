#pragma once

// The preview slot: the picture area of a game that shows its video (as in the Classic List view and Game
// options) or, when no video plays, its cover art, with the status line of the preview under it.
//
// A view that shows a game's picture owns one PreviewSlot (ListDetail and GameOptionsView do):
//
//     PreviewSlot slot_{library_};
//     void onShow() override { slot_.show(); }       // the tab became the active one
//     void onHide() override { slot_.hide(); }       // ... or was left: the preview stops
//     bool update(float) override { slot_.update(focusedGameOrEmpty); return false; }   // every frame
//     // in draw(): size the slot, take the picture and the status line, tell the slot where it was drawn
//     slot_.setSlotSize(slot.w, slot.h);              // real pixels: a video larger than this is not played
//     PreviewSlot::Picture pic = slot_.picture(game, ArtSize::Detail);
//     ... draw pic.image (drawPicture) and slot_.status() ...
//     slot_.noteSlot(slot);                           // for redrawVideo()
//
// Nothing here blocks: the video and the art arrive through the Library (revision() changes, the host
// redraws). The preview runs only for a frame in which update() named a game, so a hidden tab, a closed
// view or an empty game stops it.

#include <string>

#include "app/Library.h"
#include "ui-framework/kit/Components.h"

namespace patsgui {

class PreviewSlot {
public:
    explicit PreviewSlot(Library &library) : library_(library) {}

    void show() { active_ = true; }
    void hide() { active_ = false; }
    bool active() const { return active_; }

    // The size of the picture area in real pixels (the view sets it from the slot it draws into).
    void setSlotSize(int width, int height) { width_ = width; height_ = height; }

    // Once per frame from TabView::update(): asks the library for this game's preview. An empty ref (or a
    // hidden slot, or no slot size yet) asks for nothing, which stops a running preview.
    void update(const GameRef &game);

    struct Picture {
        ui::ImagePtr image;      // the current video frame while a video plays, else the cover art
        bool loading = false;    // the cover art may still arrive
        bool video = false;
    };
    // `gameOptions`: the cover of the Game options panel (Library::gameOptionsArt: not gated by "Show box art").
    Picture picture(const GameRef &game, ArtSize artSize, bool priority = true, bool gameOptions = false);
    // The same, with the cover requested at the size of the slot it is drawn into (a detail panel).
    Picture pictureFit(const GameRef &game, int width, int height, bool priority = true);

    // The preview's status line ("Video plays in: 5 s"); empty when none applies. Data, not a catalog key.
    std::string status() const;

    // Draws a picture centered in `slot`, clipped to it, scaled down only when it does not fit (never
    // enlarged). The one code path for every full draw of a picture and for redrawVideo(), so a partial
    // video frame is pixel-identical to a full redraw.
    static ui::Rect pictureRectIn(const ui::Rect &slot, const ui::Image &image);
    // A video frame is drawn with square corners (as Classic blits it): a moving picture repainted alone over
    // its last frame must give exactly what a full draw gives, which anti-aliased corners would not.
    static void drawPicture(ui::Painter &p, const ui::Theme &t, const ui::Rect &slot, const ui::Image &image,
                            bool video = false);

    // Tells the slot where the picture was just drawn (call after the draw that used picture(): the slot
    // rectangle of that draw). It remembers it, and whether a video frame was in it, for redrawVideo().
    void noteSlot(const ui::Rect &slot);
    // Repaints ONLY the current video frame into the slot noted by the last full draw. Returns false when
    // that is not possible (no video was in the last draw, the frame changed size, it does not fit the
    // slot): the caller then needs a full redraw. On true, `damage` is the rectangle that changed.
    bool redrawVideo(ui::Painter &p, const ui::Theme &t, ui::Rect &damage);

private:
    Library &library_;
    bool active_ = false;
    int width_ = 0, height_ = 0;
    // A video frame went into picture() since the last noteSlot(): its size.
    int pendingVideoW_ = 0, pendingVideoH_ = 0;
    // The last full draw: the slot, and the video's size in it (0 = no video was drawn).
    ui::Rect noted_;
    int videoW_ = 0, videoH_ = 0;
};

}  // namespace patsgui
