#include "Input.h"

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
}

void Input::rescan() {
    for (int i = 0; i < 64; ++i) {
        if (opened_[i]) continue;

        char path[64];
        std::snprintf(path, sizeof(path), "/dev/input/event%d", i);
        const int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        char name[256] = "?";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        if (isIgnored(name)) {
            std::printf("input: %s = %s [ignored, mirror device]\n", path, name);
            close(fd);
            opened_[i] = true;   // do not reopen it on the next scan
            continue;
        }

        if (exclusive_) ioctl(fd, EVIOCGRAB, 1);
        std::printf("input: %s = %s%s\n", path, name, exclusive_ ? " [exclusive]" : "");

        opened_[i] = true;
        Device device;
        device.fd = fd;
        device.index = i;
        probeTrigger(fd, ABS_Z, device.triggerLeftThreshold);
        probeTrigger(fd, ABS_RZ, device.triggerRightThreshold);
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

void Input::emitDirection(Action action, bool pressed, std::vector<Action> &out) {
    if (pressed) {
        out.push_back(action);
        heldDirection_ = action;
        heldSince_ = nowMs();
        lastRepeat_ = heldSince_;
    } else if (heldDirection_ == action) {
        heldDirection_ = Action::None;
    }
}

void Input::handleKey(uint16_t code, int32_t value, std::vector<Action> &out) {
    const bool pressed = value != 0;

    switch (code) {
    case KEY_UP:    case BTN_DPAD_UP:    emitDirection(Action::Up, pressed, out); return;
    case KEY_DOWN:  case BTN_DPAD_DOWN:  emitDirection(Action::Down, pressed, out); return;
    case KEY_LEFT:  case BTN_DPAD_LEFT:  emitDirection(Action::Left, pressed, out); return;
    case KEY_RIGHT: case BTN_DPAD_RIGHT: emitDirection(Action::Right, pressed, out); return;
    default: break;
    }

    if (!pressed) return;

    switch (code) {
    case BTN_SOUTH: case BTN_START: case KEY_ENTER: case KEY_KPENTER: case KEY_SPACE:
        out.push_back(Action::Confirm);
        break;
    case BTN_EAST: case KEY_ESC: case KEY_BACKSPACE:
        out.push_back(Action::Back);
        break;
    case BTN_WEST: case KEY_F:
        out.push_back(Action::ToggleFavorite);
        break;
    case BTN_NORTH: case KEY_V:
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
    auto axis = [&](int &state, Action negative, Action positive, int threshold) {
        const int next = (value <= -threshold) ? -1 : (value >= threshold ? 1 : 0);
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
    case ABS_X:     axis(device.axisX, Action::Left, Action::Right, kAxisThreshold); break;
    case ABS_Y:     axis(device.axisY, Action::Up, Action::Down, kAxisThreshold); break;
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
                if (ev.type == EV_KEY) handleKey(ev.code, ev.value, out);
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
    return out;
}
