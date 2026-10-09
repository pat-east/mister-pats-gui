#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

// MiSTer's own controller identity, in the two forms different parts of it use. See
// docs/CONTROLLER.md ("Identifying a controller") for why this has to be the evdev-reported
// vendor/product, not whatever lsusb prints for the same physical hardware.
namespace ControllerId {

// The 8-hex-digit form `deadzone=` uses: vendor and product packed into one uint32,
// e.g. vendor 0x045e, product 0x02a1 -> 0x045e02a1.
inline uint32_t vidPid(uint16_t vendor, uint16_t product) {
    return (uint32_t(vendor) << 16) | uint32_t(product);
}

// The `<vid>_<pid>` form the `.map` filename (`input_<idstr>_v3.map`) uses — lower-case,
// zero-padded to 4 hex digits each, joined with an underscore. Deliberately not extended
// with a connection-identity suffix here: `controller_unique_mapping` is on the roadmap for
// later, not v1 (see docs/CONTROLLER.md, Non-goals).
inline std::string idstr(uint16_t vendor, uint16_t product) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04x_%04x", vendor, product);
    return buffer;
}

} // namespace ControllerId
