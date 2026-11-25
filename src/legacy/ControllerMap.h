#pragma once

#include <cstdint>
#include <string>

// MiSTer's own default-mapping file format: `/media/fat/config/inputs/input_<idstr>_v3.map`,
// 32 little-endian uint32 values, 128 bytes, fixed slots. Reverse-engineered byte-for-byte
// from MiSTer's own source and cross-checked against a real file — see docs/CONTROLLER.md, "The
// .map file", for the full slot table and the reasoning behind each design choice below.
namespace ControllerMap {

enum Slot {
    DpadRight = 0,
    DpadLeft = 1,
    DpadDown = 2,
    DpadUp = 3,
    A = 4,
    B = 5,
    X = 6,
    Y = 7,
    L = 8,
    R = 9,
    Select = 10,
    Start = 11,
    MouseRight = 12,
    MouseLeft = 13,
    MouseDown = 14,
    MouseUp = 15,
    MouseButtonL = 16,
    MouseButtonR = 17,
    MouseButtonM = 18,
    MouseEmuToggle = 19,
    // 20–23 confirmed against Main_MiSTer/input.cpp's own mapping-capture state machine
    // (`SYS_BTN_OSD_KTGL`/`SYS_BTN_CNT_OK`/`SYS_BTN_CNT_ESC`/`SYS_BTN_MENU_FUNC`) and its own
    // three prompts for this area ("Menu", "Menu: OK", "Menu: Back" — menu.cpp) after an
    // earlier, wrong assumption here broke OSD confirm on real hardware. See docs/CONTROLLER.md,
    // "The .map file", for the full trace.
    OsdToggleKeyboard = 20,  // keyboard-session-only "open OSD" key; always 0 for a gamepad
    OsdComboPrimary = 21,    // "open OSD from inside a core": primary/first button
    OsdComboSecondary = 22,  // ...second button of a hold-combo; mirrors 21 for one button
    OsdPacked = 23,          // "OK"/"Back" *while the OSD is already open* — see setMenuOk/Back
    Stick1X = 24,
    Stick1Y = 25,
    Stick2X = 26,
    Stick2Y = 27,
    ActiveStickX = 28,
    ActiveStickY = 29,
    SpinnerX = 30,
    SpinnerY = 31,
    kSlotCount = 32,
};

// A whole file's worth of slots. Deliberately one fixed-size struct rather than 32 separate
// fields or a map: MiSTer's own reader/writer treats this as one atomic 128-byte unit (see
// docs/CONTROLLER.md), so a writer here has to construct — and a reader has to hand back — the
// entire thing as one unit too, never a single slot in isolation.
struct Slots {
    uint32_t values[kSlotCount] = {};
};

// An analogue-stick or spinner axis slot's encoding: `0x0002_0000 | absCode` (e.g. ABS_X=0,
// ABS_Y=1, ABS_RX=3, ABS_RY=4 — see <linux/input-event-codes.h>).
inline uint32_t axisSlotValue(uint16_t absCode) { return 0x00020000u | uint32_t(absCode); }

// index 23's two independent halves: which button confirms ("OK") or cancels ("Back") while
// the OSD is already open. Unlike every other slot, this one has no separate raw-code slot
// of its own to read back later — each half is packed in directly as it is captured, which
// is exactly how Main_MiSTer's own capture code builds it (input.cpp: `map[MENU_FUNC] = code
// & 0xFFFF` for OK, then `map[MENU_FUNC] = (code << 16) | map[MENU_FUNC]` for Back). Left at
// 0, either half falls back to this device's own regular A/B mapping (input.cpp again: `code
// == mmap[SYS_BTN_A]` is the fallback when the low half is unset) — so leaving this whole
// slot untouched is a *correct*, working default, not a placeholder.
inline void setMenuOk(Slots &slots, uint16_t code) {
    slots.values[OsdPacked] = (slots.values[OsdPacked] & 0xFFFF0000u) | uint32_t(code);
}
inline void setMenuBack(Slots &slots, uint16_t code) {
    slots.values[OsdPacked] = (slots.values[OsdPacked] & 0x0000FFFFu) | (uint32_t(code) << 16);
}

constexpr const char *kDefaultInputsDir = "/media/fat/config/inputs";

// `input_<idstr>_v3.map` under `inputsDir` — idstr is `ControllerId::idstr(vendor, product)`.
std::string pathFor(const std::string &idstr, const std::string &inputsDir = kDefaultInputsDir);

// True and `out` filled if a mapping file already exists for this idstr. False (and `out`
// left as all-zero) if there is none yet — that is not an error, just an unmapped pad.
bool read(const std::string &idstr, Slots &out, const std::string &inputsDir = kDefaultInputsDir);

// Always writes the whole 128-byte struct — see Slots above for why a partial update is not
// a safe operation for this format. Atomic (temp file + rename); deliberately no backup (see
// docs/CONTROLLER.md: not load-bearing for booting, and a missing/corrupt one already falls back
// to MiSTer's own built-in default per `load_map`).
bool write(const std::string &idstr, const Slots &slots,
          const std::string &inputsDir = kDefaultInputsDir);

} // namespace ControllerMap
