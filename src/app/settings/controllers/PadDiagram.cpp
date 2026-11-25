#include "app/settings/controllers/PadDiagram.h"

#include <algorithm>
#include <cstdio>

#include "ui-framework/kit/Input.h"

namespace patsgui {
namespace controllers {

namespace {

using ui::Color;
using ui::Rect;

std::string textOf(const char *key) { return ui::Text::key(key).str(); }

// A scale factor applied to design pixels so the drawing fits a short area.
struct Scale {
    const ui::Theme &t;
    float k;
    int operator()(int designPx) const { return std::max(1, int(float(t.px(designPx)) * k + 0.5f)); }
};

}  // namespace

// ------------------------------------------------------------------------------------------------ wizard diagram

void drawWizardDiagram(ui::Painter &p, const ui::Theme &t, const Rect &area, PadHighlight highlight) {
    const ui::ButtonColors lit = ui::readyButtonColors(t.palette);
    const Color dim = t.palette.surface.withAlpha(160);

    auto drawPart = [&](const Rect &frame, int radius, PadHighlight self, const std::string &label) {
        const bool on = self == highlight;
        p.fillRoundedRect(frame, radius, on ? lit.fill : dim);
        // The outline keeps a part visible on a light panel, where the dim fill is nearly the panel's own colour.
        if (!on) p.strokeRoundedRect(frame, radius, std::max(1, t.px(1)), t.palette.textMuted.withAlpha(150));
        ui::drawLabelColor(p, t, frame, label, ui::TextRole::Small, on ? lit.text : t.palette.textMuted, ui::Align::Center);
    };

    // A schematic, not a likeness: two clusters (D-pad left, face buttons right) with shoulders, Select/Start and sticks
    // around them, capped at a fixed width so it stays a sensible size on a wide screen.
    const int diagramWidth = std::min(area.w, t.px(560));
    const int x0 = area.x + (area.w - diagramWidth) / 2;
    const int dpadCx = x0 + t.px(110);
    const int faceCx = x0 + diagramWidth - t.px(110);
    const int clusterCy = area.y + t.px(150);

    const int shoulderY = area.y, shoulderW = t.px(120), shoulderH = t.px(40);
    drawPart({dpadCx - shoulderW / 2, shoulderY, shoulderW, shoulderH}, t.px(10), PadHighlight::L, textOf("settings.controllers.pad.l"));
    drawPart({faceCx - shoulderW / 2, shoulderY, shoulderW, shoulderH}, t.px(10), PadHighlight::R, textOf("settings.controllers.pad.r"));

    // Home/Guide: top centre, where every modern pad puts its system button (the common answer for "open the menu").
    const int midCxTop = (dpadCx + faceCx) / 2;
    const int homeSize = t.px(44);
    drawPart({midCxTop - homeSize / 2, shoulderY, homeSize, homeSize}, homeSize / 2, PadHighlight::Home,
             textOf("settings.controllers.pad.home"));

    const int dpadStep = t.px(48), dpadSize = t.px(44);
    drawPart({dpadCx - dpadSize / 2, clusterCy - dpadStep - dpadSize / 2, dpadSize, dpadSize}, t.px(8), PadHighlight::DpadUp,
             textOf("settings.controllers.pad.up"));
    drawPart({dpadCx - dpadSize / 2, clusterCy + dpadStep - dpadSize / 2, dpadSize, dpadSize}, t.px(8), PadHighlight::DpadDown,
             textOf("settings.controllers.pad.down"));
    drawPart({dpadCx - dpadStep - dpadSize / 2, clusterCy - dpadSize / 2, dpadSize, dpadSize}, t.px(8), PadHighlight::DpadLeft,
             textOf("settings.controllers.pad.left"));
    drawPart({dpadCx + dpadStep - dpadSize / 2, clusterCy - dpadSize / 2, dpadSize, dpadSize}, t.px(8), PadHighlight::DpadRight,
             textOf("settings.controllers.pad.right"));

    // The Xbox positions (Y top, X left, B right, A bottom); the names follow the layout setting.
    const int faceR = t.px(60), faceSize = t.px(48);
    auto faceFrame = [&](int cx, int cy) { return Rect{cx - faceSize / 2, cy - faceSize / 2, faceSize, faceSize}; };
    const ui::ButtonLayout layout = t.buttonLayout();
    drawPart(faceFrame(faceCx, clusterCy - faceR), faceSize / 2, PadHighlight::FaceTop, ui::buttonName(layout, ui::PadButton::Y));
    drawPart(faceFrame(faceCx - faceR, clusterCy), faceSize / 2, PadHighlight::FaceLeft, ui::buttonName(layout, ui::PadButton::X));
    drawPart(faceFrame(faceCx + faceR, clusterCy), faceSize / 2, PadHighlight::FaceRight, ui::buttonName(layout, ui::PadButton::B));
    drawPart(faceFrame(faceCx, clusterCy + faceR), faceSize / 2, PadHighlight::FaceBottom, ui::buttonName(layout, ui::PadButton::A));

    const int midY = clusterCy + faceR + t.px(30);
    const int midCx = (dpadCx + faceCx) / 2;
    const int midW = t.px(110), midH = t.px(40);
    drawPart({midCx - midW - t.px(10), midY, midW, midH}, t.px(10), PadHighlight::Select, ui::buttonName(layout, ui::PadButton::Select));
    drawPart({midCx + t.px(10), midY, midW, midH}, t.px(10), PadHighlight::Start, ui::buttonName(layout, ui::PadButton::Start));

    const int stickY = midY + midH + t.px(30);
    const int stickSize = t.px(70);

    // A stick step is "left/right" or "up/down": a bar through the lit circle points along the requested axis.
    // The caption under each circle names it ("Analog left", "Analog right"): the owner did not know what to do with the
    // two plain circles at the bottom.
    auto drawStick = [&](int cx, PadHighlight horizontalH, PadHighlight verticalH, const char *captionKey) {
        const bool horizontal = highlight == horizontalH;
        const bool vertical = highlight == verticalH;
        const Rect frame{cx - stickSize / 2, stickY, stickSize, stickSize};
        drawPart(frame, stickSize / 2, horizontal ? horizontalH : verticalH, "");
        const int captionH = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Small)) + t.px(4);
        ui::drawLabelColor(p, t, {cx - t.px(110), frame.bottom() + t.px(6), t.px(220), captionH}, textOf(captionKey),
                           ui::TextRole::Small, t.palette.textPrimary, ui::Align::Center);
        if (!horizontal && !vertical) return;
        const int thickness = t.px(10), inset = t.px(10);
        const Color bar = lit.text;
        if (horizontal)
            p.fillRoundedRect({frame.x + inset, frame.y + frame.h / 2 - thickness / 2, frame.w - 2 * inset, thickness}, thickness / 2, bar);
        else
            p.fillRoundedRect({frame.x + frame.w / 2 - thickness / 2, frame.y + inset, thickness, frame.h - 2 * inset}, thickness / 2, bar);
    };
    drawStick(dpadCx, PadHighlight::Stick1Horizontal, PadHighlight::Stick1Vertical, "settings.controllers.pad.analogLeft");
    drawStick(faceCx, PadHighlight::Stick2Horizontal, PadHighlight::Stick2Vertical, "settings.controllers.pad.analogRight");
}

// ------------------------------------------------------------------------------------------------ live pad

void drawInputTestPad(ui::Painter &p, const ui::Theme &t, const Rect &area, const PadSnapshot &pad) {
    // Classic's layout height in design pixels (the generic-pad raw line included), shrunk to fit a short area.
    const int needed = t.px(740);
    const float k = area.h >= needed || needed <= 0 ? 1.0f : std::max(0.5f, float(area.h) / float(needed));
    const Scale S{t, k};
    const ui::ButtonColors lit = ui::readyButtonColors(t.palette);
    const Color unlitFill = t.palette.surfaceHi;
    const Color outlineLit = t.palette.focus, outlineDim = t.palette.focus.withAlpha(90);
    const ui::ButtonLayout layout = t.buttonLayout();

    auto drawButton = [&](int x, int y, int w, const std::string &label, bool pressed) {
        const Rect frame{x, y, w, S(40)};
        p.fillRoundedRect(frame, S(8), pressed ? lit.fill : unlitFill);
        p.strokeRoundedRect(frame, S(8), S(2), pressed ? outlineLit : outlineDim);
        ui::drawLabelColor(p, t, frame, label, ui::TextRole::Small, pressed ? lit.text : t.palette.textPrimary, ui::Align::Center);
    };

    const int colWidth = S(170), gap = S(34);
    const int groupWidth = colWidth * 3 + gap * 2;
    const int bodyX = area.x + std::max(0, (area.w - groupWidth) / 2);
    int y = area.y + S(24);

    // D-pad: lit by a BTN_DPAD_* key, a hat's digital edge, or (a pad without gamepad buttons) an axis direction.
    const bool axisPad = pad.genericPad;
    const int axisH = axisPad ? pad.axisDirX : 0, axisV = axisPad ? pad.axisDirY : 0;
    auto key = [&](uint16_t c) { return pad.keys[c]; };
    auto logical = [&](uint16_t c) { return pad.logicalKeys[c]; };
    const bool dpadUp = key(code::kBtnDpadUp) || hatEdge(pad.hats[1]) == 1 || axisV < 0;
    const bool dpadDown = key(code::kBtnDpadDown) || hatEdge(pad.hats[1]) == 2 || axisV > 0;
    const bool dpadLeft = key(code::kBtnDpadLeft) || hatEdge(pad.hats[0]) == 1 || axisH < 0;
    const bool dpadRight = key(code::kBtnDpadRight) || hatEdge(pad.hats[0]) == 2 || axisH > 0;
    const int arrowSize = S(54), arrowGap = S(6);
    const int dpadCenterX = bodyX + (colWidth - arrowSize) / 2;
    const int dpadY = y + S(50);
    drawButton(dpadCenterX, dpadY - arrowSize - arrowGap, arrowSize, "\xE2\x86\x91", dpadUp);
    drawButton(dpadCenterX - arrowSize - arrowGap, dpadY, arrowSize, "\xE2\x86\x90", dpadLeft);
    drawButton(dpadCenterX + arrowSize + arrowGap, dpadY, arrowSize, "\xE2\x86\x92", dpadRight);
    drawButton(dpadCenterX, dpadY + arrowSize + arrowGap, arrowSize, "\xE2\x86\x93", dpadDown);

    // The face buttons at the physical Xbox positions (this pad reports BTN_NORTH for its left button, BTN_WEST for the top
    // one, as Input does); the names follow the layout.
    const int faceX = bodyX + colWidth + gap;
    const int diamondCx = faceX + colWidth + gap / 2;
    const int diamondCy = y + S(110);
    const int diamondR = S(82), faceSize = S(64);
    auto drawFace = [&](int cx, int cy, const std::string &label, bool pressed) {
        const Rect frame{cx - faceSize / 2, cy - faceSize / 2, faceSize, faceSize};
        p.fillRoundedRect(frame, faceSize / 2, pressed ? lit.fill : unlitFill);
        p.strokeRoundedRect(frame, faceSize / 2, S(2), pressed ? outlineLit : outlineDim);
        ui::drawLabelColor(p, t, frame, label, ui::TextRole::Body, pressed ? lit.text : t.palette.textPrimary, ui::Align::Center);
    };
    drawFace(diamondCx, diamondCy - diamondR, ui::buttonName(layout, ui::PadButton::Y), logical(code::kBtnWest));
    drawFace(diamondCx - diamondR, diamondCy, ui::buttonName(layout, ui::PadButton::X), logical(code::kBtnNorth));
    drawFace(diamondCx + diamondR, diamondCy, ui::buttonName(layout, ui::PadButton::B), logical(code::kBtnEast));
    drawFace(diamondCx, diamondCy + diamondR, ui::buttonName(layout, ui::PadButton::A), logical(code::kBtnSouth));

    // Shoulders (the face diamond ends 224 px below its top).
    y += S(268);
    drawButton(bodyX, y, colWidth, textOf("settings.controllers.pad.l"), logical(code::kBtnTL));
    drawButton(faceX, y, colWidth, textOf("settings.controllers.pad.r"), logical(code::kBtnTR));

    // Triggers: a fill bar when the pad reports an analogue axis, else a digital button.
    auto drawTrigger = [&](int x, const std::string &label, const AxisReading &axis, uint16_t digital) {
        if (axis.present && axis.maximum > axis.minimum) {
            const float fraction = std::max(0.0f, std::min(1.0f, float(axis.value - axis.minimum) / float(axis.maximum - axis.minimum)));
            const Rect frame{x, y + S(52), colWidth, S(28)};
            p.fillRoundedRect(frame, S(6), unlitFill);
            p.fillRoundedRect({frame.x, frame.y, int(frame.w * fraction), frame.h}, S(6), lit.fill);
            ui::drawLabelColor(p, t, frame, label, ui::TextRole::Small, fraction > 0.5f ? lit.text : t.palette.textPrimary, ui::Align::Center);
        } else {
            drawButton(x, y + S(52), colWidth, label, logical(digital));
        }
    };
    drawTrigger(bodyX, ui::buttonName(layout, ui::PadButton::TriggerLeft), pad.axes[code::kAbsZ], code::kBtnTL2);
    drawTrigger(faceX, ui::buttonName(layout, ui::PadButton::TriggerRight), pad.axes[code::kAbsRZ], code::kBtnTR2);

    // Select / Start / Home
    y += S(112);
    drawButton(bodyX, y, colWidth, ui::buttonName(layout, ui::PadButton::Select), logical(code::kBtnSelect));
    drawButton(faceX, y, colWidth, ui::buttonName(layout, ui::PadButton::Start), logical(code::kBtnStart));
    drawButton(faceX + colWidth + gap, y, colWidth, textOf("settings.controllers.pad.home"), key(code::kBtnMode));

    // Sticks: a circle with a dot at the live position.
    y += S(92);
    const int captionRoom = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body)) * 2 + S(20);   // caption + raw line
    const int stickSize = std::min(S(170), std::max(S(60), area.bottom() - y - captionRoom));
    auto drawStick = [&](int x, const AxisReading &ax, const AxisReading &ay, const char *captionKey, bool showRaw) {
        const Rect frame{x, y, stickSize, stickSize};
        p.strokeRoundedRect(frame, stickSize / 2, S(3), t.palette.focus.withAlpha(175));
        if (ax.present && ay.present && ax.maximum > ax.minimum && ay.maximum > ay.minimum) {
            const float nx = (float(ax.value - ax.minimum) / float(ax.maximum - ax.minimum)) * 2.0f - 1.0f;
            const float ny = (float(ay.value - ay.minimum) / float(ay.maximum - ay.minimum)) * 2.0f - 1.0f;
            const int dot = S(20);
            const int cx = frame.x + frame.w / 2 + int(nx * float(frame.w / 2 - dot / 2));
            const int cy = frame.y + frame.h / 2 + int(ny * float(frame.h / 2 - dot / 2));
            p.fillRoundedRect({cx - dot / 2, cy - dot / 2, dot, dot}, dot / 2, t.palette.accent);
        }
        const int captionH = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body));
        ui::drawLabel(p, t, {frame.x, frame.bottom() + S(8), colWidth, captionH}, textOf(captionKey), ui::TextRole::Body,
                      ui::TextTier::Muted);
        // A pad with no gamepad buttons often has its D-pad on this stick, with an odd range: show what it really reports.
        if (showRaw && pad.genericPad && ax.present && ay.present) {
            const std::string raw = ui::fillTemplate(textOf("settings.controllers.test.raw"),
                {std::to_string(ax.value), std::to_string(ax.minimum), std::to_string(ax.maximum),
                 std::to_string(ay.value), std::to_string(ay.minimum), std::to_string(ay.maximum)});
            ui::drawLabel(p, t, {frame.x, frame.bottom() + S(8) + captionH + S(4), groupWidth, captionH}, raw, ui::TextRole::Small,
                          ui::TextTier::Muted);
        }
    };
    drawStick(bodyX, pad.axes[code::kAbsX], pad.axes[code::kAbsY], "settings.controllers.test.stick1", true);
    drawStick(faceX + colWidth + gap, pad.axes[code::kAbsRX], pad.axes[code::kAbsRY], "settings.controllers.test.stick2", false);
}

}  // namespace controllers
}  // namespace patsgui
