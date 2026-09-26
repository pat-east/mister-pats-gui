#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

enum class Action {
    None,
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
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
    ~Input();

    // Exclusive reading keeps other readers out — including the MiSTer binary, which
    // would then never see the menu button. Off by default for that reason.
    void setExclusive(bool exclusive) { exclusive_ = exclusive; }

    void rescan();
    int deviceCount() const { return int(devices_.size()); }

    // Waits up to `timeoutMs` and returns everything that happened, repeats included.
    std::vector<Action> poll(int timeoutMs);

    // Hands the devices back, so a launched core receives input instead of us.
    void releaseAll();

private:
    struct Device {
        int fd = -1;
        int index = 0;
        int hatX = 0;
        int hatY = 0;
        int axisX = 0;
        int axisY = 0;

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
};

int64_t nowMs();
