#pragma once
// What the material classes share: how they read their parameters' bytes.

#include "Render/Materials.h"

#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace oj::render::materials {

inline constexpr float kOver511 = 1.0f / 511.0f;
inline constexpr float kOver255 = 1.0f / 255.0f;  // 0x006FF918
inline constexpr float kOver64 = 1.0f / 64.0f;    // 0x006FFAF4
inline constexpr float kOver128 = 1.0f / 128.0f;  // 0x006FFC14
inline const float kRandomScale = std::bit_cast<float>(0x38000100u);  // 1 / 32767 (0x006FF7EC)
inline const float kPhaseScale = std::bit_cast<float>(0x39490FE8u);   // 2 pi / 32767 (0x006FFAF0)

inline float asFloat(const void* value) {
    float f = 0.0f;
    std::memcpy(&f, value, 4);
    return f;
}

inline uint32_t asBits(const void* value) {
    uint32_t v = 0;
    std::memcpy(&v, value, 4);
    return v;
}

// 0x005CA370: fistp in the rounding mode the main thread runs with (to nearest).
inline int32_t roundNearest(float v) {
    return static_cast<int32_t>(std::nearbyint(v));
}

// The colour x 255 rounded as an ARGB value (the classes' TEXTUREFACTOR of a fixed-function technique).
inline uint32_t factorOf(const float* rgba) {
    auto byteOf = [](float v) { return static_cast<uint32_t>(roundNearest(v * 255.0f)) & 0xFF; };
    return (byteOf(rgba[3]) << 24) | (byteOf(rgba[0]) << 16) | (byteOf(rgba[1]) << 8) | byteOf(rgba[2]);
}

}  // namespace oj::render::materials
