#pragma once

// The control surface every sample service shares: `--set key=value` of the host runner (src/app/host) and tests
// set a state, and help() lists the keys. Sample services fake the host's state machines (scan, update, Wi-Fi,
// disc, ...) so a category agent can render and drive every page on the host without a device.

#include <string>
#include <vector>

namespace patsgui {

class SampleControl {
public:
    virtual ~SampleControl() = default;
    // True when the key belongs to this service and the value was understood.
    virtual bool set(const std::string &key, const std::string &value) = 0;
    // One line per key: "library.scan=idle|discovering|scanning|writing|done|failed".
    virtual std::vector<std::string> help() const = 0;
};

inline bool sampleBool(const std::string &v) { return v == "1" || v == "on" || v == "true" || v == "yes"; }

}  // namespace patsgui
