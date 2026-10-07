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
    Back,
    FaceXPress, // short X press for controller-management actions
    ToggleFavorite,
    CycleView,
    TabPrev,
    TabNext,
    // Shoulder triggers: skip to the previous/next initial letter. With a few thousand
    // games in one list, stepping tile by tile is not navigation.
    JumpPrev,
    JumpNext,
    Quit,
};

// evdev reader. Devices are grabbed exclusively so the frontend underneath does not act on
// the same presses, and rescanned periodically because wireless pads reappear as new nodes.
class Input {
public:
    static constexpr int kFavoriteHoldMs = 2000;
    ~Input();

    // Exclusive reading keeps other readers out — including the MiSTer binary, which
    // would then never see the menu button. Off by default for that reason.
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

        std::string name;
        uint16_t vendor = 0;
        uint16_t product = 0;

        // Some pads report the D-pad both as a hat and as BTN_DPAD_* key events. Acting on
        // both would move the cursor twice per press, so the keys are ignored on any device
        // that also has a hat.
        bool hasHat = false;

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
};

int64_t nowMs();
