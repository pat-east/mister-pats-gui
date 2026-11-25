#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GamePreview.h"
#include "Geometry.h"

class Canvas;
class Theme;

// What a view needs to show the video preview of a game: the line of status text under the cover
// area and the video itself in the cover's place. The List view and the Game options panel both use
// it, so the two look and behave the same. The state machine behind it is GamePreview (shared
// through the Context); this class only draws and tells the view when to repaint.
//
//   - statusHeight():  how much room to keep for the status line under the cover area (0 when no
//                      preview applies to the selected game).
//   - setSlot():       tell the preview how much room the video has. The video is never scaled; one
//                      that is larger than this room is not played.
//   - videoShowing():  true while the video replaces the cover; the view then skips the cover.
//   - changed() / markPainted(): the status text or the picture changed since the view last painted
//                      its panel.
//   - drawStatus(), drawVideo(): the drawing. drawVideo() copies a new frame when one is due, and
//                      after the view repainted its panel it puts the last frame back.
class PreviewDisplay {
public:
    explicit PreviewDisplay(GamePreview &preview) : preview_(preview) {}

    int statusHeight(Theme &theme) const;
    void setSlot(const Rect &coverArea) { preview_.setSlotSize(coverArea.w, coverArea.h); }
    bool videoShowing() const { return preview_.videoPlaying(); }
    bool changed() const { return preview_.revision() != paintedRevision_; }
    void markPainted() { paintedRevision_ = preview_.revision(); }

    void drawStatus(Canvas &canvas, Theme &theme, const Rect &line) const;
    void drawVideo(Canvas &canvas, const Rect &coverArea, bool repainted);

private:
    GamePreview &preview_;
    int paintedRevision_ = -1;
    std::vector<uint32_t> frame_;   // the last frame, to draw again after a repaint
};
