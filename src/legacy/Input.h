#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class Action {
    None,
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Start,
    Back,
    Select,
    FaceXPress, // short X press for controller-management actions
    ToggleFavorite,
    RemoveRecent,   // B held for two seconds
    CycleView,
    TabPrev,
    TabNext,
    // Shoulder triggers: skip to the previous/next initial letter. With a few thousand
    // games in one list, stepping tile by tile is not navigation.
    JumpPrev,
    JumpNext,
    Quit,
};

// evdev reader. Devices are grabbed exclusively while the GUI owns input, and rescanned
// periodically because wireless pads reappear as new nodes.
class Input {
public:
    static constexpr int kFavoriteHoldMs = 2000;
    static constexpr int kRemoveHoldMs = 2000;
    ~Input();

    // Exclusive reading keeps other readers out while the GUI runs. The app releases devices
    // when it exits or hands control to a MiSTer core.
    void setExclusive(bool exclusive) { exclusive_ = exclusive; }

    void rescan();
    int deviceCount() const { return int(devices_.size()); }

    // Waits up to `timeoutMs` and returns everything that happened, repeats included.
    std::vector<Action> poll(int timeoutMs);

    // X/F produces ToggleFavorite only after a continuous two-second hold.
    // A different button, a selection change, or releasing early cancels it.
    bool favoriteHoldActive() const;
    float favoriteHoldProgress() const;
    void cancelFavoriteHold();

    // B/Escape produces Back at once, as always, and RemoveRecent after a continuous
    // two-second hold. Same rules as the favourite hold: another button, a direction or
    // releasing early cancels it, and the App ties it to one selected game.
    bool removeHoldActive() const;
    float removeHoldProgress() const;
    void cancelRemoveHold();

    // Hands the devices back, so a launched core receives input instead of us.
    void releaseAll();

    // Everything the controller-management screens need to know about a connected pad, on
    // top of what the ordinary Action pipeline above already handles. Kept as a snapshot
    // struct (rather than handing out Device itself) so those screens cannot reach into or
    // depend on the navigation-only fields above.
    struct DeviceInfo {
        int slot = -1;          // index into this snapshot vector; stable only within one call
        int eventNumber = 0;    // the N in /dev/input/eventN, MiSTer's own EVIOCGID-read node
        std::string name;
        uint16_t vendor = 0;
        uint16_t product = 0;
        bool hasHat = false;
    };

    // A snapshot of every currently-open device. Rebuilt from scratch each call — cheap
    // enough at a handful of pads, and it means a caller never holds a pointer that could
    // outlive a disconnect.
    std::vector<DeviceInfo> listDevices() const;

    // Resolves a device's *current* slot from its stable `/dev/input/eventN` number — -1 if
    // it is not open right now. A caller that holds onto a DeviceInfo across more than one
    // call (the controller-management screens do, across many frames) must re-resolve the
    // slot this way before each use rather than trusting the one a stale DeviceInfo carries:
    // any device elsewhere disconnecting can reshuffle every other device's slot.
    int findSlot(int eventNumber) const;

    // Raw digital/analogue state, read directly from the kernel (EVIOCGKEY / EVIOCGABS)
    // rather than from the Action event queue — so reading it here never consumes or
    // interferes with the ordinary poll()/Action path above. `slot` is DeviceInfo::slot from
    // the same listDevices() call; an out-of-range slot reads as "not pressed"/0.
    bool keyState(int slot, uint16_t code) const;
    // Like keyState, but asks for a gamepad button (BTN_SOUTH, BTN_TL, ...) and also answers
    // for pads that report plain joystick buttons instead (see Device::genericButtons).
    bool logicalKeyState(int slot, uint16_t code) const;
    // Deflection of ABS_X or ABS_Y as -1, 0 or +1, judged against that axis' own range — the
    // same rule that moves the cursor. Lets the controller test light a D-pad that is an axis.
    int axisDirection(int slot, uint16_t code) const;
    // True for pads with no gamepad buttons (see Device::genericButtons).
    bool isGenericPad(int slot) const;
    int32_t absValue(int slot, uint16_t code) const;
    bool absInfo(int slot, uint16_t code, int32_t &minimum, int32_t &maximum) const;

private:
    struct Device {
        int fd = -1;
        int index = 0;
        int hatX = 0;
        int hatY = 0;
        int axisX = 0;
        int axisY = 0;
        // Centre and trigger distance of ABS_X/ABS_Y, from the axis' own range. A pad whose
        // D-pad is an axis running 0..255 rests at 127 and never reaches +-16000.
        int axisCenterX = 0;
        int axisCenterY = 0;
        int axisThresholdX = 16000;
        int axisThresholdY = 16000;

        std::string name;
        uint16_t vendor = 0;
        uint16_t product = 0;

        // Some pads report the D-pad both as a hat and as BTN_DPAD_* key events. Acting on
        // both would move the cursor twice per press, so the keys are ignored on any device
        // that also has a hat.
        bool hasHat = false;

        // Old and simple pads (an iBuffalo Super Famicom pad, for one) report their eight
        // buttons as BTN_TRIGGER..BTN_BASE2 (0x120-0x127) and have no gamepad buttons at all.
        // Those are mapped by physical position to the buttons the rest of the GUI expects.
        bool genericButtons = false;

        // Analogue triggers, when the pad reports them as axes. Zero threshold means the
        // axis is a stick and must not be read as a trigger.
        int triggerLeftThreshold = 0;
        int triggerRightThreshold = 0;
        int triggerLeft = 0;
        int triggerRight = 0;
    };

    void handleKey(const Device &device, uint16_t code, int32_t value, std::vector<Action> &out);
    void handleAbs(Device &device, uint16_t code, int32_t value, std::vector<Action> &out);
    static void probeTrigger(int fd, uint16_t code, int &threshold);
    static bool probeGenericButtons(int fd);
    static void probeAxis(int fd, uint16_t code, int &center, int &threshold);
    // Maps a joystick button of a generic pad to its gamepad equivalent; other codes pass through.
    static uint16_t gamepadCode(uint16_t code);
    static uint16_t joystickCode(uint16_t code);
    static bool probeHat(int fd);
    void emitDirection(Action action, bool pressed, std::vector<Action> &out);
    void appendRepeats(std::vector<Action> &out);
    void forget(size_t slot);

    std::vector<Device> devices_;
    bool opened_[64] = {};
    bool exclusive_ = false;

    Action heldDirection_ = Action::None;
    int64_t heldSince_ = 0;
    int64_t lastRepeat_ = 0;

    int favoriteDeviceIndex_ = -1;
    int64_t favoriteSince_ = 0;
    bool favoriteTriggered_ = false;
    bool favoriteCanceled_ = false;

    int removeDeviceIndex_ = -1;
    int64_t removeSince_ = 0;
    bool removeTriggered_ = false;
    bool removeCanceled_ = false;
};

int64_t nowMs();
