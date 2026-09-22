// Host-side checks for the damage tracking that drives incremental redrawing.
//
// The invariant that matters: whatever a primitive paints, it must report — otherwise stale
// pixels survive on screen because they are never transferred. This is hard to spot by eye
// on the device, so it is checked here instead.
//
// Build and run:  make -f tests/Makefile

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

#include "../src/Canvas.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

bool covers(const std::vector<Rect> &damage, const Rect &r) {
    for (const Rect &d : damage) {
        if (d.x <= r.x && d.y <= r.y && d.right() >= r.right() && d.bottom() >= r.bottom())
            return true;
    }
    return false;
}

// Every pixel that differs from the reference must sit inside a reported rectangle.
bool everyChangeReported(const Canvas &before, const Canvas &after,
                         const std::vector<Rect> &damage) {
    for (int y = 0; y < after.height(); ++y) {
        const uint32_t *a = before.row(y);
        const uint32_t *b = after.row(y);
        for (int x = 0; x < after.width(); ++x) {
            if (a[x] == b[x]) continue;
            bool inside = false;
            for (const Rect &d : damage) {
                if (d.contains(x, y)) { inside = true; break; }
            }
            if (!inside) {
                std::printf("  pixel %d,%d was changed but not reported\n", x, y);
                return false;
            }
        }
    }
    return true;
}

bool identical(const Canvas &a, const Canvas &b) {
    if (a.width() != b.width() || a.height() != b.height()) return false;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.row(y)[x] != b.row(y)[x]) {
                std::printf("  difference at %d,%d: %08X against %08X\n", x, y, a.row(y)[x],
                            b.row(y)[x]);
                return false;
            }
        }
    }
    return true;
}

Canvas makeBackground(int w, int h) {
    Canvas background(w, h);
    background.verticalGradient(background.bounds(), Color::rgb(0x0B0D14),
                                Color::rgb(0x05070C));
    background.clearDamage();
    return background;
}

} // namespace

int main() {
    constexpr int kW = 320;
    constexpr int kH = 240;

    // 1. A plain rectangle reports exactly its own area.
    {
        Canvas c(kW, kH);
        c.clearDamage();
        c.fillRect({10, 20, 30, 40}, Color::rgb(0xFF0000));
        check(c.damage().size() == 1, "rectangle reports exactly one region");
        check(covers(c.damage(), {10, 20, 30, 40}), "rectangle reports its own area");
    }

    // 2. Distant rectangles stay separate; neighbours merge.
    {
        Canvas c(kW, kH);
        c.clearDamage();
        c.fillRect({0, 0, 10, 10}, Color::rgb(0xFF0000));
        c.fillRect({200, 200, 10, 10}, Color::rgb(0xFF0000));
        check(c.damage().size() == 2, "distant regions stay separate");

        c.clearDamage();
        c.fillRect({0, 0, 10, 10}, Color::rgb(0xFF0000));
        c.fillRect({12, 0, 10, 10}, Color::rgb(0xFF0000));
        check(c.damage().size() == 1, "neighbouring regions are merged");
    }

    // 3. The list stays bounded no matter how much is drawn.
    {
        Canvas c(kW, kH);
        c.clearDamage();
        for (int i = 0; i < 200; ++i) c.fillRect({(i * 7) % 300, (i * 11) % 220, 4, 4},
                                                 Color::rgb(0x00FF00));
        check(c.damage().size() <= 12, "list stays bounded");
        check(!c.damage().empty(), "list is not empty");
    }

    // 4. The antialiased primitives report everything they touch — the case that would leave
    //    stale pixels behind, because they paint pixel by pixel.
    {
        Canvas background = makeBackground(kW, kH);
        Canvas c(kW, kH);
        c.restoreFrom(background, c.bounds());
        c.clearDamage();

        Canvas reference(kW, kH);
        reference.restoreFrom(background, reference.bounds());

        c.fillRoundedRect({40, 30, 120, 90}, 12, Color::rgb(0x4C8DFF));
        check(everyChangeReported(reference, c, c.damage()),
              "rounded rectangle reports every changed pixel");

        c.clearDamage();
        Canvas reference2(kW, kH);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
                const_cast<uint32_t *>(reference2.row(y))[x] = c.row(y)[x];

        c.strokeRoundedRect({200, 50, 80, 60}, 10, 3, Color::rgb(0xFFC65C));
        check(everyChangeReported(reference2, c, c.damage()),
              "outline reports every changed pixel");

        c.clearDamage();
        Canvas reference3(kW, kH);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
                const_cast<uint32_t *>(reference3.row(y))[x] = c.row(y)[x];

        c.dropShadow({100, 140, 90, 60}, 10, 16, Color{0, 0, 0, 170});
        check(everyChangeReported(reference3, c, c.damage()),
              "shadow reports every changed pixel");
    }

    // 5. The core promise: drawing, then restoring the background over the reported area,
    //    must return the canvas to exactly its previous state. If it does not, incremental
    //    redrawing leaves debris.
    {
        Canvas background = makeBackground(kW, kH);

        Canvas pristine(kW, kH);
        pristine.restoreFrom(background, pristine.bounds());

        Canvas c(kW, kH);
        c.restoreFrom(background, c.bounds());
        c.clearDamage();

        c.fillRoundedRect({40, 30, 120, 90}, 12, Color::rgb(0x4C8DFF));
        c.strokeRoundedRect({40, 30, 120, 90}, 12, 3, Color::rgb(0xFFC65C));
        c.dropShadow({40, 30, 120, 90}, 12, 16, Color{0, 0, 0, 170});

        const std::vector<Rect> painted = c.damage();
        for (const Rect &r : painted) c.restoreFrom(background, r);

        check(identical(pristine, c),
              "restoring the reported area returns the original state");
    }

    // 6. The footprint a screen reserves around a tile must contain everything the tile
    //    paints when focused. Too small, and neighbouring tiles keep stale pixels; the
    //    numbers here mirror Tile::draw and SystemsScreen at 1080p.
    {
        const int radius = 10;      // theme.radius()
        const int shadow = 16;      // theme.px(16), the focused shadow spread
        const int border = 3;       // theme.px(3), the focus outline

        const Rect cell{60, 40, 180, 135};
        // Tile grows by 10 % when focused, centred on its cell.
        const int dw = int(cell.w * 0.10f), dh = int(cell.h * 0.10f);
        const Rect body{cell.x - dw / 2, cell.y - dh / 2, cell.w + dw, cell.h + dh};

        // Same derivation as Tile::footprint, which the screens rely on.
        const Rect sb = Canvas::shadowBounds(body, shadow);
        const Rect reserved{std::min(body.x, sb.x) - border, std::min(body.y, sb.y) - border,
                            std::max(body.right(), sb.right()) + border -
                                (std::min(body.x, sb.x) - border),
                            std::max(body.bottom(), sb.bottom()) + border -
                                (std::min(body.y, sb.y) - border)};

        Canvas background = makeBackground(kW, kH);
        Canvas c(kW, kH);
        c.restoreFrom(background, c.bounds());
        c.clearDamage();

        c.dropShadow(body, radius, shadow, Color{0, 0, 0, 170});
        c.fillRoundedRect(body, radius, Color::rgb(0x161A26));
        c.strokeRoundedRect(body, radius, border, Color::rgb(0x4C8DFF));

        bool inside = true;
        for (const Rect &d : c.damage()) {
            if (d.x < reserved.x || d.y < reserved.y || d.right() > reserved.right() ||
                d.bottom() > reserved.bottom()) {
                std::printf("  reported %d,%d %dx%d lies outside of %d,%d %dx%d\n", d.x,
                            d.y, d.w, d.h, reserved.x, reserved.y, reserved.w, reserved.h);
                inside = false;
            }
        }
        check(inside, "safety margin around a tile is sufficient");
    }

    std::printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
    return failures ? 1 : 0;
}
