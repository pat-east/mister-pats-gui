#pragma once

// The state machines of Classic's ControllersScreen without any drawing: the input test's activity detection and
// timers, the mapping wizard's steps, capture rules and timers, and the .map slot encoding. Pure logic over the
// service's PadSnapshot, so it is tested on the host with scripted snapshots. Classic source: src/legacy/ControllersScreen.cpp
// (anyInputActive, updateInputTest, beginWizard, captureWizardStep, updateWizard, advanceWizardStep, hatEdge,
// primeButtonBaseline, primeAxisBaseline) and src/legacy/ControllerMap.h (slots, axisSlotValue, setMenuOk/Back).

#include <array>
#include <cstdint>
#include <vector>

#include "app/settings/ControllersService.h"

namespace patsgui {
namespace controllers {

using Slots = std::array<uint32_t, 32>;

// evdev codes (linux/input-event-codes.h); src/app/ includes no kernel headers.
namespace code {
constexpr uint16_t kBtnTrigger = 0x120, kBtnThumbR = 0x13e;                 // the joystick/gamepad key range Classic scans
constexpr uint16_t kBtnSouth = 0x130, kBtnEast = 0x131, kBtnNorth = 0x133, kBtnWest = 0x134;
constexpr uint16_t kBtnTL = 0x136, kBtnTR = 0x137, kBtnTL2 = 0x138, kBtnTR2 = 0x139;
constexpr uint16_t kBtnSelect = 0x13a, kBtnStart = 0x13b, kBtnMode = 0x13c;
constexpr uint16_t kBtnDpadUp = 0x220, kBtnDpadDown = 0x221, kBtnDpadLeft = 0x222, kBtnDpadRight = 0x223;
constexpr uint16_t kAbsX = 0, kAbsY = 1, kAbsZ = 2, kAbsRX = 3, kAbsRY = 4, kAbsRZ = 5;
constexpr uint16_t kAbsHat0X = 0x10;                                        // ABS_HAT0X .. ABS_HAT3Y = 0x10 .. 0x17
constexpr uint32_t kKeyMax = 0x2ff;
constexpr uint32_t kKeyEmu = kKeyMax + 1;                                   // Main_MiSTer input.h: KEY_EMU
}  // namespace code

// ControllerMap::Slot
enum Slot {
    DpadRight = 0, DpadLeft = 1, DpadDown = 2, DpadUp = 3, A = 4, B = 5, X = 6, Y = 7, L = 8, R = 9, Select = 10, Start = 11,
    OsdComboPrimary = 21, OsdComboSecondary = 22, OsdPacked = 23, Stick1X = 24, Stick1Y = 25, Stick2X = 26, Stick2Y = 27,
};

// `0x00020000 | absCode` (ControllerMap::axisSlotValue).
inline uint32_t axisSlotValue(uint16_t absCode) { return 0x00020000u | uint32_t(absCode); }
// Slot 23: the low half is "OK", the high half "Back" (ControllerMap::setMenuOk / setMenuBack).
inline void setMenuOk(Slots &s, uint16_t c) { s[OsdPacked] = (s[OsdPacked] & 0xFFFF0000u) | uint32_t(c); }
inline void setMenuBack(Slots &s, uint16_t c) { s[OsdPacked] = (s[OsdPacked] & 0x0000FFFFu) | (uint32_t(c) << 16); }

// Every key code the wizard answers with: 0x120..0x13e, then the four digital D-pad keys (candidateButtonCodes).
const std::vector<uint16_t> &candidateButtonCodes();

// A hat axis' digital direction: 0 centred (or not a hat), 1 at its minimum, 2 at its maximum; a hat has the range
// -1..1 or 0..2 (ControllersScreen::hatEdge, mirroring Main_MiSTer's input.cpp).
int hatEdge(const AxisReading &hat);

// ---------------------------------------------------------------------------------------------------- input test

// ControllersScreen::enterInputTest / anyInputActive / updateInputTest.
class InputTestLogic {
public:
    enum class Outcome { Running, Exit, Disconnected };
    static constexpr float kHoldSeconds = 5.0f, kIdleSeconds = 10.0f, kIdleWarningSeconds = 5.0f;

    void begin(const PadSnapshot &pad);                       // the rest baselines of the six axes, timers 0
    Outcome update(float dt, const PadSnapshot &pad);         // call once per frame
    float startHold() const { return startHold_; }            // seconds Start has been held (0 when not)
    float idle() const { return idle_; }                      // seconds without any input

private:
    bool anyActive(const PadSnapshot &pad);
    float startHold_ = 0.0f, idle_ = 0.0f;
    float baseline_[6] = {};
};

// ---------------------------------------------------------------------------------------------------- wizard

// Which part of the pad diagram lights up for a step (ControllersScreen::PadHighlight).
enum class PadHighlight {
    None, DpadUp, DpadDown, DpadLeft, DpadRight, FaceTop, FaceBottom, FaceLeft, FaceRight, L, R, Select, Start, Home,
    Stick1Horizontal, Stick1Vertical, Stick2Horizontal, Stick2Vertical,
};

struct WizardStep {
    const char *labelKey;       // catalog key of the label ("settings.controllers.step.start")
    int slot;                   // Slot; -1 the OSD-open combo (21 and 22), -2 / -3 the two halves of slot 23
    bool isAxis;
    PadHighlight highlight;
};

// The 19 steps of beginWizard(), in Classic's order. Prompts use Xbox positions; the slots are the SNES-position slots.
const std::vector<WizardStep> &wizardSteps();

class MappingWizard {
public:
    enum class Outcome { Running, Finished, Aborted };
    static constexpr float kAbortHoldSeconds = 5.0f, kStepIdleSeconds = 60.0f;
    static constexpr float kAxisThreshold = 0.75f;

    // beginWizard(): `existing` is the device's .map file (all zero when there is none: read-modify-write), the opening
    // screen is showing, nothing is captured.
    void begin(const Slots &existing, const PadSnapshot &pad);
    bool opening() const { return opening_; }
    // Classic's Confirm on the opening screen: capturing starts; the press that confirmed must not answer step 0.
    void confirmOpening(const PadSnapshot &pad);
    // True from confirmOpening() until the wizard ends: the host must not translate the pad into Actions
    // (ControllersScreen::wantsRawInput()). `present` = the device is still listed (Input::listDevices()).
    Outcome update(float dt, bool present, const PadSnapshot &pad);

    size_t index() const { return index_; }
    size_t count() const { return wizardSteps().size(); }
    const WizardStep &step() const { return wizardSteps()[index_ < count() ? index_ : count() - 1]; }
    const Slots &slots() const { return slots_; }
    float stepIdle() const { return stepIdle_; }
    bool startHeld() const { return startHeldLast_; }          // Start is down in this step (the hold bar shows)
    float startHold() const { return startHold_; }
    size_t answered() const { return answered_; }              // steps answered by a press or a deflection (the rest were skipped)

private:
    Outcome advance(const PadSnapshot &pad);
    Outcome capture(const PadSnapshot &pad);
    void primeButtons(const PadSnapshot &pad);
    void primeAxes(const PadSnapshot &pad);
    Outcome assign(uint32_t code, const PadSnapshot &pad);
    bool used(uint32_t code, bool exempt) const;

    bool opening_ = true;
    size_t index_ = 0;
    size_t answered_ = 0;
    Slots slots_{};
    uint16_t startCode_ = 0;
    bool startHeldLast_ = false, startArmed_ = false;
    float startHold_ = 0.0f, stepIdle_ = 0.0f;
    int32_t axisBaseline_[6] = {};
    std::array<bool, PadSnapshot::kKeyCount> buttonBaselineHeld_{};
    int hatBaselineEdge_[8] = {};
    std::vector<uint32_t> usedButtons_;
    std::vector<uint16_t> usedAxes_;
};

}  // namespace controllers
}  // namespace patsgui
