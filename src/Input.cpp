#include "Input.h"

#include "DebugLog.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

namespace {

constexpr int kRepeatDelayMs = 380;
constexpr int kRepeatRateMs = 85;
constexpr int kAxisThreshold = 16000;

// The MiSTer binary mirrors what it reads onto its own uinput device. Reading both that and
// the physical pad would count every press twice.
const char *kIgnoredDevices[] = {"MiSTer virtual input"};

bool isIgnored(const char *name) {
    for (const char *ignored : kIgnoredDevices)
        if (std::strcmp(name, ignored) == 0) return true;
    return false;
}

} // namespace

int64_t nowMs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return int64_t(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

Input::~Input() { releaseAll(); }

void Input::forget(size_t slot) {
    Device &device = devices_[slot];
    if (favoriteDeviceIndex_ == device.index) favoriteDeviceIndex_ = -1;
    if (device.fd >= 0) {
        if (exclusive_) ioctl(device.fd, EVIOCGRAB, 0);
        close(device.fd);
    }
    // Free the number so the next scan picks up whatever takes its place.
    opened_[device.index] = false;
    device.fd = -1;
}

void Input::releaseAll() {
    for (Device &d : devices_) {
        if (d.fd < 0) continue;
        if (exclusive_) ioctl(d.fd, EVIOCGRAB, 0);
        close(d.fd);
        d.fd = -1;
    }
    devices_.clear();
    for (bool &opened : opened_) opened = false;
    heldDirection_ = Action::None;
    favoriteDeviceIndex_ = -1;
}

void Input::rescan() {
    for (int i = 0; i < 64; ++i) {
        if (opened_[i]) continue;

        char path[64];
        std::snprintf(path, sizeof(path), "/dev/input/event%d", i);
        const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        char name[256] = "?";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        if (isIgnored(name)) {
            close(fd);
            opened_[i] = true;   // do not reopen it on the next scan
            continue;
        }

        if (exclusive_) ioctl(fd, EVIOCGRAB, 1);

        opened_[i] = true;
        Device device;
        device.fd = fd;
        device.index = i;
        device.name = name;

        // The kernel's own vendor/product for this evdev node — not the same thing lsusb
        // reports for the underlying USB hardware (see docs/CONTROLLER.md: confirmed to differ
        // for a Wireless Receiver's Xbox 360 pads this project ran into). Anything that keys
        // off a controller's identity the way MiSTer itself does must read it this way.
        input_id id{};
        if (ioctl(fd, EVIOCGID, &id) == 0) {
            device.vendor = id.vendor;
            device.product = id.product;
        }

        probeTrigger(fd, ABS_Z, device.triggerLeftThreshold);
        probeTrigger(fd, ABS_RZ, device.triggerRightThreshold);
        device.hasHat = probeHat(fd);
        device.genericButtons = probeGenericButtons(fd);
        probeAxis(fd, ABS_X, device.axisCenterX, device.axisThresholdX);
        probeAxis(fd, ABS_Y, device.axisCenterY, device.axisThresholdY);
        {
            // One line per opened pad, so a report about a pad that behaves oddly can come with
            // what it actually advertises (see docs/CONTROLLER.md).
            int32_t loX = 0, hiX = 0, loY = 0, hiY = 0;
            input_absinfo info{};
            if (ioctl(fd, EVIOCGABS(ABS_X), &info) == 0) { loX = info.minimum; hiX = info.maximum; }
            if (ioctl(fd, EVIOCGABS(ABS_Y), &info) == 0) { loY = info.minimum; hiY = info.maximum; }
            char line[256];
            std::snprintf(line, sizeof(line),
                          "input: opened event%d \"%s\" %04x:%04x hat=%d generic-buttons=%d "
                          "x=%d..%d (centre %d, trigger %d) y=%d..%d (centre %d, trigger %d)",
                          i, name, unsigned(device.vendor), unsigned(device.product),
                          device.hasHat ? 1 : 0, device.genericButtons ? 1 : 0, int(loX), int(hiX),
                          device.axisCenterX, device.axisThresholdX, int(loY), int(hiY),
                          device.axisCenterY, device.axisThresholdY);
            DebugLog::info(line);
        }
        devices_.push_back(device);
    }
}

void Input::probeTrigger(int fd, uint16_t code, int &threshold) {
    threshold = 0;

    input_absinfo info{};
    if (ioctl(fd, EVIOCGABS(code), &info) < 0) return;

    // ABS_Z and ABS_RZ carry the triggers on one pad and the right stick on the next. A
    // trigger rests at its minimum and only climbs; a stick rests centred and swings both
    // ways. So a range starting at zero is the honest signature of a trigger.
    if (info.minimum != 0 || info.maximum <= 0) return;

    threshold = info.maximum / 2;
}

bool Input::probeHat(int fd) {
    unsigned long bits[(ABS_MAX / (8 * sizeof(unsigned long))) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(bits)), bits) < 0) return false;

    const size_t bitsPerWord = 8 * sizeof(unsigned long);
    return (bits[ABS_HAT0X / bitsPerWord] >> (ABS_HAT0X % bitsPerWord)) & 1;
}

namespace {
// Joystick button n (BTN_TRIGGER + n) -> gamepad button, by physical position on an
// eight-button pad: 0 bottom, 1 right, 2 left, 3 top, 4/5 shoulders, 6 select, 7 start.
// This matches the iBuffalo entry in SDL's gamecontrollerdb (b:b0 a:b1 y:b2 x:b3 ...).
const uint16_t kGenericButtons[8] = {BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST,
                                     BTN_TL, BTN_TR, BTN_SELECT, BTN_START};
} // namespace

void Input::probeAxis(int fd, uint16_t code, int &center, int &threshold) {
    center = 0;
    threshold = kAxisThreshold;

    input_absinfo info{};
    if (ioctl(fd, EVIOCGABS(code), &info) < 0) return;
    const int range = info.maximum - info.minimum;
    if (range <= 0) return;

    // About half way out from the centre — what 16000 is on the usual +-32767 stick.
    center = info.minimum + range / 2;
    threshold = std::max(1, int(range * 0.245));
}

bool Input::probeGenericButtons(int fd) {
    unsigned long bits[(KEY_MAX / (8 * sizeof(unsigned long))) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) return false;

    const size_t bitsPerWord = 8 * sizeof(unsigned long);
    auto has = [&](unsigned code) { return (bits[code / bitsPerWord] >> (code % bitsPerWord)) & 1; };
    return has(BTN_TRIGGER) && !has(BTN_SOUTH) && !has(BTN_EAST);
}

uint16_t Input::gamepadCode(uint16_t code) {
    if (code >= BTN_TRIGGER && code < BTN_TRIGGER + 8) return kGenericButtons[code - BTN_TRIGGER];
    return code;
}

uint16_t Input::joystickCode(uint16_t code) {
    for (uint16_t i = 0; i < 8; ++i)
        if (kGenericButtons[i] == code) return uint16_t(BTN_TRIGGER + i);
    return code;
}

void Input::emitDirection(Action action, bool pressed, std::vector<Action> &out) {
    if (pressed) {
        if (favoriteDeviceIndex_ >= 0) favoriteCanceled_ = true;
        if (removeDeviceIndex_ >= 0) removeCanceled_ = true;
        out.push_back(action);
        heldDirection_ = action;
        heldSince_ = nowMs();
        lastRepeat_ = heldSince_;
    } else if (heldDirection_ == action) {
        heldDirection_ = Action::None;
    }
}

void Input::handleKey(const Device &device, uint16_t code, int32_t value, std::vector<Action> &out) {
    const bool pressed = value != 0;
    if (device.genericButtons) code = gamepadCode(code);

    // On the controllers used with this GUI, the raw north/west codes arrive opposite
    // to their Xbox-labelled physical positions: north is the left X face button.
    if (code == BTN_NORTH || code == KEY_F) {
        if (value == 1 && favoriteDeviceIndex_ < 0) {
            favoriteDeviceIndex_ = device.index;
            favoriteSince_ = nowMs();
            favoriteTriggered_ = false;
            favoriteCanceled_ = false;
            out.push_back(Action::FaceXPress);
        } else if (value == 0 && favoriteDeviceIndex_ == device.index) {
            favoriteDeviceIndex_ = -1;
        }
        return;
    }
    if (value == 1 && favoriteDeviceIndex_ >= 0) favoriteCanceled_ = true;

    // B: Back right away, and a hold timer beside it.
    if (code == BTN_EAST || code == KEY_ESC || code == KEY_BACKSPACE) {
        if (value == 1) {
            if (removeDeviceIndex_ < 0) {
                removeDeviceIndex_ = device.index;
                removeSince_ = nowMs();
                removeTriggered_ = false;
                removeCanceled_ = false;
            }
            out.push_back(Action::Back);
        } else if (value == 0 && removeDeviceIndex_ == device.index) {
            removeDeviceIndex_ = -1;
        }
        return;
    }
    if (value == 1 && removeDeviceIndex_ >= 0) removeCanceled_ = true;

    // Some pads report the D-pad as BTN_DPAD_* keys in addition to the ABS_HAT0X/Y axes
    // handled in handleAbs; acting on both would move the cursor twice per press.
    switch (code) {
    case KEY_UP:    emitDirection(Action::Up, pressed, out); return;
    case KEY_DOWN:  emitDirection(Action::Down, pressed, out); return;
    case KEY_LEFT:  emitDirection(Action::Left, pressed, out); return;
    case KEY_RIGHT: emitDirection(Action::Right, pressed, out); return;
    case BTN_DPAD_UP:    if (!device.hasHat) emitDirection(Action::Up, pressed, out); return;
    case BTN_DPAD_DOWN:  if (!device.hasHat) emitDirection(Action::Down, pressed, out); return;
    case BTN_DPAD_LEFT:  if (!device.hasHat) emitDirection(Action::Left, pressed, out); return;
    case BTN_DPAD_RIGHT: if (!device.hasHat) emitDirection(Action::Right, pressed, out); return;
    default: break;
    }

    if (!pressed) return;

    switch (code) {
    case BTN_START:
        out.push_back(Action::Start);
        break;
    case BTN_SOUTH: case KEY_ENTER: case KEY_KPENTER: case KEY_SPACE:
        out.push_back(Action::Confirm);
        break;
    case BTN_EAST: case KEY_ESC: case KEY_BACKSPACE:
        out.push_back(Action::Back);
        break;
    case BTN_SELECT: case KEY_SELECT:
        out.push_back(Action::Select);
        break;
    case BTN_WEST: case KEY_V:
        out.push_back(Action::CycleView);
        break;
    case BTN_TL: case KEY_PAGEUP:
        out.push_back(Action::TabPrev);
        break;
    case BTN_TR: case KEY_PAGEDOWN: case KEY_TAB:
        out.push_back(Action::TabNext);
        break;
    case BTN_TL2: case KEY_COMMA:
        out.push_back(Action::JumpPrev);
        break;
    case BTN_TR2: case KEY_DOT:
        out.push_back(Action::JumpNext);
        break;
    case KEY_Q:
        out.push_back(Action::Quit);
        break;
    default:
        break;
    }
}

void Input::handleAbs(Device &device, uint16_t code, int32_t value, std::vector<Action> &out) {
    // D-pads arrive as a hat on most pads, analogue sticks as scaled axes.
    auto axis = [&](int &state, Action negative, Action positive, int threshold,
                    int center = 0) {
        const int offset = value - center;
        const int next = (offset <= -threshold) ? -1 : (offset >= threshold ? 1 : 0);
        if (next == state) return;

        if (state == -1) emitDirection(negative, false, out);
        if (state == 1) emitDirection(positive, false, out);
        state = next;
        if (next == -1) emitDirection(negative, true, out);
        if (next == 1) emitDirection(positive, true, out);
    };

    // Triggers fire once per pull; holding one down must not run through the alphabet.
    auto trigger = [&](int &state, Action action, int threshold) {
        const int next = (value >= threshold) ? 1 : 0;
        if (next == state) return;
        state = next;
        if (next) out.push_back(action);
    };

    switch (code) {
    case ABS_HAT0X: axis(device.hatX, Action::Left, Action::Right, 1); break;
    case ABS_HAT0Y: axis(device.hatY, Action::Up, Action::Down, 1); break;
    case ABS_X:     axis(device.axisX, Action::Left, Action::Right, device.axisThresholdX, device.axisCenterX); break;
    case ABS_Y:     axis(device.axisY, Action::Up, Action::Down, device.axisThresholdY, device.axisCenterY); break;
    case ABS_Z:
        if (device.triggerLeftThreshold)
            trigger(device.triggerLeft, Action::JumpPrev, device.triggerLeftThreshold);
        break;
    case ABS_RZ:
        if (device.triggerRightThreshold)
            trigger(device.triggerRight, Action::JumpNext, device.triggerRightThreshold);
        break;
    default: break;
    }
}

void Input::appendRepeats(std::vector<Action> &out) {
    if (heldDirection_ == Action::None) return;

    const int64_t now = nowMs();
    if (now - heldSince_ < kRepeatDelayMs) return;
    if (now - lastRepeat_ < kRepeatRateMs) return;

    lastRepeat_ = now;
    out.push_back(heldDirection_);
}

bool Input::favoriteHoldActive() const {
    return favoriteDeviceIndex_ >= 0 && !favoriteCanceled_ && !favoriteTriggered_;
}

float Input::favoriteHoldProgress() const {
    if (!favoriteHoldActive()) return 0.0f;
    return std::min(1.0f, float(nowMs() - favoriteSince_) / kFavoriteHoldMs);
}

void Input::cancelFavoriteHold() { favoriteCanceled_ = true; }

bool Input::removeHoldActive() const {
    return removeDeviceIndex_ >= 0 && !removeCanceled_ && !removeTriggered_;
}

float Input::removeHoldProgress() const {
    if (!removeHoldActive()) return 0.0f;
    return std::min(1.0f, float(nowMs() - removeSince_) / kRemoveHoldMs);
}

void Input::cancelRemoveHold() { removeCanceled_ = true; }

std::vector<Action> Input::poll(int timeoutMs) {
    std::vector<Action> out;

    std::vector<pollfd> pfds;
    pfds.reserve(devices_.size());
    for (const Device &d : devices_) pfds.push_back({d.fd, POLLIN, 0});

    if (!pfds.empty() && ::poll(pfds.data(), pfds.size(), timeoutMs) > 0) {
        bool lost = false;

        for (size_t i = 0; i < pfds.size(); ++i) {
            // A pad that goes to sleep leaves its node behind as a dead descriptor. Without
            // noticing that, we would hold the corpse and never open its replacement — which
            // looks exactly like the controller having stopped working.
            if (pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                std::printf("input: /dev/input/event%d disconnected\n", devices_[i].index);
                forget(i);
                lost = true;
                continue;
            }

            if (!(pfds[i].revents & POLLIN)) continue;

            input_event ev{};
            ssize_t got;
            while ((got = read(pfds[i].fd, &ev, sizeof(ev))) == sizeof(ev)) {
                if (ev.type == EV_KEY) handleKey(devices_[i], ev.code, ev.value, out);
                else if (ev.type == EV_ABS) handleAbs(devices_[i], ev.code, ev.value, out);
            }

            if (got < 0 && errno == ENODEV) {
                std::printf("input: /dev/input/event%d vanished\n", devices_[i].index);
                forget(i);
                lost = true;
            }
        }

        if (lost) {
            devices_.erase(std::remove_if(devices_.begin(), devices_.end(),
                                          [](const Device &d) { return d.fd < 0; }),
                           devices_.end());
            rescan();
        }
    } else if (pfds.empty()) {
        // Nothing to read from; still pace the caller's loop.
        timespec ts{0, long(timeoutMs) * 1000000L};
        nanosleep(&ts, nullptr);
    }

    appendRepeats(out);
    if (favoriteHoldActive() && nowMs() - favoriteSince_ >= kFavoriteHoldMs) {
        favoriteTriggered_ = true;
        out.push_back(Action::ToggleFavorite);
    }
    if (removeHoldActive() && nowMs() - removeSince_ >= kRemoveHoldMs) {
        removeTriggered_ = true;
        out.push_back(Action::RemoveRecent);
    }
    return out;
}

std::vector<Input::DeviceInfo> Input::listDevices() const {
    std::vector<DeviceInfo> out;
    out.reserve(devices_.size());
    for (size_t i = 0; i < devices_.size(); ++i) {
        const Device &d = devices_[i];
        DeviceInfo info;
        info.slot = int(i);
        info.eventNumber = d.index;
        info.name = d.name;
        info.vendor = d.vendor;
        info.product = d.product;
        info.hasHat = d.hasHat;
        out.push_back(info);
    }
    return out;
}

int Input::findSlot(int eventNumber) const {
    for (size_t i = 0; i < devices_.size(); ++i)
        if (devices_[i].index == eventNumber) return int(i);
    return -1;
}

bool Input::keyState(int slot, uint16_t code) const {
    if (slot < 0 || slot >= int(devices_.size())) return false;
    const int fd = devices_[size_t(slot)].fd;
    if (fd < 0) return false;

    unsigned long bits[(KEY_MAX / (8 * sizeof(unsigned long))) + 1] = {};
    if (ioctl(fd, EVIOCGKEY(sizeof(bits)), bits) < 0) return false;

    const size_t bitsPerWord = 8 * sizeof(unsigned long);
    return (bits[code / bitsPerWord] >> (code % bitsPerWord)) & 1;
}

int Input::axisDirection(int slot, uint16_t code) const {
    if (slot < 0 || slot >= int(devices_.size())) return 0;
    const Device &device = devices_[size_t(slot)];
    if (code != ABS_X && code != ABS_Y) return 0;
    const int center = code == ABS_X ? device.axisCenterX : device.axisCenterY;
    const int threshold = code == ABS_X ? device.axisThresholdX : device.axisThresholdY;
    const int offset = absValue(slot, code) - center;
    return offset <= -threshold ? -1 : (offset >= threshold ? 1 : 0);
}

bool Input::isGenericPad(int slot) const {
    return slot >= 0 && slot < int(devices_.size()) && devices_[size_t(slot)].genericButtons;
}

bool Input::logicalKeyState(int slot, uint16_t code) const {
    if (slot >= 0 && slot < int(devices_.size()) && devices_[size_t(slot)].genericButtons)
        return keyState(slot, joystickCode(code));
    return keyState(slot, code);
}

int32_t Input::absValue(int slot, uint16_t code) const {
    if (slot < 0 || slot >= int(devices_.size())) return 0;
    const int fd = devices_[size_t(slot)].fd;
    if (fd < 0) return 0;

    input_absinfo info{};
    if (ioctl(fd, EVIOCGABS(code), &info) < 0) return 0;
    return info.value;
}

bool Input::absInfo(int slot, uint16_t code, int32_t &minimum, int32_t &maximum) const {
    if (slot < 0 || slot >= int(devices_.size())) return false;
    const int fd = devices_[size_t(slot)].fd;
    if (fd < 0) return false;

    input_absinfo info{};
    if (ioctl(fd, EVIOCGABS(code), &info) < 0) return false;
    minimum = info.minimum;
    maximum = info.maximum;
    return true;
}
