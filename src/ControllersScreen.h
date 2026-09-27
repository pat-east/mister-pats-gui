#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Bluetooth.h"
#include "ControllerMap.h"
#include "Screen.h"

// Settings -> Controllers. Everything CONTROLLER.md's design covers lives in this one
// screen, as internal modes rather than five separate Screen subclasses App would have to
// wire up individually — the modes share one entry point (the controller list) and never
// appear except reached from it, so App only ever needs to know about one of them, exactly
// like ScanScreen/SystemVisibilityScreen already do for their own modal flows.
//
// Controller-first throughout, per CONTROLLER.md's premise: every mode below is fully
// operable from a pad alone, including the one mode that is normally hardest to do that way
// (button-mapping capture).
class ControllersScreen : public Screen {
public:
    ControllersScreen(Context &context, Input &input, std::function<void()> onClose);

    // Rebuilds the controller list from whatever Input currently has open. Called each time
    // the screen is opened, same reason SystemVisibilityScreen::refresh() exists: a
    // reconnect in between must not show a stale list.
    void refresh();

    // True while a mode needs every raw button/axis event for itself (the mapping wizard's
    // capture step) — App must not translate the underlying poll() into Actions and dispatch
    // them while this is true, or a press meant as this wizard's answer would also trigger
    // this project's own Back/Confirm handling one layer up. See ControllersScreen.cpp.
    bool wantsRawInput() const;

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    enum class Mode { List, InputTest, Deadzone, Wizard, Bluetooth };

    struct Row {
        Input::DeviceInfo device;
        std::string idstr;      // ControllerId::idstr(device.vendor, device.product)
        uint32_t vidPid = 0;    // ControllerId::vidPid(device.vendor, device.product)
        bool sharesMapping = false; // another row has the same idstr

        // Read once in rebuildRows() rather than from render() every frame — this is the
        // list screen's idle state, so re-reading two files off the SD card thirty times a
        // second for as long as someone happens to sit on it would be pure waste.
        bool hasDeadzone = false;
        int deadzoneValue = 0;
        bool hasMapping = false;
    };

    void rebuildRows();
    const Row *selectedRow() const;

    // A DeviceInfo's own `slot` is only good for the one Input::listDevices() call that
    // produced it (see Input.h) — every other device disconnecting can reshuffle it. Rows
    // here live far longer than one call, so anything that talks to Input has to re-resolve
    // the slot from the device's stable event number first. -1 means "not connected right
    // now" (unplugged, or never was).
    int currentSlot(const Row &row) const;

    // List
    void renderList(Canvas &canvas, const Rect &area);
    void handleListAction(Action action);

    // Input test — read-only, so every input just lights up in the display; the only way
    // out is holding Start for 5 seconds, checked here rather than through the ordinary
    // Action path (there is no "Back" out of this screen any more — see ControllersScreen.cpp).
    // Also leaves on its own after 10s with nothing at all happening — some pads register
    // extra sub-devices (a DualShock's own "Motion Sensors"/"Touchpad" nodes) that this
    // screen can be pointed at but that never report a Start press at all, which would
    // otherwise strand someone with no way out but pulling the controller.
    void enterInputTest();
    void renderInputTest(Canvas &canvas, const Rect &area);
    void updateInputTest(float deltaSeconds);
    bool anyInputActive(int slot);

    // Deadzone
    void enterDeadzone();
    void renderDeadzone(Canvas &canvas, const Rect &area);
    void handleDeadzoneAction(Action action);

    // Button-mapping wizard — see CONTROLLER.md, "Button mapping", for the full rule set
    // this implements (Start captured first, then hold-5s/press-once/1-minute-idle for
    // every step after).

    // Which part of the on-screen pad diagram lights up for the current step — a *physical
    // position*, independent of which underlying `.map` slot the press ends up written to
    // (those two can differ; see CONTROLLER.md, "The .map file", on why MiSTer's own A/B/X/Y
    // slots follow SNES-style positions while this wizard's own prompts stay in the more
    // familiar Xbox ones). None for the two "Menu: OK"/"Menu: Back" steps, which reuse
    // whatever button the person likes rather than pointing at one fixed spot.
    enum class PadHighlight {
        None, DpadUp, DpadDown, DpadLeft, DpadRight,
        FaceTop, FaceBottom, FaceLeft, FaceRight,
        L, R, Select, Start, Home,
        Stick1Horizontal, Stick1Vertical, Stick2Horizontal, Stick2Vertical,
    };

    struct WizardStep {
        std::string label;      // shown on screen, e.g. "Right", "A (bottom)", "Stick 1 (left/right)"
        int slot = -1;          // ControllerMap::Slot this step assigns; -2/-3 are the two
                                // packed halves of slot 23, -1 the OSD-open combo (21/22) —
                                // see the assign() lambda in captureWizardStep()
        bool isAxis = false;
        PadHighlight highlight = PadHighlight::None;
    };
    void beginWizard();
    void updateWizard(float deltaSeconds);
    void renderWizard(Canvas &canvas, const Rect &area);
    void renderPadDiagram(Canvas &canvas, const Rect &area, PadHighlight highlight);
    void captureWizardStep();
    void advanceWizardStep(bool skipped);
    void abortWizard();
    void saveWizardResult();
    void primeAxisBaseline();

    // A hat axis's current digital direction: 0 = centred, 1 = at its minimum, 2 = at its
    // maximum — the same three-way read MiSTer's own input.cpp does for a hat (see
    // ControllersScreen.cpp for the exact source this mirrors). 0 if `code` is not a
    // hat-shaped axis (i.e. not a small, fixed -1..1 or 0..2 range) on this device at all.
    int hatEdge(int slot, uint16_t code) const;

    // Snapshots which candidate button codes are held, and which hat axes are already
    // deflected, right as a non-axis step begins — so captureWizardStep() only ever answers
    // a step with a *fresh* press or a *fresh* hat deflection, never one already under way
    // when the step started (a button held over from confirming the previous step, or from
    // the very same press that began the wizard in the first place).
    void primeButtonBaseline();

    // Bluetooth pairing modal
    void enterBluetooth();
    void updateBluetooth(float deltaSeconds);
    void renderBluetooth(Canvas &canvas, const Rect &area);
    void handleBluetoothAction(Action action);
    void leaveBluetooth();

    Context &context_;
    Input &input_;
    std::function<void()> onClose_;

    Mode mode_ = Mode::List;
    std::vector<Row> rows_;
    int cursor_ = 0;
    bool bluetoothAvailable_ = false;

    // Input test
    float inputTestStartHold_ = 0.0f;
    float inputTestIdleSeconds_ = 0.0f;
    // Each axis's own rough "rest" reading — seeded once when this screen opens
    // (enterInputTest()), then continuously nudged towards wherever the axis currently sits
    // *whenever it is not already flagged as active* (see anyInputActive()). An axis's true
    // rest value is not reliably its range's mathematical centre (a trigger sits at one end;
    // a motion-sensor axis sits wherever gravity/orientation puts it), and it is not
    // reliably *constant* either — a live accelerometer/gyro keeps drifting a little even
    // lying still. A frozen one-time snapshot eventually gets outrun by that drift over a
    // full 10-second window; continuously chasing it while at rest does not, while freezing
    // it the moment real activity is detected keeps a genuine, sustained deflection from
    // ever "catching up" and reading as rest again.
    // A float, not an int: the per-frame adjustment below is a small fraction of a typically
    // small raw difference, and truncating that to an integer every frame would round it to
    // zero forever — exactly defeating the slow-drift case this exists to handle.
    float inputTestAxisBaseline_[6] = {}; // ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ

    // Deadzone
    int deadzoneValue_ = 25;
    bool deadzoneHadExisting_ = false;

    // Wizard
    std::vector<WizardStep> wizardSteps_;
    size_t wizardIndex_ = 0;
    ControllerMap::Slots wizardSlots_;
    uint16_t startCode_ = 0;         // captured in step 0, drives hold/skip/timeout after
    bool startHeldLast_ = false;
    float startHoldSeconds_ = 0.0f;
    // False right as a step begins until Start (startCode_) is seen released at least once —
    // guards against the trailing release of the very press that ended the previous step
    // (Start captured, or tapped to skip/confirm) being misread as a fresh tap on this one.
    bool startArmed_ = false;
    float stepIdleSeconds_ = 0.0f;
    bool wizardOpeningShown_ = false;
    int wizardStartDeviceEventNumber_ = -1; // detects the pad itself disconnecting mid-wizard
    int32_t axisBaseline_[6] = {};          // ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ
    std::vector<bool> buttonBaselineHeld_;  // indexed by raw key code, from primeButtonBaseline()
    int hatBaselineEdge_[8] = {};           // indexed by (hatCode - ABS_HAT0X)

    // Every code (button, hat-derived, or stick axis) already assigned to an earlier step in
    // this same run of the wizard — once a physical control has answered one prompt, it is
    // no longer an eligible answer for any later one. Without this, one over-sensitive stick
    // or a finger resting on the wrong button can silently reuse itself across several steps.
    std::vector<uint32_t> wizardUsedButtonCodes_;
    std::vector<uint16_t> wizardUsedAxisCodes_;

    // Bluetooth
    Bluetooth bluetooth_;
    enum class BtState { Idle, Scanning, Found, NoBluetooth, Timeout };
    BtState btState_ = BtState::Idle;
    float btTimer_ = 0.0f;
    std::vector<Input::DeviceInfo> btDevicesBeforeScan_;
    std::string btLastLine_;
    std::string btFoundName_;
    std::string btFoundIdstr_;
    bool btFoundHasMapping_ = false;
};
