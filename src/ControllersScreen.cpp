#include "ControllersScreen.h"

#include <algorithm>
#include <cstdio>
#include <linux/input.h>

#include "Canvas.h"
#include "ControllerId.h"
#include "MisterIni.h"

namespace {

// Every evdev key code this project's wizard is willing to capture as an answer: the
// generic joystick-class range (older/simpler pads), the modern gamepad range, and the
// digital D-pad keys some pads send instead of (or alongside) a hat. Scanned as raw numbers
// rather than named BTN_* constants because several of those names alias the same code
// (BTN_A == BTN_SOUTH, etc.) — the pad's own driver picks one name, this only cares about
// the number that actually arrives.
const std::vector<uint16_t> &candidateButtonCodes() {
    static const std::vector<uint16_t> codes = [] {
        std::vector<uint16_t> c;
        for (uint16_t code = 0x120; code <= 0x13e; ++code) c.push_back(code); // BTN_TRIGGER..BTN_THUMBR
        c.push_back(BTN_DPAD_UP);
        c.push_back(BTN_DPAD_DOWN);
        c.push_back(BTN_DPAD_LEFT);
        c.push_back(BTN_DPAD_RIGHT);
        return c;
    }();
    return codes;
}

// Stick axes only — ABS_Z/ABS_RZ are excluded deliberately, same as Input.cpp's own trigger
// detection: those two carry analogue triggers on plenty of pads, and this project has
// nowhere in the .map format's slot table to put a captured trigger yet (see CONTROLLER.md,
// "Also not accounted for: analogue triggers" — flagged there as needing its own look, not
// guessed at here).
const uint16_t kStickAxisCodes[4] = {ABS_X, ABS_Y, ABS_RX, ABS_RY};

// Main_MiSTer/input.h: `#define KEY_EMU (KEY_MAX+1)` — the base MiSTer's own input.cpp adds
// a hat/axis code to (doubled, plus a direction bit) to synthesise an EV_KEY-shaped code for
// something that isn't really a key at all. Reproduced here, not included from that project,
// since only this one constant is needed and it can never change without breaking every
// `.map` file already written by any MiSTer on earth.
constexpr uint32_t kKeyEmu = uint32_t(KEY_MAX) + 1;

} // namespace

ControllersScreen::ControllersScreen(Context &context, Input &input,
                                     std::function<void()> onClose)
    : context_(context), input_(input), onClose_(std::move(onClose)) {}

void ControllersScreen::refresh() {
    mode_ = Mode::List;
    cursor_ = 0;
    bluetoothAvailable_ = Bluetooth::adapterPresent();
    rebuildRows();
}

void ControllersScreen::rebuildRows() {
    rows_.clear();
    for (const Input::DeviceInfo &device : input_.listDevices()) {
        Row row;
        row.device = device;
        row.idstr = ControllerId::idstr(device.vendor, device.product);
        row.vidPid = ControllerId::vidPid(device.vendor, device.product);
        rows_.push_back(row);
    }

    for (size_t i = 0; i < rows_.size(); ++i) {
        for (size_t j = 0; j < rows_.size(); ++j) {
            if (i != j && rows_[i].idstr == rows_[j].idstr) rows_[i].sharesMapping = true;
        }

        rows_[i].hasDeadzone = MisterIni::readDeadzone(rows_[i].vidPid, rows_[i].deadzoneValue);
        ControllerMap::Slots mapped;
        rows_[i].hasMapping = ControllerMap::read(rows_[i].idstr, mapped);
    }

    if (cursor_ >= int(rows_.size())) cursor_ = std::max(0, int(rows_.size()) - 1);
}

const ControllersScreen::Row *ControllersScreen::selectedRow() const {
    if (cursor_ < 0 || cursor_ >= int(rows_.size())) return nullptr;
    return &rows_[size_t(cursor_)];
}

int ControllersScreen::currentSlot(const Row &row) const {
    return input_.findSlot(row.device.eventNumber);
}

bool ControllersScreen::wantsRawInput() const {
    return mode_ == Mode::Wizard && !wizardOpeningShown_;
}

// ---------------------------------------------------------------------------------------
// Top-level dispatch
// ---------------------------------------------------------------------------------------

void ControllersScreen::update(float deltaSeconds) {
    if (mode_ == Mode::InputTest) updateInputTest(deltaSeconds);
    else if (mode_ == Mode::Wizard) updateWizard(deltaSeconds);
    else if (mode_ == Mode::Bluetooth) updateBluetooth(deltaSeconds);
}

void ControllersScreen::handle(Action action) {
    switch (mode_) {
    case Mode::List: handleListAction(action); return;
    case Mode::InputTest:
        // Deliberately does nothing: every input here is meant to just light up in the
        // display, including whatever a pad happens to send as Back/Confirm/etc. — the only
        // way out is holding Start for 5 seconds, handled in updateInputTest() from raw
        // state, not through this Action path at all.
        return;
    case Mode::Deadzone: handleDeadzoneAction(action); return;
    case Mode::Wizard:
        // Only the opening confirmation screen (before any capture starts) uses the
        // ordinary Action path — see CONTROLLER.md, "Button mapping": once capturing
        // begins, only a raw evdev event (routed through updateWizard(), not here) can mean
        // anything, precisely so an in-progress capture step's own button press is never
        // also misread as this project's own Back/Confirm.
        if (wizardOpeningShown_) {
            if (action == Action::Confirm) {
                wizardOpeningShown_ = false;
                // The very press that just fired this Confirm (physically still held for a
                // few more milliseconds on real hardware) must not bleed into step 0's own
                // capture as if it were an answer — see primeButtonBaseline().
                primeButtonBaseline();
            } else if (action == Action::Back) {
                mode_ = Mode::List;
            }
        }
        return;
    case Mode::Bluetooth: handleBluetoothAction(action); return;
    }
}

void ControllersScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    switch (mode_) {
    case Mode::List: renderList(canvas, area); return;
    case Mode::InputTest: renderInputTest(canvas, area); return;
    case Mode::Deadzone: renderDeadzone(canvas, area); return;
    case Mode::Wizard: renderWizard(canvas, area); return;
    case Mode::Bluetooth: renderBluetooth(canvas, area); return;
    }
}

std::string ControllersScreen::hints() const {
    switch (mode_) {
    case Mode::List:
        return "A Input test   X Deadzone   Y Button mapping   B Back";
    case Mode::InputTest:
        return "Every input lights up below — hold Start 5s to leave";
    case Mode::Deadzone:
        return "Left/Right Adjust   A Save   B Cancel";
    case Mode::Wizard:
        if (wizardOpeningShown_) return "A Begin   B Cancel, nothing captured yet";
        return "Start: press = skip this button, hold 5s = cancel wizard, nothing saved";
    case Mode::Bluetooth:
        switch (btState_) {
        case BtState::Idle: return "A Start scanning   B Close";
        case BtState::Scanning: return "B Cancel";
        case BtState::Found: return "A Set up mapping now   B Skip for now";
        case BtState::NoBluetooth: return "B Close";
        case BtState::Timeout: return "A Try again   B Close";
        }
    }
    return "B Back";
}

// ---------------------------------------------------------------------------------------
// 1. Controller list
// ---------------------------------------------------------------------------------------

void ControllersScreen::handleListAction(Action action) {
    const int rowCount = int(rows_.size());
    const int itemCount = rowCount + (bluetoothAvailable_ ? 1 : 0);

    switch (action) {
    case Action::Up:
        if (cursor_ > 0) --cursor_;
        break;
    case Action::Down:
        if (cursor_ + 1 < itemCount) ++cursor_;
        break;
    case Action::Back:
        onClose_();
        break;
    case Action::Confirm:
        if (cursor_ == rowCount && bluetoothAvailable_) enterBluetooth();
        else if (selectedRow()) enterInputTest();
        break;
    case Action::FaceXPress: // physical X — deadzone needs no hold
        if (const Row *row = selectedRow()) {
            int32_t lo, hi;
            if (input_.absInfo(currentSlot(*row), ABS_X, lo, hi)) enterDeadzone();
            else context_.notify("This controller has no analogue stick");
        }
        break;
    case Action::CycleView: // physical Y — button mapping
        if (selectedRow()) beginWizard();
        break;
    default:
        break;
    }
}

void ControllersScreen::renderList(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;

    theme.bold().draw(canvas, area.x, area.y, "Controllers", theme.sizeHeading(),
                      theme.textPrimary);

    const int headerHeight = theme.px(58);
    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    if (rows_.empty() && !bluetoothAvailable_) {
        theme.regular().drawCentered(canvas, body, "No controllers connected", theme.sizeHeading(),
                                     theme.textMuted);
        return;
    }

    const int listWidth = std::min(body.w * 45 / 100, theme.px(560));
    const Rect list{body.x, body.y, listWidth, body.h};
    const Rect detail{list.right() + theme.gap(), body.y, body.w - listWidth - theme.gap(),
                      body.h};

    const int rowHeight = theme.px(58);
    for (size_t i = 0; i < rows_.size(); ++i) {
        const Row &row = rows_[i];
        const Rect frame{list.x, list.y + int(i) * rowHeight, list.w, rowHeight - theme.px(8)};
        if (frame.bottom() > list.bottom()) break;

        const bool active = int(i) == cursor_;
        canvas.fillRoundedRect(frame, theme.px(8),
                               active ? theme.surfaceHi : theme.surface.withAlpha(150));
        if (active)
            canvas.fillRoundedRect({frame.x, frame.y, theme.px(4), frame.h}, theme.px(2),
                                   theme.accent);

        const int textY = frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
        theme.regular().draw(canvas, frame.x + theme.px(20), textY, row.device.name,
                             theme.sizeBody(), active ? theme.textPrimary : theme.textMuted);

        if (row.sharesMapping) {
            const char *dot = "*";
            const int dotWidth = theme.bold().measure(dot, theme.sizeBody());
            theme.bold().draw(canvas, frame.right() - theme.px(20) - dotWidth, textY, dot,
                              theme.sizeBody(), theme.accent);
        }
    }

    if (bluetoothAvailable_) {
        const size_t i = rows_.size();
        const Rect frame{list.x, list.y + int(i) * rowHeight, list.w, rowHeight - theme.px(8)};
        if (frame.bottom() <= list.bottom()) {
            const bool active = int(i) == cursor_;
            canvas.fillRoundedRect(frame, theme.px(8),
                                   active ? theme.surfaceHi : theme.surface.withAlpha(110));
            if (active)
                canvas.fillRoundedRect({frame.x, frame.y, theme.px(4), frame.h}, theme.px(2),
                                       theme.accent);
            const int textY =
                frame.y + (frame.h - theme.regular().lineHeight(theme.sizeBody())) / 2;
            theme.regular().draw(canvas, frame.x + theme.px(20), textY,
                                 "Connect new controller via Bluetooth", theme.sizeBody(),
                                 active ? theme.accent : theme.textMuted);
        }
    }

    canvas.fillRoundedRect(detail, theme.radius(), theme.surface.withAlpha(110));
    const Rect body2 = detail.inset(theme.px(26));
    int y = body2.y;
    const int lineStep = theme.regular().lineHeight(theme.sizeBody()) + theme.px(10);

    const Row *row = selectedRow();
    if (!row) {
        if (bluetoothAvailable_ && cursor_ == int(rows_.size())) {
            theme.regular().draw(canvas, body2.x, y,
                                 "Pair a new pad. Not every MiSTer has Bluetooth — this row "
                                 "only shows up because this one does.",
                                 theme.sizeBody(), theme.textMuted);
        }
        return;
    }

    theme.bold().draw(canvas, body2.x, y, row->device.name, theme.sizeBody(), theme.textPrimary);
    y += lineStep;

    char idLine[64];
    std::snprintf(idLine, sizeof(idLine), "%04x:%04x  ·  hat: %s", row->device.vendor,
                 row->device.product, row->device.hasHat ? "yes" : "no");
    theme.regular().draw(canvas, body2.x, y, idLine, theme.sizeBody(),
                         theme.textMuted.withAlpha(190));
    y += lineStep;

    if (row->sharesMapping) {
        theme.regular().draw(canvas, body2.x, y,
                             "Shares its mapping with another identical controller below",
                             theme.sizeBody(), theme.accent);
        y += lineStep;
    }

    y += theme.px(8);
    char deadLine[64];
    if (row->hasDeadzone)
        std::snprintf(deadLine, sizeof(deadLine), "Deadzone: %d (set)", row->deadzoneValue);
    else
        std::snprintf(deadLine, sizeof(deadLine), "Deadzone: not set");
    theme.regular().draw(canvas, body2.x, y, deadLine, theme.sizeBody(), theme.textPrimary);
    y += lineStep;

    theme.regular().draw(canvas, body2.x, y,
                         row->hasMapping ? "Button mapping: set" : "Button mapping: not set",
                         theme.sizeBody(), theme.textPrimary);
}

// ---------------------------------------------------------------------------------------
// 2. Input test
// ---------------------------------------------------------------------------------------

void ControllersScreen::enterInputTest() {
    mode_ = Mode::InputTest;
    inputTestStartHold_ = 0.0f;
    inputTestIdleSeconds_ = 0.0f;

    const Row *row = selectedRow();
    const int slot = row ? currentSlot(*row) : -1;
    for (uint16_t code : {uint16_t(ABS_X), uint16_t(ABS_Y), uint16_t(ABS_Z), uint16_t(ABS_RX),
                          uint16_t(ABS_RY), uint16_t(ABS_RZ)})
        inputTestAxisBaseline_[code] = slot >= 0 ? float(input_.absValue(slot, code)) : 0.0f;
}

bool ControllersScreen::anyInputActive(int slot) {
    for (uint16_t code : candidateButtonCodes())
        if (input_.keyState(slot, code)) return true;
    for (uint16_t hatCode = ABS_HAT0X; hatCode <= ABS_HAT3Y; ++hatCode)
        if (hatEdge(slot, hatCode) != 0) return true;

    bool axisActive = false;
    for (uint16_t code : {uint16_t(ABS_X), uint16_t(ABS_Y), uint16_t(ABS_Z), uint16_t(ABS_RX),
                          uint16_t(ABS_RY), uint16_t(ABS_RZ)}) {
        int32_t lo, hi;
        if (!input_.absInfo(slot, code, lo, hi) || hi <= lo) continue;

        const float current = float(input_.absValue(slot, code));
        const float halfRange = float(hi - lo) / 2.0f;
        // Measured from this axis's own *rest* reading, not from the range's mathematical
        // centre — a trigger rests at one end, not the middle, and a motion-sensor axis
        // rests wherever gravity/orientation puts it, so "distance from centre" reads a
        // resting trigger as permanently ~100% deflected.
        const float deflection = std::abs(current - inputTestAxisBaseline_[code]) / halfRange;

        // A generous noise floor (15%) — this is only meant to catch a real touch, not to
        // double as the wizard's own precise capture threshold. Below it, the baseline keeps
        // chasing the current reading, so a live sensor's own ongoing drift/jitter (real
        // accelerometer/gyro data keeps wandering a little even lying still — a *fixed*
        // rest snapshot eventually gets outrun by that over a full 10-second window) never
        // accumulates into a false positive. At or above it, the baseline is left exactly
        // where it is, so a genuine, sustained deflection keeps reading as active for as
        // long as it is held rather than "catching up" and quietly going stale.
        if (deflection > 0.15f) axisActive = true;
        else inputTestAxisBaseline_[code] += (current - inputTestAxisBaseline_[code]) * 0.05f;
    }

    return axisActive;
}

void ControllersScreen::updateInputTest(float deltaSeconds) {
    const Row *row = selectedRow();
    if (!row) { mode_ = Mode::List; return; }
    const int slot = currentSlot(*row);
    if (slot < 0) { mode_ = Mode::List; refresh(); return; }

    // The two ways out of this screen: everything else is deliberately just a reading, not
    // a command (see CONTROLLER.md's premise for this screen — read-only).
    if (input_.keyState(slot, BTN_START)) {
        inputTestStartHold_ += deltaSeconds;
        if (inputTestStartHold_ >= 5.0f) {
            inputTestStartHold_ = 0.0f;
            mode_ = Mode::List;
            return;
        }
    } else {
        inputTestStartHold_ = 0.0f;
    }

    // Some pads register extra sub-devices alongside the real controller (a DualShock's own
    // "Motion Sensors" or "Touchpad" nodes) — pointing this screen at one of those can mean
    // there is no Start to hold at all, which would otherwise strand someone here for good.
    if (anyInputActive(slot)) {
        inputTestIdleSeconds_ = 0.0f;
    } else {
        inputTestIdleSeconds_ += deltaSeconds;
        if (inputTestIdleSeconds_ >= 10.0f) {
            inputTestIdleSeconds_ = 0.0f;
            mode_ = Mode::List;
        }
    }
}

void ControllersScreen::renderInputTest(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    const Row *row = selectedRow();
    if (!row) { mode_ = Mode::List; return; }

    theme.bold().draw(canvas, area.x, area.y, "Input test — " + row->device.name,
                      theme.sizeHeading(), theme.textPrimary);

    // A disconnect here falls back to the list, same as everywhere else in this feature —
    // see CONTROLLER.md, open question 7.
    const int slot = currentSlot(*row);
    if (slot < 0) { mode_ = Mode::List; refresh(); return; }

    const int headerHeight = theme.px(58);
    Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    if (inputTestStartHold_ > 0.0f) {
        const Rect bar{body.x, body.bottom() - theme.px(14), theme.px(300), theme.px(10)};
        canvas.fillRoundedRect(bar, theme.px(5), theme.surface.withAlpha(160));
        canvas.fillRoundedRect(
            {bar.x, bar.y, int(bar.w * std::min(1.0f, inputTestStartHold_ / 5.0f)), bar.h},
            theme.px(5), theme.warning);
    }

    // Only shown once it is actually close to firing — a constant countdown would be
    // distracting noise on a screen whose whole job is watching *other* things light up.
    if (inputTestIdleSeconds_ > 5.0f) {
        char idle[64];
        std::snprintf(idle, sizeof(idle), "No input for a while — leaving in %.0fs",
                     double(10.0f - inputTestIdleSeconds_));
        const int idleWidth = theme.regular().measure(idle, theme.sizeSmall());
        theme.regular().draw(canvas, area.right() - idleWidth, area.y + theme.px(10), idle,
                             theme.sizeSmall(), theme.warning);
    }

    auto drawButton = [&](int x, int y, int w, const char *label, bool pressed) {
        const Rect frame{x, y, w, theme.px(40)};
        canvas.fillRoundedRect(frame, theme.px(8),
                               pressed ? theme.accent : theme.surface.withAlpha(160));
        theme.regular().drawCentered(canvas, frame, label, theme.sizeSmall(),
                                     pressed ? theme.textPrimary : theme.textMuted);
    };

    int y = body.y;
    const int colWidth = theme.px(140);
    const int gap = theme.gap();

    // D-pad — lit by either a literal BTN_DPAD_* key or a hat's digital edge: a D-pad-only
    // pad (a Saturn-style pad, for one) reports its D-pad as a hat axis, not as keys at all.
    // See CONTROLLER.md and the wizard's own capture logic below for the same distinction.
    const bool dpadUp = input_.keyState(slot, BTN_DPAD_UP) || hatEdge(slot, ABS_HAT0Y) == 1;
    const bool dpadDown = input_.keyState(slot, BTN_DPAD_DOWN) || hatEdge(slot, ABS_HAT0Y) == 2;
    const bool dpadLeft = input_.keyState(slot, BTN_DPAD_LEFT) || hatEdge(slot, ABS_HAT0X) == 1;
    const bool dpadRight = input_.keyState(slot, BTN_DPAD_RIGHT) || hatEdge(slot, ABS_HAT0X) == 2;
    drawButton(body.x, y, colWidth, "D-pad Up", dpadUp);
    drawButton(body.x, y + theme.px(46), colWidth, "D-pad Down", dpadDown);
    drawButton(body.x, y + theme.px(92), colWidth, "D-pad Left", dpadLeft);
    drawButton(body.x, y + theme.px(138), colWidth, "D-pad Right", dpadRight);

    // Label the physical Xbox positions. This controller reports BTN_NORTH for its left
    // X button and BTN_WEST for its top Y button; use the same mapping as Input.
    const int faceX = body.x + colWidth + gap;
    const int diamondCx = faceX + colWidth + gap / 2;
    const int diamondCy = y + theme.px(100);
    const int diamondR = theme.px(70);
    const int faceSize = theme.px(52);
    auto drawFaceButton = [&](int cx, int cy, const char *label, bool pressed) {
        const Rect frame{cx - faceSize / 2, cy - faceSize / 2, faceSize, faceSize};
        canvas.fillRoundedRect(frame, faceSize / 2, pressed ? theme.accent : theme.surface.withAlpha(160));
        theme.regular().drawCentered(canvas, frame, label, theme.sizeSmall(),
                                     pressed ? theme.textPrimary : theme.textMuted);
    };
    drawFaceButton(diamondCx, diamondCy - diamondR, "Y", input_.keyState(slot, BTN_WEST));
    drawFaceButton(diamondCx - diamondR, diamondCy, "X", input_.keyState(slot, BTN_NORTH));
    drawFaceButton(diamondCx + diamondR, diamondCy, "B", input_.keyState(slot, BTN_EAST));
    drawFaceButton(diamondCx, diamondCy + diamondR, "A", input_.keyState(slot, BTN_SOUTH));

    // The Sega six-button display toggle is disabled for now (see CONTROLLER.md's own open
    // questions): dropped rather than left in as dead, unreachable code now that nothing can
    // turn it on any more.

    // Shoulders — digital, or a fill bar if the pad reports it as an analogue trigger.
    y += theme.px(200);
    drawButton(body.x, y, colWidth, "L", input_.keyState(slot, BTN_TL));
    drawButton(faceX, y, colWidth, "R", input_.keyState(slot, BTN_TR));

    auto drawTrigger = [&](int x, const char *label, uint16_t absCode, uint16_t digitalCode) {
        int32_t lo, hi;
        if (input_.absInfo(slot, absCode, lo, hi) && hi > lo) {
            const int32_t value = input_.absValue(slot, absCode);
            const float fraction = std::max(0.0f, std::min(1.0f, float(value - lo) / float(hi - lo)));
            const Rect frame{x, y + theme.px(46), colWidth, theme.px(28)};
            canvas.fillRoundedRect(frame, theme.px(6), theme.surface.withAlpha(160));
            canvas.fillRoundedRect({frame.x, frame.y, int(frame.w * fraction), frame.h}, theme.px(6),
                                   theme.accent);
            theme.regular().drawCentered(canvas, frame, label, theme.sizeSmall(), theme.textPrimary);
        } else {
            drawButton(x, y + theme.px(46), colWidth, label, input_.keyState(slot, digitalCode));
        }
    };
    drawTrigger(body.x, "L2", ABS_Z, BTN_TL2);
    drawTrigger(faceX, "R2", ABS_RZ, BTN_TR2);

    // Select / Start / Home
    y += theme.px(100);
    drawButton(body.x, y, colWidth, "Select", input_.keyState(slot, BTN_SELECT));
    drawButton(faceX, y, colWidth, "Start", input_.keyState(slot, BTN_START));
    drawButton(faceX + colWidth + gap, y, colWidth, "Home", input_.keyState(slot, BTN_MODE));

    // Sticks — a small square standing in for the circle this renderer has no dedicated
    // primitive for (fillRoundedRect at half-width radius reads as a circle at this size),
    // with a dot at the live position and a ring at the configured deadzone.
    y += theme.px(80);
    auto drawStick = [&](int x, uint16_t codeX, uint16_t codeY, const char *label) {
        const int size = theme.px(140);
        const Rect frame{x, y, size, size};
        canvas.strokeRoundedRect(frame, size / 2, theme.px(3), theme.surfaceHi);

        int32_t loX, hiX, loY, hiY;
        if (input_.absInfo(slot, codeX, loX, hiX) && input_.absInfo(slot, codeY, loY, hiY) &&
            hiX > loX && hiY > loY) {
            const float nx = (float(input_.absValue(slot, codeX) - loX) / float(hiX - loX)) * 2.0f - 1.0f;
            const float ny = (float(input_.absValue(slot, codeY) - loY) / float(hiY - loY)) * 2.0f - 1.0f;
            const int dotSize = theme.px(20);
            const int cx = frame.x + frame.w / 2 + int(nx * (frame.w / 2 - dotSize / 2));
            const int cy = frame.y + frame.h / 2 + int(ny * (frame.h / 2 - dotSize / 2));
            canvas.fillRoundedRect({cx - dotSize / 2, cy - dotSize / 2, dotSize, dotSize},
                                   dotSize / 2, theme.accent);
        }
        theme.regular().draw(canvas, frame.x, frame.bottom() + theme.px(6), label, theme.sizeSmall(),
                             theme.textMuted);
    };
    drawStick(body.x, ABS_X, ABS_Y, "Stick 1");
    drawStick(faceX + colWidth + gap, ABS_RX, ABS_RY, "Stick 2");
}

// ---------------------------------------------------------------------------------------
// 3. Deadzone
// ---------------------------------------------------------------------------------------

void ControllersScreen::enterDeadzone() {
    const Row *row = selectedRow();
    if (!row) return;

    int existing;
    deadzoneHadExisting_ = MisterIni::readDeadzone(row->vidPid, existing);
    deadzoneValue_ = deadzoneHadExisting_ ? existing : 25;
    mode_ = Mode::Deadzone;
}

void ControllersScreen::handleDeadzoneAction(Action action) {
    switch (action) {
    case Action::Left: deadzoneValue_ = std::max(0, deadzoneValue_ - 1); break;
    case Action::Right: deadzoneValue_ = std::min(100, deadzoneValue_ + 1); break;
    case Action::Back: mode_ = Mode::List; break;
    case Action::Confirm: {
        const Row *row = selectedRow();
        if (row && MisterIni::writeDeadzone(row->vidPid, deadzoneValue_)) {
            context_.notify("Deadzone saved");
            rows_[size_t(cursor_)].hasDeadzone = true;
            rows_[size_t(cursor_)].deadzoneValue = deadzoneValue_;
            mode_ = Mode::List;
        } else {
            context_.showError("Could not save",
                               "MiSTer.ini could not be backed up or written. Nothing was changed.");
        }
        break;
    }
    default: break;
    }
}

void ControllersScreen::renderDeadzone(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    const Row *row = selectedRow();
    if (!row) { mode_ = Mode::List; return; }

    const int slot = currentSlot(*row);
    if (slot < 0) { mode_ = Mode::List; refresh(); return; }

    theme.bold().draw(canvas, area.x, area.y, "Deadzone — " + row->device.name,
                      theme.sizeHeading(), theme.textPrimary);

    const int headerHeight = theme.px(58);
    const Rect body{area.x, area.y + headerHeight, std::min(area.w, theme.px(760)),
                    area.h - headerHeight};

    const Rect track{body.x, body.y + theme.px(60), body.w, theme.px(20)};
    canvas.fillRoundedRect(track, theme.px(10), theme.surface.withAlpha(160));
    const int filled = int(track.w * deadzoneValue_ / 100);
    canvas.fillRoundedRect({track.x, track.y, filled, track.h}, theme.px(10), theme.accent);

    char valueLine[16];
    std::snprintf(valueLine, sizeof(valueLine), "%d", deadzoneValue_);
    theme.bold().draw(canvas, body.x, body.y, valueLine, theme.sizeHeading(), theme.textPrimary);

    theme.regular().draw(canvas, body.x, track.bottom() + theme.px(24),
                         "Move the stick around — the position above shows the raw reading "
                         "this value is compared against.",
                         theme.sizeBody(), theme.textMuted);

    int32_t loX, hiX;
    if (input_.absInfo(slot, ABS_X, loX, hiX) && hiX > loX) {
        const int32_t value = input_.absValue(slot, ABS_X);
        const float fraction = float(value - loX) / float(hiX - loX);
        const int dotX = track.x + int(track.w * fraction);
        canvas.fillRoundedRect({dotX - theme.px(6), track.y - theme.px(14), theme.px(12), theme.px(48)},
                               theme.px(4), theme.textPrimary);
    }

    char writeLine[96];
    std::snprintf(writeLine, sizeof(writeLine), "Writes to MiSTer.ini:  deadzone=0x%08x, %d",
                 row->vidPid, deadzoneValue_);
    theme.regular().draw(canvas, body.x, track.bottom() + theme.px(70), writeLine, theme.sizeBody(),
                         theme.textMuted.withAlpha(190));
}

// ---------------------------------------------------------------------------------------
// 4. Button mapping wizard
// ---------------------------------------------------------------------------------------

void ControllersScreen::beginWizard() {
    const Row *row = selectedRow();
    if (!row) return;

    wizardSteps_.clear();
    wizardSteps_.push_back({"Start", ControllerMap::Start, false, PadHighlight::Start});
    wizardSteps_.push_back({"D-pad Right", ControllerMap::DpadRight, false, PadHighlight::DpadRight});
    wizardSteps_.push_back({"D-pad Left", ControllerMap::DpadLeft, false, PadHighlight::DpadLeft});
    wizardSteps_.push_back({"D-pad Down", ControllerMap::DpadDown, false, PadHighlight::DpadDown});
    wizardSteps_.push_back({"D-pad Up", ControllerMap::DpadUp, false, PadHighlight::DpadUp});
    // Confirmed on real hardware: MiSTer's own A/B/X/Y slots follow SNES-style *positions*
    // (right=A, bottom=B, top=X, left=Y), not the Xbox letters most modern pads are
    // physically labelled with (bottom=A, right=B, left=X, top=Y) — see CONTROLLER.md, "The
    // .map file". This wizard keeps the familiar Xbox letters in its own prompts, so each one
    // has to target the MiSTer slot that means the *same physical position*, not the slot
    // with the matching letter — writing straight into the same-named slot is exactly what
    // silently swapped X and Y on a real pad.
    wizardSteps_.push_back({"A (bottom)", ControllerMap::B, false, PadHighlight::FaceBottom});
    wizardSteps_.push_back({"B (right)", ControllerMap::A, false, PadHighlight::FaceRight});
    wizardSteps_.push_back({"X (left)", ControllerMap::Y, false, PadHighlight::FaceLeft});
    wizardSteps_.push_back({"Y (top)", ControllerMap::X, false, PadHighlight::FaceTop});
    wizardSteps_.push_back({"L", ControllerMap::L, false, PadHighlight::L});
    wizardSteps_.push_back({"R", ControllerMap::R, false, PadHighlight::R});
    wizardSteps_.push_back({"Select", ControllerMap::Select, false, PadHighlight::Select});
    wizardSteps_.push_back({"Stick 1: move left/right", ControllerMap::Stick1X, true, PadHighlight::Stick1Horizontal});
    wizardSteps_.push_back({"Stick 1: move up/down", ControllerMap::Stick1Y, true, PadHighlight::Stick1Vertical});
    wizardSteps_.push_back({"Stick 2: move left/right", ControllerMap::Stick2X, true, PadHighlight::Stick2Horizontal});
    wizardSteps_.push_back({"Stick 2: move up/down", ControllerMap::Stick2Y, true, PadHighlight::Stick2Vertical});
    // Three separate prompts here, matching stock MiSTer's own wizard exactly (menu.cpp:
    // "Menu", "Menu: OK", "Menu: Back" — see CONTROLLER.md, "The .map file", for the full
    // trace through input.cpp that pinned this down after an earlier, wrong assumption broke
    // OSD confirm on real hardware). These are genuinely three different things:
    //  - "Menu" opens the OSD from inside a running core (captured into slots 21/22 —
    //    special-cased below as step.slot == -1).
    //  - "Menu: OK" / "Menu: Back" are what confirms/cancels *once the OSD is already open*
    //    (packed into slot 23 — step.slot == -2 / -3). Skipping either is fine and correct:
    //    unset, both fall back to this same pad's regular A/B mapping (captured earlier in
    //    this same run) rather than leaving anything unusable.
    // Highlighted at the Home/Guide position: not where this combo has to live, but by far
    // the most common real answer on a modern pad — every pad puts a system button top
    // centre nowadays, per real-world confirmation from testing this wizard.
    wizardSteps_.push_back({"Open the menu from inside a running game", -1, false, PadHighlight::Home});
    wizardSteps_.push_back({"Confirm inside the menu (\"OK\")", -2, false, PadHighlight::None});
    wizardSteps_.push_back({"Cancel inside the menu (\"Back\")", -3, false, PadHighlight::None});

    wizardIndex_ = 0;
    wizardSlots_ = ControllerMap::Slots{};
    ControllerMap::read(row->idstr, wizardSlots_); // read-modify-write base; ok if it misses
    startCode_ = 0;
    startHeldLast_ = false;
    startHoldSeconds_ = 0.0f;
    startArmed_ = false;
    stepIdleSeconds_ = 0.0f;
    wizardUsedButtonCodes_.clear();
    wizardUsedAxisCodes_.clear();
    wizardOpeningShown_ = true;
    wizardStartDeviceEventNumber_ = row->device.eventNumber;
    mode_ = Mode::Wizard;
    // Also primed the moment wizardOpeningShown_ actually clears (see handle()) — this call
    // just means nothing is left stale if that never happens (Back cancels first).
    primeButtonBaseline();
}

void ControllersScreen::primeAxisBaseline() {
    const Row *row = selectedRow();
    if (!row) return;
    const int slot = currentSlot(*row);
    for (uint16_t code : kStickAxisCodes) axisBaseline_[code] = input_.absValue(slot, code);
}

int ControllersScreen::hatEdge(int slot, uint16_t code) const {
    // Mirrors Main_MiSTer/input.cpp's own hat handling exactly (the branch that turns a hat
    // deflection into a synthetic EV_KEY for its capture/config code): a hat's range is a
    // small, fixed -1..1 or 0..2, never something to compare against a percentage threshold
    // the way a full-range stick pretending to be a D-pad would be.
    int32_t lo, hi;
    if (!input_.absInfo(slot, code, lo, hi)) return 0;
    if (!((hi == 1 && lo == -1) || (hi == 2 && lo == 0))) return 0;

    const int32_t value = input_.absValue(slot, code);
    if (value == lo) return 1;
    if (value == hi) return 2;
    return 0;
}

void ControllersScreen::primeButtonBaseline() {
    const Row *row = selectedRow();
    buttonBaselineHeld_.assign(KEY_MAX + 1, false);
    for (int &edge : hatBaselineEdge_) edge = 0;
    if (!row) return;
    const int slot = currentSlot(*row);
    if (slot < 0) return;

    for (uint16_t code : candidateButtonCodes()) buttonBaselineHeld_[code] = input_.keyState(slot, code);
    for (uint16_t hatCode = ABS_HAT0X; hatCode <= ABS_HAT3Y; ++hatCode)
        hatBaselineEdge_[hatCode - ABS_HAT0X] = hatEdge(slot, hatCode);
}

void ControllersScreen::abortWizard() {
    mode_ = Mode::List;
    context_.notify("Button mapping cancelled — nothing was saved");
}

void ControllersScreen::saveWizardResult() {
    const Row *row = selectedRow();
    if (row && ControllerMap::write(row->idstr, wizardSlots_)) {
        context_.notify("Button mapping saved");
        rows_[size_t(cursor_)].hasMapping = true;
    } else {
        context_.showError("Could not save", "The mapping file could not be written.");
    }
    mode_ = Mode::List;
}

void ControllersScreen::advanceWizardStep(bool /*skipped*/) {
    ++wizardIndex_;
    stepIdleSeconds_ = 0.0f;
    startHeldLast_ = false;
    startHoldSeconds_ = 0.0f;
    startArmed_ = false;

    if (wizardIndex_ >= wizardSteps_.size()) { saveWizardResult(); return; }
    if (wizardSteps_[wizardIndex_].isAxis) primeAxisBaseline();
    else primeButtonBaseline();
}

void ControllersScreen::captureWizardStep() {
    const Row *row = selectedRow();
    if (!row) { abortWizard(); return; }
    const int slot = currentSlot(*row);
    if (slot < 0) { abortWizard(); return; }
    const WizardStep &step = wizardSteps_[wizardIndex_];

    if (step.isAxis) {
        int bestCode = -1;
        float bestFraction = 0.0f;
        for (uint16_t code : kStickAxisCodes) {
            if (std::find(wizardUsedAxisCodes_.begin(), wizardUsedAxisCodes_.end(), code) !=
                wizardUsedAxisCodes_.end())
                continue; // already answered an earlier stick step — not eligible again

            int32_t lo, hi;
            if (!input_.absInfo(slot, code, lo, hi) || hi <= lo) continue;
            const int32_t value = input_.absValue(slot, code);
            // Fraction of the *half*-range (centre to one edge) rather than of the whole
            // span: a stick resting near centre can only ever travel to one edge or the
            // other, never across the full min..max distance, so measuring against the
            // whole range makes even a full deflection read as roughly "50%".
            const float halfRange = float(hi - lo) / 2.0f;
            if (halfRange <= 0.0f) continue;
            const float fraction = std::abs(float(value - axisBaseline_[code])) / halfRange;
            if (fraction > bestFraction) { bestFraction = fraction; bestCode = code; }
        }
        // A firm 75% of that half-range — deliberately strict. Too loose and a diagonal
        // push registers on the wrong axis first (up/down mistaken for left/right, or the
        // reverse); asking for a near-full, deliberate deflection is what actually tells the
        // two apart reliably.
        if (bestCode >= 0 && bestFraction > 0.75f) {
            wizardSlots_.values[step.slot] = ControllerMap::axisSlotValue(uint16_t(bestCode));
            wizardUsedAxisCodes_.push_back(uint16_t(bestCode));
            advanceWizardStep(false);
        }
        return;
    }

    // "Menu: OK" / "Menu: Back" are exempt from the reuse lock, matching MiSTer's own wizard
    // exactly (input.cpp only duplicate-checks steps before SYS_BTN_CNT_OK): reusing this
    // pad's regular A button as "confirm inside the menu" too is the *normal* case, not a
    // mistake to guard against.
    const bool exemptFromReuseLock = (step.slot == -2 || step.slot == -3);

    auto alreadyUsed = [&](uint32_t code) {
        if (exemptFromReuseLock) return false;
        return std::find(wizardUsedButtonCodes_.begin(), wizardUsedButtonCodes_.end(), code) !=
              wizardUsedButtonCodes_.end();
    };

    auto assign = [&](uint32_t code) {
        if (step.slot == -1) {
            // "Open the menu from inside a running game": mirrored into both combo halves
            // for the common single-button case. A genuine two-button hold-combo (per
            // Main_MiSTer's own input.cpp) is not offered by this wizard yet — see
            // CONTROLLER.md.
            wizardSlots_.values[ControllerMap::OsdComboPrimary] = code;
            wizardSlots_.values[ControllerMap::OsdComboSecondary] = code;
        } else if (step.slot == -2) {
            ControllerMap::setMenuOk(wizardSlots_, uint16_t(code));
        } else if (step.slot == -3) {
            ControllerMap::setMenuBack(wizardSlots_, uint16_t(code));
        } else {
            wizardSlots_.values[step.slot] = code;
        }
        // Once a physical control has answered a step, it is not eligible to answer any
        // later one — see the member comment on wizardUsedButtonCodes_ (except "Menu:
        // OK"/"Back" themselves, which are meant to reuse an earlier button — see above).
        if (!exemptFromReuseLock) wizardUsedButtonCodes_.push_back(code);
        advanceWizardStep(false);
    };

    for (uint16_t code : candidateButtonCodes()) {
        if (code == startCode_) continue; // reserved — handled as press-to-skip, not an answer
        if (alreadyUsed(code)) continue;
        // Edge-triggered, not level-triggered: a code already held when this step began
        // (leftover from confirming the previous step, or — for step 0 — the very same
        // press that opened the wizard) must never be captured. Without this, a button held
        // a little too long silently answers every step it is still down for, one per
        // frame, as fast as this runs — not a skip, a full stampede through the whole list.
        if (code < buttonBaselineHeld_.size() && buttonBaselineHeld_[code]) continue;
        if (!input_.keyState(slot, code)) continue;
        assign(code);
        return;
    }

    // A D-pad-only pad (no analogue stick at all — a Saturn-style pad, for one) reports its
    // D-pad as a hat axis, not as BTN_DPAD_* keys, so the key scan above never sees it. This
    // reproduces MiSTer's own capture encoding for that case — see hatEdge()'s own comment —
    // so the resulting `.map` slot means the same thing MiSTer's own OSD wizard would write.
    for (uint16_t hatCode = ABS_HAT0X; hatCode <= ABS_HAT3Y; ++hatCode) {
        const int edge = hatEdge(slot, hatCode);
        if (edge == 0) continue;
        if (edge == hatBaselineEdge_[hatCode - ABS_HAT0X]) continue; // not a fresh deflection
        const uint32_t code = kKeyEmu + (uint32_t(hatCode) << 1) + uint32_t(edge == 2 ? 1 : 0);
        if (alreadyUsed(code)) continue;
        assign(code);
        return;
    }
}

void ControllersScreen::updateWizard(float deltaSeconds) {
    if (wizardOpeningShown_) return;

    bool stillConnected = false;
    for (const Input::DeviceInfo &d : input_.listDevices())
        if (d.eventNumber == wizardStartDeviceEventNumber_) stillConnected = true;
    if (!stillConnected) { abortWizard(); return; }

    const Row *row = selectedRow();
    if (!row) { abortWizard(); return; }

    if (wizardIndex_ == 0) {
        // Start itself is still unknown: no hold/skip/timeout rule can apply yet, since it
        // is the very thing that rule is built around. Plain capture only.
        captureWizardStep();
        if (wizardIndex_ > 0) startCode_ = uint16_t(wizardSlots_.values[ControllerMap::Start]);
        return;
    }

    const int slot = currentSlot(*row);
    if (slot < 0) { abortWizard(); return; }
    const bool held = input_.keyState(slot, startCode_);

    if (!startArmed_) {
        // The step that just finished may have finished *because* Start was pressed (either
        // captured as Start itself in step 0, or tapped to skip/confirm in a later one) —
        // its physical release can still be a frame or two away. Without this gate, that
        // trailing release is indistinguishable from a fresh "tap Start" on this new step,
        // which silently skips it before the person has pressed anything for it at all —
        // Start ends up answering two prompts from one single press.
        if (held) return;
        startArmed_ = true;
    }

    if (held) {
        startHoldSeconds_ += deltaSeconds;
        startHeldLast_ = true;
        if (startHoldSeconds_ >= 5.0f) { abortWizard(); return; }
        return; // Start is down: wait and see whether this becomes a hold or a tap.
    }
    if (startHeldLast_) {
        // Was held, now released, and never reached 5s above: a plain tap — skip.
        advanceWizardStep(true);
        return;
    }

    stepIdleSeconds_ += deltaSeconds;
    if (stepIdleSeconds_ >= 60.0f) { advanceWizardStep(true); return; }

    captureWizardStep();
}

void ControllersScreen::renderPadDiagram(Canvas &canvas, const Rect &area, PadHighlight highlight) {
    Theme &theme = context_.theme;

    const Color dim = theme.surface.withAlpha(160);
    const Color lit = theme.accent;

    auto drawPart = [&](const Rect &frame, int radius, PadHighlight self, const char *label) {
        const bool on = (self == highlight);
        canvas.fillRoundedRect(frame, radius, on ? lit : dim);
        theme.regular().drawCentered(canvas, frame, label, theme.sizeSmall(),
                                     on ? theme.textPrimary : theme.textMuted);
    };

    // A schematic, not a likeness: two clusters (D-pad left, face buttons right) plus
    // shoulders/select-start/sticks above and below, capped at a fixed width so it stays a
    // sensible size on a very wide screen rather than stretching to fill it.
    const int diagramWidth = std::min(area.w, theme.px(560));
    const int x0 = area.x + (area.w - diagramWidth) / 2;

    const int dpadCx = x0 + theme.px(110);
    const int faceCx = x0 + diagramWidth - theme.px(110);
    const int clusterCy = area.y + theme.px(150);

    const int shoulderY = area.y;
    const int shoulderW = theme.px(120);
    const int shoulderH = theme.px(40);
    drawPart({dpadCx - shoulderW / 2, shoulderY, shoulderW, shoulderH}, theme.px(10), PadHighlight::L, "L");
    drawPart({faceCx - shoulderW / 2, shoulderY, shoulderW, shoulderH}, theme.px(10), PadHighlight::R, "R");

    // Home/Guide: top centre, between the shoulders — where every modern pad puts its own
    // system button, and by far the most common real answer for "open the menu" below.
    const int midCxTop = (dpadCx + faceCx) / 2;
    const int homeSize = theme.px(44);
    drawPart({midCxTop - homeSize / 2, shoulderY, homeSize, homeSize}, homeSize / 2,
             PadHighlight::Home, "Home");

    const int dpadStep = theme.px(48);
    const int dpadSize = theme.px(44);
    drawPart({dpadCx - dpadSize / 2, clusterCy - dpadStep - dpadSize / 2, dpadSize, dpadSize},
             theme.px(8), PadHighlight::DpadUp, "Up");
    drawPart({dpadCx - dpadSize / 2, clusterCy + dpadStep - dpadSize / 2, dpadSize, dpadSize},
             theme.px(8), PadHighlight::DpadDown, "Down");
    drawPart({dpadCx - dpadStep - dpadSize / 2, clusterCy - dpadSize / 2, dpadSize, dpadSize},
             theme.px(8), PadHighlight::DpadLeft, "Left");
    drawPart({dpadCx + dpadStep - dpadSize / 2, clusterCy - dpadSize / 2, dpadSize, dpadSize},
             theme.px(8), PadHighlight::DpadRight, "Right");

    // Xbox-letter positions (Y top, X left, B right, A bottom) — matching this wizard's own
    // prompts, which are deliberately not MiSTer's internal SNES-position slot names. See the
    // comment in beginWizard().
    const int faceR = theme.px(60);
    const int faceSize = theme.px(48);
    auto faceFrame = [&](int cx, int cy) {
        return Rect{cx - faceSize / 2, cy - faceSize / 2, faceSize, faceSize};
    };
    drawPart(faceFrame(faceCx, clusterCy - faceR), faceSize / 2, PadHighlight::FaceTop, "Y");
    drawPart(faceFrame(faceCx - faceR, clusterCy), faceSize / 2, PadHighlight::FaceLeft, "X");
    drawPart(faceFrame(faceCx + faceR, clusterCy), faceSize / 2, PadHighlight::FaceRight, "B");
    drawPart(faceFrame(faceCx, clusterCy + faceR), faceSize / 2, PadHighlight::FaceBottom, "A");

    const int midY = clusterCy + faceR + theme.px(30);
    const int midCx = (dpadCx + faceCx) / 2;
    const int midW = theme.px(110);
    const int midH = theme.px(40);
    drawPart({midCx - midW - theme.px(10), midY, midW, midH}, theme.px(10), PadHighlight::Select, "Select");
    drawPart({midCx + theme.px(10), midY, midW, midH}, theme.px(10), PadHighlight::Start, "Start");

    const int stickY = midY + midH + theme.px(30);
    const int stickSize = theme.px(70);

    // A stick step is either "left/right" or "up/down" — the circle alone does not say
    // which, so a bar through the middle points along whichever axis this step is actually
    // asking for, on top of the same lit/dim circle drawPart() already draws.
    auto drawStick = [&](int cx, PadHighlight horizontalHighlight, PadHighlight verticalHighlight,
                         const char *label) {
        const bool horizontal = (highlight == horizontalHighlight);
        const bool vertical = (highlight == verticalHighlight);
        const Rect frame{cx - stickSize / 2, stickY, stickSize, stickSize};
        drawPart(frame, stickSize / 2, horizontal ? horizontalHighlight : verticalHighlight, label);
        if (!horizontal && !vertical) return;

        const int barThickness = theme.px(10);
        const int barInset = theme.px(10);
        if (horizontal) {
            canvas.fillRoundedRect({frame.x + barInset, frame.y + frame.h / 2 - barThickness / 2,
                                   frame.w - 2 * barInset, barThickness},
                                   barThickness / 2, theme.textPrimary);
        } else {
            canvas.fillRoundedRect({frame.x + frame.w / 2 - barThickness / 2, frame.y + barInset,
                                   barThickness, frame.h - 2 * barInset},
                                   barThickness / 2, theme.textPrimary);
        }
    };
    drawStick(dpadCx, PadHighlight::Stick1Horizontal, PadHighlight::Stick1Vertical, "1");
    drawStick(faceCx, PadHighlight::Stick2Horizontal, PadHighlight::Stick2Vertical, "2");
}

void ControllersScreen::renderWizard(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;
    const Row *row = selectedRow();
    const std::string name = row ? row->device.name : std::string("Controller");

    theme.bold().draw(canvas, area.x, area.y, "Button mapping — " + name, theme.sizeHeading(),
                      theme.textPrimary);

    const int headerHeight = theme.px(58);
    const Rect body{area.x, area.y + headerHeight, area.w, area.h - headerHeight};

    if (wizardOpeningShown_) {
        theme.bold().draw(canvas, body.x, body.y, "First button to configure: Start",
                          theme.sizeBody(), theme.textPrimary);
        theme.regular().draw(canvas, body.x, body.y + theme.px(48),
                             "This button must be assigned correctly — it is also how you "
                             "cancel this wizard once it starts. Hold Start for 5 seconds at "
                             "any point to abandon it, discarding everything captured so far.",
                             theme.sizeBody(), theme.textMuted);
        theme.bold().draw(canvas, body.x, body.y + theme.px(110), "Choose wisely.",
                          theme.sizeBody(), theme.textPrimary);
        return;
    }

    if (wizardIndex_ >= wizardSteps_.size()) return; // one frame from saveWizardResult()

    const WizardStep &step = wizardSteps_[wizardIndex_];
    char heading[128];
    std::snprintf(heading, sizeof(heading), "Press the button for  %s", step.label.c_str());
    theme.bold().draw(canvas, body.x, body.y, heading, theme.sizeBody(), theme.textPrimary);

    if (wizardIndex_ > 0) {
        const float remaining = std::max(0.0f, 60.0f - stepIdleSeconds_);
        char waiting[64];
        std::snprintf(waiting, sizeof(waiting), "waiting... skips in %.0fs", double(remaining));
        theme.regular().draw(canvas, body.x, body.y + theme.px(48), waiting, theme.sizeSmall(),
                             theme.textMuted);

        const Rect bar{body.x, body.y + theme.px(80), theme.px(400), theme.px(14)};
        canvas.fillRoundedRect(bar, theme.px(7), theme.surface.withAlpha(160));
        canvas.fillRoundedRect({bar.x, bar.y, int(bar.w * remaining / 60.0f), bar.h}, theme.px(7),
                               theme.accent);

        if (startHeldLast_) {
            const Rect hold{body.x, body.y + theme.px(110), theme.px(400), theme.px(14)};
            canvas.fillRoundedRect(hold, theme.px(7), theme.surface.withAlpha(160));
            canvas.fillRoundedRect(
                {hold.x, hold.y, int(hold.w * std::min(1.0f, startHoldSeconds_ / 5.0f)), hold.h},
                theme.px(7), theme.warning);
            theme.regular().draw(canvas, body.x, hold.bottom() + theme.px(6),
                                 "holding Start — release before 5s to just skip this button",
                                 theme.sizeSmall(), theme.warning);
        }
    }

    char progress[32];
    std::snprintf(progress, sizeof(progress), "%zu / %zu", wizardIndex_ + 1, wizardSteps_.size());
    const int width = theme.regular().measure(progress, theme.sizeSmall());
    theme.regular().draw(canvas, body.right() - width, body.y, progress, theme.sizeSmall(),
                         theme.textMuted);

    // Removes any dependence on remembering which letter sits where — the highlighted part
    // is unambiguous regardless of Xbox/PlayStation/SNES labelling habits. Not drawn for the
    // three OSD-related steps, which are not tied to one physical spot on the pad.
    if (step.highlight != PadHighlight::None) {
        const Rect diagramArea{body.x, body.y + theme.px(170), body.w, body.h - theme.px(170)};
        if (diagramArea.h > theme.px(200)) renderPadDiagram(canvas, diagramArea, step.highlight);
    }
}

// ---------------------------------------------------------------------------------------
// 5. Bluetooth pairing
// ---------------------------------------------------------------------------------------

void ControllersScreen::enterBluetooth() {
    btState_ = Bluetooth::adapterPresent() ? BtState::Idle : BtState::NoBluetooth;
    btTimer_ = 0.0f;
    btLastLine_.clear();
    mode_ = Mode::Bluetooth;
}

void ControllersScreen::leaveBluetooth() {
    bluetooth_.cancel();
    mode_ = Mode::List;
    refresh();
}

void ControllersScreen::handleBluetoothAction(Action action) {
    switch (btState_) {
    case BtState::Idle:
        if (action == Action::Confirm) {
            btDevicesBeforeScan_ = input_.listDevices();
            if (bluetooth_.startScanning()) { btState_ = BtState::Scanning; btTimer_ = 0.0f; }
            else context_.showError("Could not start", "btpair could not be started.");
        } else if (action == Action::Back) {
            leaveBluetooth();
        }
        break;
    case BtState::Scanning:
        if (action == Action::Back) leaveBluetooth();
        break;
    case BtState::Found:
        if (action == Action::Confirm) {
            mode_ = Mode::Wizard;
            cursor_ = int(rows_.size()); // recomputed below via rebuildRows() + name match
            rebuildRows();
            for (size_t i = 0; i < rows_.size(); ++i)
                if (rows_[i].idstr == btFoundIdstr_) cursor_ = int(i);
            beginWizard();
        } else if (action == Action::Back) {
            leaveBluetooth();
        }
        break;
    case BtState::NoBluetooth:
    case BtState::Timeout:
        if (action == Action::Confirm && btState_ == BtState::Timeout) {
            btState_ = BtState::Idle;
        } else if (action == Action::Back) {
            leaveBluetooth();
        }
        break;
    }
}

void ControllersScreen::updateBluetooth(float deltaSeconds) {
    if (btState_ != BtState::Scanning) return;

    btTimer_ += deltaSeconds;

    std::string line;
    while (bluetooth_.nextLine(line)) btLastLine_ = line;

    // The device-list diff, not btpair's own log text, is what actually decides "a new
    // controller connected" — see CONTROLLER.md, "Detecting 'a new controller just paired'".
    const std::vector<Input::DeviceInfo> now = input_.listDevices();
    for (const Input::DeviceInfo &device : now) {
        bool wasThere = false;
        for (const Input::DeviceInfo &before : btDevicesBeforeScan_)
            if (before.eventNumber == device.eventNumber) wasThere = true;
        if (wasThere) continue;

        bluetooth_.cancel();
        btFoundName_ = device.name;
        btFoundIdstr_ = ControllerId::idstr(device.vendor, device.product);
        ControllerMap::Slots existing;
        btFoundHasMapping_ = ControllerMap::read(btFoundIdstr_, existing);
        btState_ = BtState::Found;
        return;
    }

    // Placeholder, unmeasured against real hardware — see CONTROLLER.md, open question 11.
    if (btTimer_ >= 120.0f) {
        bluetooth_.cancel();
        btState_ = BtState::Timeout;
    }
}

void ControllersScreen::renderBluetooth(Canvas &canvas, const Rect &area) {
    Theme &theme = context_.theme;

    const Rect modal{area.x + area.w / 6, area.y + area.h / 4, area.w * 2 / 3, area.h / 2};
    canvas.fillRoundedRect(modal, theme.radius(), theme.surfaceHi);
    theme.bold().draw(canvas, modal.x + theme.px(24), modal.y + theme.px(20),
                      "Connect new controller", theme.sizeBody(), theme.textPrimary);

    const Rect textArea{modal.x + theme.px(24), modal.y + theme.px(70), modal.w - theme.px(48),
                        modal.h - theme.px(100)};

    switch (btState_) {
    case BtState::Idle:
        theme.regular().draw(canvas, textArea.x, textArea.y,
                             "Put your controller into pairing mode, then press A",
                             theme.sizeBody(), theme.textMuted);
        break;
    case BtState::Scanning: {
        theme.regular().draw(canvas, textArea.x, textArea.y, "Scanning...", theme.sizeBody(),
                             theme.textPrimary);
        char waiting[64];
        std::snprintf(waiting, sizeof(waiting), "waiting for a device to answer, up to 2:00 (%.0fs)",
                     double(btTimer_));
        theme.regular().draw(canvas, textArea.x, textArea.y + theme.px(36), waiting,
                             theme.sizeSmall(), theme.textMuted);
        if (!btLastLine_.empty())
            theme.regular().draw(canvas, textArea.x, textArea.y + theme.px(72), btLastLine_,
                                 theme.sizeSmall(), theme.textMuted.withAlpha(160));
        break;
    }
    case BtState::Found: {
        char found[128];
        std::snprintf(found, sizeof(found), "Found: %s (%s)", btFoundName_.c_str(),
                     btFoundIdstr_.c_str());
        theme.regular().draw(canvas, textArea.x, textArea.y, found, theme.sizeBody(),
                             theme.textPrimary);
        theme.regular().draw(canvas, textArea.x, textArea.y + theme.px(40),
                             btFoundHasMapping_
                                 ? "This controller already has a button mapping."
                                 : "This controller has no button mapping yet. Set it up now?",
                             theme.sizeBody(), theme.textMuted);
        break;
    }
    case BtState::NoBluetooth:
        theme.regular().draw(canvas, textArea.x, textArea.y,
                             "This MiSTer has no Bluetooth adapter.", theme.sizeBody(),
                             theme.textMuted);
        break;
    case BtState::Timeout:
        theme.regular().draw(canvas, textArea.x, textArea.y,
                             "Did not find a new device — try again?", theme.sizeBody(),
                             theme.textMuted);
        break;
    }
}
