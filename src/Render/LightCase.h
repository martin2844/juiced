#pragma once

// The race's light cases (WORLD LIGHT ULTRA's sets, the user 2026-10-28: a set of each case, each within the limits of
// the shaders 3.0, tuned and grown apart): a dry day, a wet day, a dry night, a wet night, and a dry day with cars'
// lights on in a tunnel (Game/Scene/TunnelLights). Their bits are the shader
// table's (JuicedConfigExt's WorldLight.py CASES, WorldLightShaders::Case): a pixel shader's variant serves some cases,
// the device binds the race's case's (IRenderDevice::setLightCase). What a case draws: Game/Scene/LightSets.

#include <array>
#include <cstddef>

namespace oj::render {

enum class LightCase : unsigned { DayDry = 1, DayWet = 2, NightDry = 4, NightWet = 8, DayTunnel = 16 };

constexpr std::array<LightCase, 5> kLightCases{LightCase::DayDry, LightCase::DayWet, LightCase::NightDry,
                                               LightCase::NightWet, LightCase::DayTunnel};

// The case a shader is made for first; the other cases' variants are bound in its place in their case.
constexpr LightCase kBaseLightCase = LightCase::NightDry;

// A case's place in kLightCases.
constexpr size_t lightCaseIndex(LightCase c) {
    for (size_t i = 0; i < kLightCases.size(); ++i) {
        if (kLightCases[i] == c) return i;
    }
    return 0;
}

}  // namespace oj::render
