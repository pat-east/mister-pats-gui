#pragma once

#include <algorithm>

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    int right()  const { return x + w; }
    int bottom() const { return y + h; }
    bool empty() const { return w <= 0 || h <= 0; }

    bool contains(int px, int py) const {
        return px >= x && py >= y && px < right() && py < bottom();
    }

    Rect inset(int d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }

    Rect intersect(const Rect &o) const {
        const int nx = std::max(x, o.x);
        const int ny = std::max(y, o.y);
        const int nr = std::min(right(), o.right());
        const int nb = std::min(bottom(), o.bottom());
        return {nx, ny, nr - nx, nb - ny};
    }

    // Largest rect of the given aspect ratio that fits inside, centred.
    Rect fitAspect(int aw, int ah) const {
        if (aw <= 0 || ah <= 0 || empty()) return {x, y, 0, 0};
        int fw = w;
        int fh = int(long(w) * ah / aw);
        if (fh > h) {
            fh = h;
            fw = int(long(h) * aw / ah);
        }
        return {x + (w - fw) / 2, y + (h - fh) / 2, fw, fh};
    }
};
