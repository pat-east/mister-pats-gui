#include "app/settings/controllers/PadLogic.h"

#include <algorithm>
#include <cmath>

namespace patsgui {
namespace controllers {

const std::vector<uint16_t> &candidateButtonCodes() {
    static const std::vector<uint16_t> codes = [] {
        std::vector<uint16_t> c;
        for (uint16_t k = code::kBtnTrigger; k <= code::kBtnThumbR; ++k) c.push_back(k);   // BTN_TRIGGER..BTN_THUMBR
        c.push_back(code::kBtnDpadUp);
        c.push_back(code::kBtnDpadDown);
        c.push_back(code::kBtnDpadLeft);
        c.push_back(code::kBtnDpadRight);
        return c;
    }();
    return codes;
}

int hatEdge(const AxisReading &hat) {
    if (!hat.present) return 0;
    if (!((hat.maximum == 1 && hat.minimum == -1) || (hat.maximum == 2 && hat.minimum == 0))) return 0;
    if (hat.value == hat.minimum) return 1;
    if (hat.value == hat.maximum) return 2;
    return 0;
}

// ---------------------------------------------------------------------------------------------------- input test

void InputTestLogic::begin(const PadSnapshot &pad) {
    startHold_ = 0.0f;
    idle_ = 0.0f;
    for (int i = 0; i < 6; ++i) baseline_[i] = pad.connected ? float(pad.axes[i].value) : 0.0f;
}

// ControllersScreen::anyInputActive (CS:337-370): a key, a hat edge, or an axis more than 15 % of its half-range away
// from its own rest reading. Below 15 % the rest reading follows the axis (5 % of the way per call), so a drifting
// sensor never counts; at or above it the reading stays, so a held deflection keeps counting.
bool InputTestLogic::anyActive(const PadSnapshot &pad) {
    for (uint16_t k : candidateButtonCodes())
        if (pad.keys[k]) return true;
    for (const AxisReading &hat : pad.hats)
        if (hatEdge(hat) != 0) return true;

    bool axisActive = false;
    for (int i = 0; i < 6; ++i) {
        const AxisReading &a = pad.axes[i];
        if (!a.present || a.maximum <= a.minimum) continue;
        const float current = float(a.value);
        const float halfRange = float(a.maximum - a.minimum) / 2.0f;
        const float deflection = std::abs(current - baseline_[i]) / halfRange;
        if (deflection > 0.15f) axisActive = true;
        else baseline_[i] += (current - baseline_[i]) * 0.05f;
    }
    return axisActive;
}

InputTestLogic::Outcome InputTestLogic::update(float dt, const PadSnapshot &pad) {
    if (!pad.connected) return Outcome::Disconnected;

    // Hold Start for 5 s: the raw BTN_START (not the logical mapping), as Classic.
    if (pad.keys[code::kBtnStart]) {
        startHold_ += dt;
        if (startHold_ >= kHoldSeconds) {
            startHold_ = 0.0f;
            return Outcome::Exit;
        }
    } else {
        startHold_ = 0.0f;
    }

    // 10 s with nothing at all happening also leaves (a sub-device of a pad may have no Start).
    if (anyActive(pad)) {
        idle_ = 0.0f;
    } else {
        idle_ += dt;
        if (idle_ >= kIdleSeconds) {
            idle_ = 0.0f;
            return Outcome::Exit;
        }
    }
    return Outcome::Running;
}

// ---------------------------------------------------------------------------------------------------- wizard

const std::vector<WizardStep> &wizardSteps() {
    using H = PadHighlight;
    static const std::vector<WizardStep> steps = {
        {"settings.controllers.step.start", Start, false, H::Start},
        {"settings.controllers.step.dpadRight", DpadRight, false, H::DpadRight},
        {"settings.controllers.step.dpadLeft", DpadLeft, false, H::DpadLeft},
        {"settings.controllers.step.dpadDown", DpadDown, false, H::DpadDown},
        {"settings.controllers.step.dpadUp", DpadUp, false, H::DpadUp},
        // MiSTer's own A/B/X/Y slots follow SNES positions (right A, bottom B, top X, left Y), so each Xbox-position
        // prompt writes the slot of the same physical position (CS:670-678).
        {"settings.controllers.step.faceBottom", B, false, H::FaceBottom},
        {"settings.controllers.step.faceRight", A, false, H::FaceRight},
        {"settings.controllers.step.faceLeft", Y, false, H::FaceLeft},
        {"settings.controllers.step.faceTop", X, false, H::FaceTop},
        {"settings.controllers.step.l", L, false, H::L},
        {"settings.controllers.step.r", R, false, H::R},
        {"settings.controllers.step.select", Select, false, H::Select},
        {"settings.controllers.step.stick1x", Stick1X, true, H::Stick1Horizontal},
        {"settings.controllers.step.stick1y", Stick1Y, true, H::Stick1Vertical},
        {"settings.controllers.step.stick2x", Stick2X, true, H::Stick2Horizontal},
        {"settings.controllers.step.stick2y", Stick2Y, true, H::Stick2Vertical},
        {"settings.controllers.step.menu", -1, false, H::Home},
        {"settings.controllers.step.menuOk", -2, false, H::None},
        {"settings.controllers.step.menuBack", -3, false, H::None},
    };
    return steps;
}

namespace {
const uint16_t kStickAxes[4] = {code::kAbsX, code::kAbsY, code::kAbsRX, code::kAbsRY};   // ABS_Z / ABS_RZ carry triggers
}

void MappingWizard::begin(const Slots &existing, const PadSnapshot &pad) {
    slots_ = existing;                 // read-modify-write: the wizard only changes the slots it asks for
    index_ = 0;
    answered_ = 0;
    startCode_ = 0;
    startHeldLast_ = false;
    startArmed_ = false;
    startHold_ = 0.0f;
    stepIdle_ = 0.0f;
    usedButtons_.clear();
    usedAxes_.clear();
    opening_ = true;
    primeButtons(pad);
}

void MappingWizard::confirmOpening(const PadSnapshot &pad) {
    opening_ = false;
    primeButtons(pad);   // the confirming press (still down for a few ms) must not answer step 0
}

void MappingWizard::primeButtons(const PadSnapshot &pad) {
    buttonBaselineHeld_.fill(false);
    for (int &e : hatBaselineEdge_) e = 0;
    if (!pad.connected) return;
    for (uint16_t k : candidateButtonCodes()) buttonBaselineHeld_[k] = pad.keys[k];
    for (int h = 0; h < 8; ++h) hatBaselineEdge_[h] = hatEdge(pad.hats[h]);
}

void MappingWizard::primeAxes(const PadSnapshot &pad) {
    for (uint16_t k : kStickAxes) axisBaseline_[k] = pad.axes[k].value;
}

bool MappingWizard::used(uint32_t c, bool exempt) const {
    if (exempt) return false;   // "Menu: OK" / "Menu: Back" may reuse any button (MiSTer's own wizard only locks earlier steps)
    return std::find(usedButtons_.begin(), usedButtons_.end(), c) != usedButtons_.end();
}

MappingWizard::Outcome MappingWizard::advance(const PadSnapshot &pad) {
    ++index_;
    stepIdle_ = 0.0f;
    startHeldLast_ = false;
    startHold_ = 0.0f;
    startArmed_ = false;
    if (index_ >= count()) return Outcome::Finished;
    if (wizardSteps()[index_].isAxis) primeAxes(pad);
    else primeButtons(pad);
    return Outcome::Running;
}

MappingWizard::Outcome MappingWizard::assign(uint32_t c, const PadSnapshot &pad) {
    const WizardStep &s = wizardSteps()[index_];
    const bool exempt = s.slot == -2 || s.slot == -3;
    if (s.slot == -1) {
        // "Open the menu from inside a running game": mirrored into both halves of the combo (a single button).
        slots_[OsdComboPrimary] = c;
        slots_[OsdComboSecondary] = c;
    } else if (s.slot == -2) {
        setMenuOk(slots_, uint16_t(c));
    } else if (s.slot == -3) {
        setMenuBack(slots_, uint16_t(c));
    } else {
        slots_[size_t(s.slot)] = c;
    }
    if (!exempt) usedButtons_.push_back(c);   // a control that answered a step cannot answer a later one
    ++answered_;
    return advance(pad);
}

MappingWizard::Outcome MappingWizard::capture(const PadSnapshot &pad) {
    if (!pad.connected) return Outcome::Aborted;
    const WizardStep &s = wizardSteps()[index_];

    if (s.isAxis) {
        int bestCode = -1;
        float bestFraction = 0.0f;
        for (uint16_t k : kStickAxes) {
            if (std::find(usedAxes_.begin(), usedAxes_.end(), k) != usedAxes_.end()) continue;
            const AxisReading &a = pad.axes[k];
            if (!a.present || a.maximum <= a.minimum) continue;
            const float halfRange = float(a.maximum - a.minimum) / 2.0f;
            if (halfRange <= 0.0f) continue;
            const float fraction = std::abs(float(a.value - axisBaseline_[k])) / halfRange;
            if (fraction > bestFraction) { bestFraction = fraction; bestCode = k; }
        }
        // A firm 75 % of the half-range: asking for a deliberate deflection tells the axes apart (CS:817-822).
        if (bestCode >= 0 && bestFraction > kAxisThreshold) {
            slots_[size_t(s.slot)] = axisSlotValue(uint16_t(bestCode));
            usedAxes_.push_back(uint16_t(bestCode));
            ++answered_;
            return advance(pad);
        }
        return Outcome::Running;
    }

    const bool exempt = s.slot == -2 || s.slot == -3;
    // Edge-triggered means: a control that was down when the step began is ignored only until it has been released.
    // Without this re-arming, the button that confirmed the opening page (Start, which that page accepts) could never
    // answer step 0, because it is still "held at the start" for ever.
    for (uint16_t k : candidateButtonCodes())
        if (!pad.keys[k]) buttonBaselineHeld_[k] = false;
    for (int h = 0; h < 8; ++h)
        if (hatEdge(pad.hats[h]) == 0) hatBaselineEdge_[h] = 0;
    for (uint16_t k : candidateButtonCodes()) {
        if (k == startCode_) continue;                 // reserved: press-to-skip, never an answer
        if (used(k, exempt)) continue;
        if (buttonBaselineHeld_[k]) continue;          // held when the step began: edge-triggered, never captured
        if (!pad.keys[k]) continue;
        return assign(k, pad);
    }

    // A D-pad-only pad reports its D-pad as a hat axis: MiSTer's own capture code KEY_EMU + (hat << 1) + (max ? 1 : 0).
    // Deliberately not for step 0: Start must be a real key (Classic accepted a hat there, a defect: the code is above
    // KEY_MAX and cannot be read back as a key).
    if (index_ == 0) return Outcome::Running;
    for (int h = 0; h < 8; ++h) {
        const int edge = hatEdge(pad.hats[h]);
        if (edge == 0 || edge == hatBaselineEdge_[h]) continue;   // not a fresh deflection
        const uint32_t c = code::kKeyEmu + (uint32_t(code::kAbsHat0X + h) << 1) + uint32_t(edge == 2 ? 1 : 0);
        if (used(c, exempt)) continue;
        return assign(c, pad);
    }
    return Outcome::Running;
}

MappingWizard::Outcome MappingWizard::update(float dt, bool present, const PadSnapshot &pad) {
    if (opening_) return Outcome::Running;
    if (index_ >= count()) return Outcome::Finished;            // every step answered: nothing left to capture
    if (!present) return Outcome::Aborted;                      // the pad itself disconnected mid-wizard

    if (index_ == 0) {
        // Start itself is still unknown: no hold, skip or timeout rule applies yet; plain capture, no way out but a press.
        const Outcome o = capture(pad);
        if (o == Outcome::Aborted) return o;
        if (index_ > 0) startCode_ = uint16_t(slots_[Start]);
        return o;
    }

    if (!pad.connected) return Outcome::Aborted;
    const bool held = startCode_ < PadSnapshot::kKeyCount && pad.keys[startCode_];

    if (!startArmed_) {
        // The press that ended the previous step may still be released: not a fresh tap on this one.
        if (held) return Outcome::Running;
        startArmed_ = true;
    }

    if (held) {
        startHold_ += dt;
        startHeldLast_ = true;
        if (startHold_ >= kAbortHoldSeconds) return Outcome::Aborted;
        return Outcome::Running;     // Start is down: a hold or a tap, wait and see
    }
    if (startHeldLast_) return advance(pad);   // held and released before 5 s: a tap, skip this button

    stepIdle_ += dt;
    if (stepIdle_ >= kStepIdleSeconds) return advance(pad);   // nothing for a minute: skip
    return capture(pad);
}

}  // namespace controllers
}  // namespace patsgui
