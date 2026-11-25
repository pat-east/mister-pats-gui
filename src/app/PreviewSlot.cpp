#include "app/PreviewSlot.h"

#include <algorithm>

#include "ui-framework/kit/Text.h"

namespace patsgui {

void PreviewSlot::update(const GameRef &game) {
    if (!active_ || game.empty() || width_ <= 0 || height_ <= 0) return;
    library_.focusPreview(game, width_, height_);
}

PreviewSlot::Picture PreviewSlot::picture(const GameRef &game, ArtSize artSize, bool priority, bool gameOptions) {
    Picture pic;
    if (library_.preview().playing) {
        pic.image = library_.previewFrame();
        if (pic.image) {
            pic.video = true;
            pendingVideoW_ = pic.image->width();
            pendingVideoH_ = pic.image->height();
            return pic;
        }
    }
    pendingVideoW_ = pendingVideoH_ = 0;
    const Art art = gameOptions ? library_.gameOptionsArt(game, artSize, priority) : library_.gameArt(game, artSize, priority);
    pic.image = art.image;
    pic.loading = art.loading;
    return pic;
}

PreviewSlot::Picture PreviewSlot::pictureFit(const GameRef &game, int width, int height, bool priority) {
    Picture pic;
    if (library_.preview().playing) {
        pic.image = library_.previewFrame();
        if (pic.image) {
            pic.video = true;
            pendingVideoW_ = pic.image->width();
            pendingVideoH_ = pic.image->height();
            return pic;
        }
    }
    pendingVideoW_ = pendingVideoH_ = 0;
    const Art art = library_.gameArtFit(game, width, height, priority);
    pic.image = art.image;
    pic.loading = art.loading;
    return pic;
}

std::string PreviewSlot::status() const { return library_.preview().status; }

ui::Rect PreviewSlot::pictureRectIn(const ui::Rect &slot, const ui::Image &image) {
    int w = image.width(), h = image.height();
    if (w > slot.w || h > slot.h) {   // never enlarged; scaled down only to fit
        const long k = std::min(long(slot.w) * 1000 / w, long(slot.h) * 1000 / h);
        w = int(w * k / 1000);
        h = int(h * k / 1000);
    }
    return {slot.x + (slot.w - w) / 2, slot.y + (slot.h - h) / 2, w, h};
}

void PreviewSlot::drawPicture(ui::Painter &p, const ui::Theme &t, const ui::Rect &slot, const ui::Image &image,
                              bool video) {
    p.pushClip(slot);
    p.drawImage(image, pictureRectIn(slot, image), 255, video ? 0 : t.px(ui::Radius::chip));
    p.popClip();
}

void PreviewSlot::noteSlot(const ui::Rect &slot) {
    noted_ = slot;
    videoW_ = pendingVideoW_;
    videoH_ = pendingVideoH_;
    pendingVideoW_ = pendingVideoH_ = 0;
}

bool PreviewSlot::redrawVideo(ui::Painter &p, const ui::Theme &t, ui::Rect &damage) {
    if (!active_ || videoW_ <= 0 || noted_.empty() || !library_.preview().playing) return false;
    const ui::ImagePtr frame = library_.previewFrame();
    if (!frame || !frame->valid() || frame->width() != videoW_ || frame->height() != videoH_) return false;
    if (videoW_ > noted_.w || videoH_ > noted_.h) return false;   // the full draw scaled it: draw that again
    drawPicture(p, t, noted_, *frame, true);
    damage = pictureRectIn(noted_, *frame).intersect(noted_);
    return !damage.empty();
}

}  // namespace patsgui
