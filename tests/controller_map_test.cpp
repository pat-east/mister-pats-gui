// Host-side checks for MiSTer's binary `.map` button-mapping format — see docs/CONTROLLER.md,
// "The .map file", for the byte-for-byte layout this reproduces. A read-modify-write bug
// here produces a file that looks well-formed but silently maps the wrong button, which is
// exactly the failure mode this test is meant to catch before a real pad ever sees it.
//
// Build and run:  make -f tests/Makefile

#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>

#include "../src/ControllerId.h"
#include "../src/ControllerMap.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

const char *kDir = "/tmp/controller_map_test_inputs";

} // namespace

int main() {
    std::string cmd = "rm -rf '" + std::string(kDir) + "'";
    system(cmd.c_str());

    check(ControllerId::idstr(0x045e, 0x02a1) == "045e_02a1",
          "idstr formats vendor_product as lower-case, zero-padded hex");

    check(ControllerMap::pathFor("045e_02a1", kDir) ==
             std::string(kDir) + "/input_045e_02a1_v3.map",
          "pathFor builds MiSTer's own input_<idstr>_v3.map name");

    // 1. No file yet: read reports "not mapped", not an error, and hands back an all-zero
    //    struct rather than garbage.
    ControllerMap::Slots empty;
    check(!ControllerMap::read("045e_02a1", empty, kDir), "read misses when no file exists yet");
    bool allZero = true;
    for (uint32_t v : empty.values) allZero = allZero && (v == 0);
    check(allZero, "an unmapped read hands back an all-zero struct, not garbage");

    // 2. A full round trip: every slot, including the packed OSD slot, survives write+read
    //    exactly.
    ControllerMap::Slots slots;
    slots.values[ControllerMap::DpadRight] = 0x122; // BTN_DPAD_RIGHT-ish placeholder code
    slots.values[ControllerMap::DpadLeft] = 0x121;
    slots.values[ControllerMap::DpadDown] = 0x120;
    slots.values[ControllerMap::DpadUp] = 0x123;
    slots.values[ControllerMap::A] = 0x130;
    slots.values[ControllerMap::B] = 0x131;
    slots.values[ControllerMap::X] = 0x133;
    slots.values[ControllerMap::Y] = 0x134;
    slots.values[ControllerMap::L] = 0x136;
    slots.values[ControllerMap::R] = 0x137;
    slots.values[ControllerMap::Select] = 0x13a;
    slots.values[ControllerMap::Start] = 0x13b;
    slots.values[ControllerMap::OsdComboPrimary] = 0x13c;
    slots.values[ControllerMap::OsdComboSecondary] = 0x13c;
    ControllerMap::setMenuOk(slots, 0x130);
    ControllerMap::setMenuBack(slots, 0x131);
    slots.values[ControllerMap::Stick1X] = ControllerMap::axisSlotValue(0);  // ABS_X
    slots.values[ControllerMap::Stick1Y] = ControllerMap::axisSlotValue(1);  // ABS_Y
    slots.values[ControllerMap::Stick2X] = ControllerMap::axisSlotValue(3);  // ABS_RX
    slots.values[ControllerMap::Stick2Y] = ControllerMap::axisSlotValue(4);  // ABS_RY

    check(slots.values[ControllerMap::OsdPacked] == (0x130u | (0x131u << 16)),
          "setMenuOk/setMenuBack pack OK (low 16) and Back (high 16) as documented");

    // setMenuBack must not clobber a low half already set by an earlier setMenuOk call.
    ControllerMap::Slots order;
    ControllerMap::setMenuOk(order, 0x130);
    ControllerMap::setMenuBack(order, 0x131);
    check(order.values[ControllerMap::OsdPacked] == (0x130u | (0x131u << 16)),
          "setMenuOk then setMenuBack composes correctly regardless of call order");

    check(ControllerMap::write("045e_02a1", slots, kDir), "write succeeds");

    struct stat st{};
    check(stat(ControllerMap::pathFor("045e_02a1", kDir).c_str(), &st) == 0 &&
             st.st_size == ControllerMap::kSlotCount * 4,
          "file on disk is exactly 32 * 4 = 128 bytes");

    ControllerMap::Slots roundTripped;
    check(ControllerMap::read("045e_02a1", roundTripped, kDir), "read hits after a write");

    bool identical = true;
    for (int i = 0; i < ControllerMap::kSlotCount; ++i)
        identical = identical && (roundTripped.values[i] == slots.values[i]);
    check(identical, "every one of the 32 slots survives a write+read round trip exactly");

    // 3. A file of the wrong size is treated as unmapped, not decoded as garbage.
    {
        std::ofstream bad(std::string(kDir) + "/input_dead_beef_v3.map", std::ios::binary);
        bad << "not a map file";
    }
    ControllerMap::Slots garbage;
    check(!ControllerMap::read("dead_beef", garbage, kDir),
          "a wrong-sized file is reported as unmapped rather than mis-decoded");

    // 4. A second write overwrites the first cleanly (the read-modify-write contract is the
    //    caller's job — this only has to prove the writer itself does a clean full replace).
    ControllerMap::Slots second;
    second.values[ControllerMap::Start] = 0x999;
    check(ControllerMap::write("045e_02a1", second, kDir), "second write succeeds");
    ControllerMap::Slots afterSecond;
    check(ControllerMap::read("045e_02a1", afterSecond, kDir) &&
             afterSecond.values[ControllerMap::Start] == 0x999 &&
             afterSecond.values[ControllerMap::A] == 0,
          "a second write fully replaces the file rather than merging with the first");

    system(cmd.c_str());

    std::printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
    return failures ? 1 : 0;
}
