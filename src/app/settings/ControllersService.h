#pragma once

// What the Controllers category needs from the host. Classic: Preferences face-button style + ButtonLabels, Input
// (devices and raw state), ControllerMap (.map files), MisterIni (the deadzone line), Bluetooth (btpair); the screen
// src/legacy/ControllersScreen.*. Owned by the Controllers agent.
//
// The split: the host does the I/O (devices, files, the btpair process), the category (src/app/settings/controllers/)
// keeps every state machine of Classic's ControllersScreen (list, input test, deadzone, the mapping wizard with its
// capture rules and timers, Bluetooth pairing), so they run unchanged against fake services on the host.
// Plain value types only; raw evdev numbers are integers (src/app/ includes no <linux/input.h>).

#include <array>
#include <bitset>
#include <cstdint>
#include <string>
#include <vector>

namespace patsgui {

// One open input device (Classic: Input::DeviceInfo + the file facts ControllersScreen::rebuildRows() reads once).
struct ControllerInfo {
    int deviceId = -1;              // the evdev event number: stable while the device is connected (never a slot)
    std::string name;
    int vendor = 0, product = 0;    // shown as "%04x:%04x"
    std::string idstr;              // ControllerId::idstr: "045e_02a1" (the .map file name part)
    bool hasHat = false;            // the device reports ABS_HAT0X
    bool hasAnalogStick = false;    // ABS_X has a range: the deadzone can be adjusted
    bool sharesMapping = false;     // another listed device has the same idstr
    bool hasDeadzone = false;       // a `deadzone=` line for this vendor:product exists in MiSTer.ini
    int deadzone = 0;
    bool hasMapping = false;        // a valid .map file exists
};

// One absolute axis: present when the device reports it with a range (Input::absInfo).
struct AxisReading {
    bool present = false;
    int value = 0, minimum = 0, maximum = 0;
};

// The raw state of one device, read in one pass per frame, no file access (Classic reads it through
// Input::keyState / logicalKeyState / absInfo / absValue / axisDirection / isGenericPad).
struct PadSnapshot {
    static constexpr int kKeyCount = 768;           // KEY_MAX + 1
    bool connected = false;                         // false: the device is gone
    std::bitset<kKeyCount> keys;                    // raw evdev keys held, indexed by code
    std::bitset<kKeyCount> logicalKeys;             // gamepad codes; a generic pad's joystick buttons mapped (logicalKeyState)
    AxisReading axes[6];                            // ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ
    AxisReading hats[8];                            // ABS_HAT0X .. ABS_HAT3Y
    bool genericPad = false;                        // a pad with no gamepad buttons (Input::isGenericPad)
    int axisDirX = 0, axisDirY = 0;                 // Input::axisDirection(ABS_X / ABS_Y): -1, 0, 1 (generic pads)
};

class ControllersService {
public:
    virtual ~ControllersService() = default;
    virtual bool available() const = 0;             // the host has an Input (SettingsHost::input)
    virtual void update(float deltaSeconds) = 0;    // once per frame while the Settings tab is shown

    // ---- Face-button labels (Classic: Preferences::faceButtonStyle() / setFaceButtonStyle() and ButtonLabels.h's
    //      setFaceButtonStyle(), SettingsScreen.cpp "Face-button labels"). Values "xbox", "playstation", "nintendo".
    //      Setting it saves the preference and switches the labels at once; the new GUI follows through the look bridge.
    virtual std::string faceButtonStyle() const = 0;
    virtual void setFaceButtonStyle(const std::string &style) = 0;

    // ---- The controller list (Classic: ControllersScreen::refresh() / rebuildRows(), called when the screen opens, when
    //      Bluetooth is left, and when a device disappears during a test). refreshControllers() enumerates the devices and
    //      reads MiSTer.ini and the .map files once; controllers() is that cached result (no I/O, safe every frame).
    virtual void refreshControllers() = 0;
    virtual const std::vector<ControllerInfo> &controllers() const = 0;
    virtual bool bluetoothAvailable() const = 0;    // Bluetooth::adapterPresent(), sampled by refreshControllers()
    // The devices open right now, in memory only (Input::listDevices): the wizard's "still connected" check and the
    // Bluetooth new-device comparison. No file reads: ControllerInfo's file facts are not filled.
    virtual std::vector<ControllerInfo> listDevices() = 0;

    // ---- Raw state, one snapshot per call (call it once per frame).
    virtual PadSnapshot pad(int deviceId) = 0;
    // True while the mapping wizard captures: the host must not translate the pad into Actions (Classic:
    // ControllersScreen::wantsRawInput(); SettingsHost::rawInputWanted).
    virtual void setRawInputWanted(bool wanted) = 0;

    // ---- MiSTer.ini `deadzone=0x<vendor product>, <0-100>` (Classic: MisterIni::readDeadzone / writeDeadzone). Saving makes
    //      the mandatory backup first and writes atomically; false = nothing was changed.
    virtual bool readDeadzone(int deviceId, int &value) = 0;
    virtual bool saveDeadzone(int deviceId, int value) = 0;

    // ---- The device's .map file: 32 slots (ControllerMap::read / write). readMapping() zeroes `slots` and returns false
    //      when there is no valid file. writeMapping() writes all 128 bytes atomically, without a backup.
    virtual bool readMapping(int deviceId, std::array<uint32_t, 32> &slots) = 0;
    virtual bool writeMapping(int deviceId, const std::array<uint32_t, 32> &slots) = 0;

    // ---- Bluetooth pairing (Classic: Bluetooth, the btpair process). All calls return at once except cancel, which may
    //      wait up to about half a second (a bounded wait, as Classic).
    virtual bool startBluetoothScan() = 0;                      // false: btpair could not be started
    virtual bool nextBluetoothLine(std::string &line) = 0;      // one line of btpair's output, non-blocking
    virtual void cancelBluetoothScan() = 0;                     // idempotent
    // How long a scan runs before the page gives up: Classic's placeholder 120 s (unmeasured). A service may shorten it
    // (the host runner, to reach the timeout page).
    virtual float bluetoothTimeoutSeconds() const { return 120.0f; }
};

}  // namespace patsgui
