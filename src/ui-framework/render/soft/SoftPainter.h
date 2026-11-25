#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "ui-framework/core/Painter.h"
#include "ui-framework/render/soft/Canvas.h"
#include "ui-framework/render/soft/Font.h"

namespace ui {

// The CPU renderer: a Painter on top of the software Canvas and the FreeType Font.
class SoftPainter : public Painter {
public:
    SoftPainter(Canvas &canvas, Font &regular, Font &bold) : canvas_(canvas), regular_(regular), bold_(bold) {}

    Canvas &canvas() { return canvas_; }

    Rect bounds() const override { return canvas_.bounds(); }
    Capabilities caps() const override { Capabilities c; c.damageTracking = true; return c; }

    void pushClip(const Rect &r) override { canvas_.pushClip(r); }
    void popClip() override { canvas_.popClip(); }

    void clear(Color c) override { canvas_.clear(c); }
    void fillRect(const Rect &r, Color c) override { if (recorder_) cover(r, c); canvas_.fillRect(r, c); }
    void fillRoundedRect(const Rect &r, int radius, Color c) override { if (recorder_) cover(r.inset(radius / 3), c); canvas_.fillRoundedRect(r, radius, c); }
    void strokeRoundedRect(const Rect &r, int radius, int thickness, Color c) override {
        canvas_.strokeRoundedRect(r, radius, thickness, c);
    }
    void fillGradient(const Rect &r, Color top, Color bottom) override { canvas_.verticalGradient(r, top, bottom); }
    void drawLine(int x0, int y0, int x1, int y1, int thickness, Color c) override;
    void drawImage(const Image &img, const Rect &dst, uint8_t alpha, int radius) override {
        canvas_.drawImage(img, dst, alpha, radius);
    }
    void drawMask(const Image &mask, int x, int y, Color c) override;
    void drawText(FontRole role, int px, int x, int y, const std::string &text, Color c) override {
        if (recorder_) record(role, px, x, y, text);
        font(role).draw(canvas_, x, y, text, px, c);
    }

    int lineHeight(FontRole role, int px) override { return font(role).lineHeight(px); }
    int measure(FontRole role, int px, const std::string &text) override { return font(role).measure(text, px); }
    std::string elide(FontRole role, int px, const std::string &text, int maxWidth) override {
        std::string r = font(role).elide(text, px, maxWidth);
        if (recorder_ && r != text) elided_.push_back(r);
        return r;
    }

    // Optional recording of every text run drawn (layout checks of the host runner; off by default, costs nothing then).
    // `box` is the run's full rectangle, `clip` the clip in force, `elided` is true when the text is the result of an elide() that cut it.
    struct TextRun {
        Rect box, clip;
        std::string text;
        bool elided = false;
        bool covered = false;   // an opaque fill drawn later hides it all (the page behind a dialog)
        Rect shown;             // the part of box inside the clip that no later opaque fill hides (only a trimmed rectangle)
    };
    void setTextRecorder(std::vector<TextRun> *runs) { recorder_ = runs; elided_.clear(); }

private:
    void cover(const Rect &r, Color c) {
        if (c.a != 255) return;
        const Rect f = r.intersect(canvas_.clip());
        for (TextRun &t : *recorder_) {
            Rect &v = t.shown;
            if (v.empty() || f.empty()) continue;
            const bool rows = f.y <= v.y && f.bottom() >= v.bottom(), cols = f.x <= v.x && f.right() >= v.right();
            if (rows && f.x <= v.x && f.right() > v.x) { const int cut = std::min(f.right(), v.right()) - v.x; v.x += cut; v.w -= cut; }
            else if (rows && f.right() >= v.right() && f.x < v.right()) v.w = f.x - v.x;
            else if (cols && f.y <= v.y && f.bottom() > v.y) { const int cut = std::min(f.bottom(), v.bottom()) - v.y; v.y += cut; v.h -= cut; }
            else if (cols && f.bottom() >= v.bottom() && f.y < v.bottom()) v.h = f.y - v.y;
            if (v.w <= 0 || v.h <= 0) { v = Rect{}; t.covered = true; }
        }
    }
    void record(FontRole role, int px, int x, int y, const std::string &text) {
        TextRun run;
        run.box = Rect{x, y, font(role).measure(text, px), font(role).lineHeight(px)};
        run.clip = canvas_.clip();
        run.shown = run.box.intersect(run.clip);
        run.text = text;
        for (const std::string &e : elided_) if (e == text) { run.elided = true; break; }
        recorder_->push_back(run);
    }
    std::vector<TextRun> *recorder_ = nullptr;
    std::vector<std::string> elided_;

    Font &font(FontRole role) { return role == FontRole::Bold ? bold_ : regular_; }

    Canvas &canvas_;
    Font &regular_;
    Font &bold_;
};

}  // namespace ui
