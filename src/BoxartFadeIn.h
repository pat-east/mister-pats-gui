#pragma once

#include "Image.h"

// Tracks the existing three-step pixel reveal for one displayed boxart.
class BoxartFadeIn {
public:
    // Returns true when the displayed image changes. Focused artwork is always complete.
    bool observe(const ImagePtr &image, bool focused) {
        const Image *current = image.get();
        const bool changed = current != image_;
        if (changed) {
            image_ = current;
            phase_ = current && !focused ? 0 : 3;
        } else if (focused) {
            phase_ = 3;
        }
        return changed;
    }

    int revealPercent() const {
        static constexpr int kRevealPercent[] = {50, 75, 100};
        return kRevealPercent[phase_ < 3 ? phase_ : 2];
    }

    bool active() const { return image_ && phase_ < 3; }
    const Image *image() const { return image_; }

    void advance() {
        if (phase_ < 3) ++phase_;
    }

private:
    const Image *image_ = nullptr;
    int phase_ = 3;
};
