#pragma once

// Sample Controllers service for the host runner and the tests: fake devices, a scriptable pad (held keys, stick
// positions, a hat, scripted presses that walk through the wizard), an in-memory MiSTer.ini deadzone and .map store with
// failure injection, and a scripted Bluetooth scan. Every mode and state of the category can be shown without a device:
//
//   make -C src/app run ARGS="--tab settings --page controllers/test --set controllers.pad.keys=south,tl --set controllers.pad.stick=x:0.7,y:-0.4"
//   make -C src/app run ARGS="--tab settings --page controllers/wizard --keys a --set controllers.script=wizard --after 6"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>

#include "app/settings/ControllersService.h"
#include "app/settings/SampleControl.h"

namespace patsgui {

class SampleControllersService : public ControllersService, public SampleControl {
public:
    SampleControllersService() { rebuildDevices(); }

    // ---- state the host runner sets (also public for tests)
    std::string style = "xbox";
    bool bluetoothAdapter = true;
    bool btStartFails = false, deadzoneFails = false, mappingFails = false;
    float btFoundAfter = -1.0f;            // seconds after a scan starts until a new pad appears (< 0: never)
    bool btFoundMapped = false;
    std::vector<std::string> btLines;       // lines btpair "prints", one per call of nextBluetoothLine()
    bool rawInputWanted = false;            // what the page asked for (the host would stop translating the pad)
    int padCount = 2;
    bool sharedMapping = false, hat = false, analogTriggers = false, generic = false;
    bool stickOnPadB = false;
    PadSnapshot live;                        // the held state of the device being watched (keys, sticks, hat)
    float unplugAt = -1.0f;                  // seconds from now until pad A is unplugged
    std::string script;                      // "", "wizard", "wizard-abort", "test"
    std::map<std::string, std::array<uint32_t, 32>> maps;   // idstr -> .map file (the fake card)
    std::map<unsigned, int> deadzones;                       // vendor << 16 | product -> value (the fake MiSTer.ini)
    int mappingsWritten = 0, deadzonesWritten = 0;

    float btTimeout = 120.0f;               // the scan gives up after this many seconds (controllers.bt.timeout)
    float bluetoothTimeoutSeconds() const override { return btTimeout; }

    bool available() const override { return true; }

    void update(float dt) override {
        time_ += dt;
        if (unplugAt >= 0.0f) {
            unplugAt -= dt;
            if (unplugAt < 0.0f) { unplugAt = -1.0f; unplugged_ = true; rebuildDevices(); }
        }
        if (bluetoothScanning_) {
            scanTime_ += dt;
            if (btFoundAfter >= 0.0f && scanTime_ >= btFoundAfter && !btDeviceAdded_) { btDeviceAdded_ = true; rebuildDevices(); }
        }
        if (script == "wizard" || script == "wizard-abort") {
            if (rawInputWanted) {
                if (rawStart_ < 0.0f) rawStart_ = time_;
                runWizardScript(time_ - rawStart_);
            } else {
                rawStart_ = -1.0f;
            }
        } else if (script == "test") {
            runTestScript(time_);
        }
    }

    // ---- face-button labels
    std::string faceButtonStyle() const override { return style; }
    void setFaceButtonStyle(const std::string &s) override {
        if (s == "xbox" || s == "playstation" || s == "nintendo") style = s;
    }

    // ---- list
    void refreshControllers() override {
        cache_ = devices_;
        for (ControllerInfo &c : cache_) {
            c.hasMapping = maps.count(c.idstr) > 0;
            const unsigned key = (unsigned(c.vendor) << 16) | unsigned(c.product);
            c.hasDeadzone = deadzones.count(key) > 0;
            c.deadzone = c.hasDeadzone ? deadzones[key] : 0;
            c.sharesMapping = false;
            for (const ControllerInfo &o : cache_)
                if (&o != &c && o.idstr == c.idstr) c.sharesMapping = true;
        }
    }
    const std::vector<ControllerInfo> &controllers() const override { return cache_; }
    bool bluetoothAvailable() const override { return bluetoothAdapter; }
    std::vector<ControllerInfo> listDevices() override { return devices_; }

    // ---- raw state
    PadSnapshot pad(int deviceId) override {
        PadSnapshot s = live;
        s.connected = false;
        for (const ControllerInfo &d : devices_) {
            if (d.deviceId != deviceId) continue;
            s.connected = true;
            const bool stick = d.hasAnalogStick;
            if (!stick) for (int i = 0; i < 6; ++i) s.axes[i] = AxisReading();
            if (!d.hasHat) for (int i = 0; i < 8; ++i) s.hats[i] = AxisReading();
        }
        return s;
    }
    void setRawInputWanted(bool wanted) override { rawInputWanted = wanted; }

    // ---- MiSTer.ini deadzone
    bool readDeadzone(int deviceId, int &value) override {
        for (const ControllerInfo &d : devices_) {
            if (d.deviceId != deviceId) continue;
            const auto it = deadzones.find((unsigned(d.vendor) << 16) | unsigned(d.product));
            if (it == deadzones.end()) return false;
            value = it->second;
            return true;
        }
        return false;
    }
    bool saveDeadzone(int deviceId, int value) override {
        if (deadzoneFails) return false;
        for (ControllerInfo &d : devices_) {
            if (d.deviceId != deviceId) continue;
            deadzones[(unsigned(d.vendor) << 16) | unsigned(d.product)] = value;
            ++deadzonesWritten;
            // Classic updates only the focused row of the list it holds, not an identical pad's.
            for (ControllerInfo &c : cache_)
                if (c.deviceId == deviceId) { c.hasDeadzone = true; c.deadzone = value; }
            return true;
        }
        return false;
    }

    // ---- .map file
    bool readMapping(int deviceId, std::array<uint32_t, 32> &slots) override {
        slots.fill(0);
        for (const ControllerInfo &d : devices_) {
            if (d.deviceId != deviceId) continue;
            const auto it = maps.find(d.idstr);
            if (it == maps.end()) return false;
            slots = it->second;
            return true;
        }
        return false;
    }
    bool writeMapping(int deviceId, const std::array<uint32_t, 32> &slots) override {
        if (mappingFails) return false;
        for (const ControllerInfo &d : devices_) {
            if (d.deviceId != deviceId) continue;
            maps[d.idstr] = slots;
            ++mappingsWritten;
            for (ControllerInfo &c : cache_)
                if (c.deviceId == deviceId) c.hasMapping = true;
            return true;
        }
        return false;
    }

    // ---- Bluetooth
    bool startBluetoothScan() override {
        if (btStartFails) return false;
        bluetoothScanning_ = true;
        scanTime_ = 0.0f;
        btDeviceAdded_ = false;
        pendingLines_ = btLines;
        return true;
    }
    bool nextBluetoothLine(std::string &line) override {
        if (!bluetoothScanning_ || pendingLines_.empty()) return false;
        line = pendingLines_.front();
        pendingLines_.erase(pendingLines_.begin());
        return true;
    }
    void cancelBluetoothScan() override { bluetoothScanning_ = false; }
    bool scanning() const { return bluetoothScanning_; }

    // ---- --set keys
    bool set(const std::string &key, const std::string &v) override {
        if (key == "controllers.pads") { padCount = std::max(0, std::min(3, std::atoi(v.c_str()))); rebuildDevices(); return true; }
        if (key == "controllers.shared") { sharedMapping = sampleBool(v); rebuildDevices(); return true; }
        if (key == "controllers.hat") { hat = sampleBool(v); rebuildDevices(); return true; }
        if (key == "controllers.stickOnB") { stickOnPadB = sampleBool(v); rebuildDevices(); return true; }
        if (key == "controllers.style" && (v == "xbox" || v == "playstation" || v == "nintendo")) { style = v; return true; }
        if (key == "controllers.bluetooth") { bluetoothAdapter = v != "none"; return true; }
        if (key == "controllers.bt.start") { btStartFails = v == "fail"; return true; }
        if (key == "controllers.bt.timeout") { btTimeout = std::max(1.0f, float(std::atof(v.c_str()))); return true; }
        if (key == "controllers.bt.line") { btLines.push_back(v); return true; }
        if (key == "controllers.bt.found") { btFoundAfter = v == "never" ? -1.0f : float(std::atof(v.c_str())); return true; }
        if (key == "controllers.bt.mapped") { btFoundMapped = sampleBool(v); if (btFoundMapped) maps["0003_0003"] = std::array<uint32_t, 32>(); else maps.erase("0003_0003"); return true; }
        if (key == "controllers.deadzone.fail") { deadzoneFails = sampleBool(v); return true; }
        if (key == "controllers.mapping.fail") { mappingFails = sampleBool(v); return true; }
        if (key == "controllers.unplug") { unplugAt = float(std::atof(v.c_str())); return true; }
        if (key == "controllers.analogTriggers") { analogTriggers = sampleBool(v); applyAxes(); return true; }
        if (key == "controllers.generic") { generic = sampleBool(v); live.genericPad = generic; return true; }
        if (key == "controllers.script") { script = v == "none" ? "" : v; scriptReset(); return true; }
        if (key == "controllers.pad.keys") { setKeys(v); return true; }
        if (key == "controllers.pad.stick") { setSticks(v); return true; }
        if (key == "controllers.pad.hat") { setHat(v); return true; }
        return false;
    }
    std::vector<std::string> help() const override {
        return {"controllers.style=xbox|playstation|nintendo",
                "controllers.pads=0|1|2|3   (0 empty list; 2 default: pad A with a stick, pad B without)",
                "controllers.shared=0|1   (pad B has pad A's id: they share a mapping)",
                "controllers.hat=0|1   (pad A has a hat)   controllers.stickOnB=0|1   controllers.generic=0|1   controllers.analogTriggers=0|1",
                "controllers.pad.keys=south,east,north,west,tl,tr,tl2,tr2,select,start,mode,dpadup,dpaddown,dpadleft,dpadright (held)",
                "controllers.pad.stick=x:0.7,y:-0.4,rx:0,ry:0,lt:0.5,rt:1   (-1..1; triggers 0..1)",
                "controllers.pad.hat=none|up|down|left|right",
                "controllers.script=none|wizard|wizard-abort|test   (presses by itself; wizard starts when capture starts)",
                "controllers.unplug=<seconds>   (pad A disappears)",
                "controllers.bluetooth=available|none   controllers.bt.start=ok|fail   controllers.bt.line=<text>   controllers.bt.found=<seconds>|never   controllers.bt.timeout=<seconds>   controllers.bt.mapped=0|1",
                "controllers.deadzone.fail=0|1   controllers.mapping.fail=0|1"};
    }

private:
    void rebuildDevices() {
        devices_.clear();
        auto add = [&](int id, const char *name, int vid, int pid, bool stick, bool hasHat) {
            ControllerInfo d;
            d.deviceId = id; d.name = name; d.vendor = vid; d.product = pid;
            char b[16];
            std::snprintf(b, sizeof(b), "%04x_%04x", vid, pid);
            d.idstr = b; d.hasAnalogStick = stick; d.hasHat = hasHat;
            devices_.push_back(d);
        };
        if (padCount >= 1 && !unplugged_) add(0, "Example pad A", 0x045e, 0x02a1, true, hat);
        if (padCount >= 2) add(1, "Example pad B", sharedMapping ? 0x045e : 0x054c, sharedMapping ? 0x02a1 : 0x09cc, stickOnPadB, false);
        if (padCount >= 3) add(2, "Example pad C", 0x2dc8, 0x3106, true, false);
        if (btDeviceAdded_) add(7, "Example Bluetooth pad", 0x0003, 0x0003, true, false);
        applyAxes();
        // the file facts of a list that is already shown stay as they were until the next refresh (a snapshot)
        if (cache_.empty()) refreshControllers();
    }
    void applyAxes() {
        for (int i = 0; i < 6; ++i) { live.axes[i].present = true; live.axes[i].minimum = -32768; live.axes[i].maximum = 32767; }
        for (int i = 2; i <= 5; i += 3) { /* ABS_Z and ABS_RZ */
            live.axes[i].present = analogTriggers;
            live.axes[i].minimum = 0; live.axes[i].maximum = 255; if (live.axes[i].value < 0) live.axes[i].value = 0;
        }
        for (int i = 0; i < 8; ++i) { live.hats[i].present = true; live.hats[i].minimum = -1; live.hats[i].maximum = 1; }
        live.genericPad = generic;
    }

    static int keyCode(const std::string &n) {
        static const struct { const char *name; int code; } table[] = {
            {"south", 0x130}, {"east", 0x131}, {"north", 0x133}, {"west", 0x134}, {"tl", 0x136}, {"tr", 0x137}, {"tl2", 0x138}, {"tr2", 0x139},
            {"select", 0x13a}, {"start", 0x13b}, {"mode", 0x13c}, {"dpadup", 0x220}, {"dpaddown", 0x221}, {"dpadleft", 0x222}, {"dpadright", 0x223}};
        for (const auto &r : table) if (n == r.name) return r.code;
        return int(std::strtol(n.c_str(), nullptr, 0));
    }
    void setKeys(const std::string &v) {
        live.keys.reset(); live.logicalKeys.reset();
        std::stringstream in(v);
        std::string item;
        while (std::getline(in, item, ',')) {
            const int c = keyCode(item);
            if (c > 0 && c < PadSnapshot::kKeyCount) { live.keys.set(size_t(c)); live.logicalKeys.set(size_t(c)); }
        }
    }
    void setAxis(int index, float f) {
        AxisReading &a = live.axes[index];
        a.value = a.minimum + int((f * 0.5f + 0.5f) * float(a.maximum - a.minimum));
        if (index == 2 || index == 5) a.value = int(f * 255.0f);
    }
    void setSticks(const std::string &v) {
        std::stringstream in(v);
        std::string item;
        while (std::getline(in, item, ',')) {
            const size_t c = item.find(':');
            if (c == std::string::npos) continue;
            const std::string n = item.substr(0, c);
            const float f = float(std::atof(item.c_str() + c + 1));
            if (n == "x") setAxis(0, f); else if (n == "y") setAxis(1, f); else if (n == "rx") setAxis(3, f); else if (n == "ry") setAxis(4, f);
            else if (n == "lt") setAxis(2, f); else if (n == "rt") setAxis(5, f);
        }
        live.axisDirX = live.axes[0].value > 8000 ? 1 : live.axes[0].value < -8000 ? -1 : 0;
        live.axisDirY = live.axes[1].value > 8000 ? 1 : live.axes[1].value < -8000 ? -1 : 0;
    }
    void setHat(const std::string &v) {
        live.hats[0].value = v == "left" ? -1 : v == "right" ? 1 : 0;
        live.hats[1].value = v == "up" ? -1 : v == "down" ? 1 : 0;
    }

    void centerSticks() { for (int i : {0, 1, 3, 4}) live.axes[i].value = 0; }
    void scriptReset() { live.keys.reset(); live.logicalKeys.reset(); centerSticks(); rawStart_ = -1.0f; }

    // One press every half second: down for 0.2 s, up for 0.3 s. Start first, then every answer of the wizard in order;
    // the stick steps deflect an axis to the end. "wizard-abort" answers Start, then holds it past 5 s.
    void runWizardScript(float t) {
        static const int order[] = {0x13b, 0x223, 0x222, 0x221, 0x220, 0x130, 0x131, 0x133, 0x134, 0x136, 0x137, 0x13a,
                                    -1, -2, -4, -5, 0x13c, 0x130, 0x131};   // -1..-5: stick 1 X, stick 1 Y, (none), stick 2 X, stick 2 Y
        live.keys.reset(); live.logicalKeys.reset(); centerSticks();
        const int slot = int(t / 0.5f);
        const float phase = t - float(slot) * 0.5f;
        if (script == "wizard-abort") {
            if (t > 0.5f && t < 7.5f) set1(0x13b);          // hold Start for 7 s after it answered step 0
            else if (t >= 0.2f && t < 0.4f) set1(0x13b);
            return;
        }
        if (slot >= int(sizeof(order) / sizeof(order[0])) || phase < 0.1f || phase > 0.3f) return;
        const int c = order[slot];
        if (c > 0) { set1(c); return; }
        // axes: -1 stick 1 horizontal (ABS_X), -2 stick 1 vertical (ABS_Y), -4 stick 2 horizontal (ABS_RX), -5 stick 2 vertical (ABS_RY)
        const int axis = c == -1 ? 0 : c == -2 ? 1 : c == -4 ? 3 : 4;
        live.axes[axis].value = live.axes[axis].maximum;
    }
    void set1(int c) { live.keys.set(size_t(c)); live.logicalKeys.set(size_t(c)); }

    // The input test script: every control in turn, then a stick circle.
    void runTestScript(float t) {
        static const int order[] = {0x220, 0x223, 0x221, 0x222, 0x130, 0x131, 0x133, 0x134, 0x136, 0x137, 0x138, 0x139, 0x13a, 0x13b, 0x13c};
        live.keys.reset(); live.logicalKeys.reset();
        const int slot = int(t / 0.5f) % 18;
        if (slot < int(sizeof(order) / sizeof(order[0]))) set1(order[slot]);
        const float a = t * 2.0f;
        live.axes[0].value = int(std::cos(a) * 24000.0f);
        live.axes[1].value = int(std::sin(a) * 24000.0f);
        live.axes[3].value = int(std::cos(a * 0.7f) * 18000.0f);
        live.axes[4].value = int(std::sin(a * 0.7f) * 18000.0f);
        if (analogTriggers) { live.axes[2].value = int((std::sin(a) * 0.5f + 0.5f) * 255.0f); live.axes[5].value = int((std::cos(a) * 0.5f + 0.5f) * 255.0f); }
    }

    std::vector<ControllerInfo> devices_, cache_;
    float time_ = 0.0f, rawStart_ = -1.0f, scanTime_ = 0.0f;
    bool unplugged_ = false, bluetoothScanning_ = false, btDeviceAdded_ = false;
    std::vector<std::string> pendingLines_;
};

}  // namespace patsgui
