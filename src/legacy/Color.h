#pragma once

#include <cstdint>

// Canvas pixels are ARGB32 throughout; the framebuffer conversion happens once on present.
struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    static Color rgb(uint32_t hex) {
        return {uint8_t((hex >> 16) & 0xFF), uint8_t((hex >> 8) & 0xFF), uint8_t(hex & 0xFF), 255};
    }

    Color withAlpha(uint8_t alpha) const { return {r, g, b, alpha}; }

    Color scaled(float f) const {
        auto clamp = [](float v) { return uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v)); };
        return {clamp(r * f), clamp(g * f), clamp(b * f), a};
    }

    uint32_t argb() const {
        return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }

    static Color lerp(const Color &from, const Color &to, float t) {
        if (t <= 0) return from;
        if (t >= 1) return to;
        auto mix = [t](uint8_t a0, uint8_t b0) { return uint8_t(a0 + (b0 - a0) * t); };
        return {mix(from.r, to.r), mix(from.g, to.g), mix(from.b, to.b), mix(from.a, to.a)};
    }
};
