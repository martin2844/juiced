#pragma once
// What the particles' files share of their own helpers.

#include "Game/Scene/Particles/Emitter.h"

#include "Core/Base/X87.h"
#include "Game/Scene/Particles.h"

#include <bit>

namespace oj::scene::particles {

inline constexpr float kTwoPi = 6.2831855f;
inline constexpr float kEpsilon = 1.1920929e-07f;

// 0x00506AE0: the frequency made whole (cvtss2si with the SSE path, [0x0079E5F8] & 2, rounding to the nearest; x87
// cut otherwise; here the nearest) for the sound.
inline void soundFrequency(audio::Instance* sound, float hz) {
    if (sound != nullptr) sound->setFrequency(static_cast<uint32_t>(x87::nearest(hz)));
}

// 0x005CA370 (and cvtss2si): to the nearest.
inline int32_t nearest(float v) {
    return static_cast<int32_t>(std::nearbyint(v));
}

inline const float kRandomScale = std::bit_cast<float>(0x38000100u);  // 1 / 32767 (0x006FF7EC)

}  // namespace oj::scene::particles
