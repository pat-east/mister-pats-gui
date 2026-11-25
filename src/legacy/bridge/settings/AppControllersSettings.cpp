#include "bridge/settings/AppControllersSettings.h"

#include <linux/input.h>

#include "ButtonLabels.h"
#include "ControllerId.h"
#include "ControllerMap.h"
#include "Input.h"
#include "MisterIni.h"
#include "Preferences.h"

namespace {

// The keys the wizard and the test read: Classic's candidateButtonCodes() (0x120..0x13e and the four digital D-pad keys),
// which also hold every gamepad button the test shows (BTN_SOUTH .. BTN_MODE).
const unsigned short kDpadKeys[4] = {BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT};
// The gamepad codes the test asks logicalKeyState() for (a pad without gamepad buttons maps its joystick buttons).
const unsigned short kLogicalKeys[] = {BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT, BTN_START};

patsgui::ControllerInfo infoOf(const Input::DeviceInfo &d) {
    patsgui::ControllerInfo c;
    c.deviceId = d.eventNumber;
    c.name = d.name;
    c.vendor = d.vendor;
    c.product = d.product;
    c.idstr = ControllerId::idstr(d.vendor, d.product);
    c.hasHat = d.hasHat;
    return c;
}

void arrayOf(const ControllerMap::Slots &from, std::array<uint32_t, 32> &to) {
    for (int i = 0; i < 32; ++i) to[size_t(i)] = from.values[i];
}

}  // namespace

AppControllersSettings::~AppControllersSettings() {
    env_.context.settingsHost.rawInputWanted = false;   // never leave the pad captured
    bluetooth_.cancel();                                  // and never leave btpair running
}

bool AppControllersSettings::available() const { return input() != nullptr; }

void AppControllersSettings::update(float) { ++frame_; }

// ---- Face-button labels: the two setters Classic's row calls (SettingsScreen.cpp buildRows(), "Face-button labels").
// The new GUI follows through the look bridge (LookBridge.cpp reads Preferences::faceButtonStyle every frame).
std::string AppControllersSettings::faceButtonStyle() const { return env_.context.preferences.faceButtonStyle(); }

void AppControllersSettings::setFaceButtonStyle(const std::string &style) {
    if (style != "xbox" && style != "playstation" && style != "nintendo") return;
    env_.context.preferences.setFaceButtonStyle(style);
    ::setFaceButtonStyle(style);
    if (env_.invalidate) env_.invalidate();
}

// ---- The list: ControllersScreen::refresh() / rebuildRows().
void AppControllersSettings::refreshControllers() {
    cache_.clear();
    Input *in = input();
    if (!in) return;
    for (const Input::DeviceInfo &d : in->listDevices()) {
        patsgui::ControllerInfo c = infoOf(d);
        int32_t lo = 0, hi = 0;
        c.hasAnalogStick = in->absInfo(in->findSlot(d.eventNumber), ABS_X, lo, hi);
        cache_.push_back(c);
    }
    for (size_t i = 0; i < cache_.size(); ++i) {
        for (size_t j = 0; j < cache_.size(); ++j)
            if (i != j && cache_[i].idstr == cache_[j].idstr) cache_[i].sharesMapping = true;
        const uint32_t vidPid = ControllerId::vidPid(uint16_t(cache_[i].vendor), uint16_t(cache_[i].product));
        int value = 0;
        cache_[i].hasDeadzone = MisterIni::readDeadzone(vidPid, value);   // read once per refresh, never per frame (SD card)
        cache_[i].deadzone = value;
        ControllerMap::Slots mapped;
        cache_[i].hasMapping = ControllerMap::read(cache_[i].idstr, mapped);
    }
}

bool AppControllersSettings::bluetoothAvailable() const { return Bluetooth::adapterPresent(); }

std::vector<patsgui::ControllerInfo> AppControllersSettings::listDevices() {
    std::vector<patsgui::ControllerInfo> out;
    if (Input *in = input())
        for (const Input::DeviceInfo &d : in->listDevices()) out.push_back(infoOf(d));
    return out;
}

bool AppControllersSettings::find(int deviceId, patsgui::ControllerInfo &out, int *slot) const {
    Input *in = input();
    if (!in) return false;
    for (const Input::DeviceInfo &d : in->listDevices()) {
        if (d.eventNumber != deviceId) continue;
        out = infoOf(d);
        if (slot) *slot = d.slot;
        return true;
    }
    return false;
}

// ---- Raw state: the same reads as Classic (anyInputActive, renderInputTest, captureWizardStep), once per frame.
patsgui::PadSnapshot AppControllersSettings::readPad(int slot) const {
    patsgui::PadSnapshot s;
    Input *in = input();
    if (!in || slot < 0) return s;
    s.connected = true;
    for (unsigned short k = BTN_TRIGGER; k <= BTN_THUMBR; ++k)
        if (in->keyState(slot, k)) s.keys.set(k);
    for (unsigned short k : kDpadKeys)
        if (in->keyState(slot, k)) s.keys.set(k);
    for (unsigned short k : kLogicalKeys)
        if (in->logicalKeyState(slot, k)) s.logicalKeys.set(k);
    for (int i = 0; i < 6; ++i) {
        int32_t lo = 0, hi = 0;
        if (!in->absInfo(slot, uint16_t(i), lo, hi)) continue;
        s.axes[i].present = true;
        s.axes[i].minimum = lo;
        s.axes[i].maximum = hi;
        s.axes[i].value = in->absValue(slot, uint16_t(i));
    }
    for (int i = 0; i < 8; ++i) {
        int32_t lo = 0, hi = 0;
        const uint16_t code = uint16_t(ABS_HAT0X + i);
        if (!in->absInfo(slot, code, lo, hi)) continue;
        s.hats[i].present = true;
        s.hats[i].minimum = lo;
        s.hats[i].maximum = hi;
        s.hats[i].value = in->absValue(slot, code);
    }
    s.genericPad = in->isGenericPad(slot);
    s.axisDirX = in->axisDirection(slot, ABS_X);
    s.axisDirY = in->axisDirection(slot, ABS_Y);
    return s;
}

patsgui::PadSnapshot AppControllersSettings::pad(int deviceId) {
    if (cachedFrame_ == frame_ && cachedDevice_ == deviceId) return cachedPad_;   // one pass per frame and device
    Input *in = input();
    patsgui::PadSnapshot s;
    if (in) s = readPad(in->findSlot(deviceId));
    cachedFrame_ = frame_;
    cachedDevice_ = deviceId;
    cachedPad_ = s;
    return s;
}

// Classic: ControllersScreen::wantsRawInput(); App skips Action dispatch while it is set.
void AppControllersSettings::setRawInputWanted(bool wanted) { env_.context.settingsHost.rawInputWanted = wanted; }

// ---- MiSTer.ini deadzone line.
bool AppControllersSettings::readDeadzone(int deviceId, int &value) {
    patsgui::ControllerInfo d;
    if (!find(deviceId, d)) return false;
    return MisterIni::readDeadzone(ControllerId::vidPid(uint16_t(d.vendor), uint16_t(d.product)), value);
}

bool AppControllersSettings::saveDeadzone(int deviceId, int value) {
    patsgui::ControllerInfo d;
    if (!find(deviceId, d)) return false;
    if (!MisterIni::writeDeadzone(ControllerId::vidPid(uint16_t(d.vendor), uint16_t(d.product)), value)) return false;
    for (patsgui::ControllerInfo &c : cache_)   // Classic updates the focused row only, not an identical pad's
        if (c.deviceId == deviceId) { c.hasDeadzone = true; c.deadzone = value; }
    return true;
}

// ---- The .map file.
bool AppControllersSettings::readMapping(int deviceId, std::array<uint32_t, 32> &slots) {
    slots.fill(0);
    patsgui::ControllerInfo d;
    if (!find(deviceId, d)) return false;
    ControllerMap::Slots m;
    const bool ok = ControllerMap::read(d.idstr, m);
    arrayOf(m, slots);
    return ok;
}

bool AppControllersSettings::writeMapping(int deviceId, const std::array<uint32_t, 32> &slots) {
    patsgui::ControllerInfo d;
    if (!find(deviceId, d)) return false;
    ControllerMap::Slots m;
    for (int i = 0; i < 32; ++i) m.values[i] = slots[size_t(i)];
    if (!ControllerMap::write(d.idstr, m)) return false;
    for (patsgui::ControllerInfo &c : cache_)
        if (c.deviceId == deviceId) c.hasMapping = true;
    return true;
}

// ---- Bluetooth (btpair).
bool AppControllersSettings::startBluetoothScan() { return bluetooth_.startScanning(); }
bool AppControllersSettings::nextBluetoothLine(std::string &line) { return bluetooth_.nextLine(line); }
void AppControllersSettings::cancelBluetoothScan() { bluetooth_.cancel(); }
