#include "Game/Scene/LightSets.h"

#include <iterator>

namespace oj::scene {

namespace {

// In kLightCases' order. A dry day: the sun's cascades (no headlight: the cars' lights are off). A wet day: an overcast
// sky (the race's wet light tiles hold no sun's shadow, the stock's rain casts none), the headlights at half. By night
// the street lamps' shadows and the light in the haze, thicker in the wet; the lights in the road only in the wet (a
// dry road looked wet with them, the user 2026-10-28). A dry day with cars' lights on in a tunnel: the sun's cascades
// (the tunnel's roof) and the lights as by night. The clouds: scattered by a dry day, their shadows the sun's; an
// overcast sky in the rain, its shadows soft; by night thinner and no shadow.
constexpr LightSet kSets[] = {
    // DayDry
    {.sunCascades = true, .flames = 0.3f, .cloudCover = 0.45f, .cloudShadows = 1.0f, .cloudOpacity = 0.95f},
    // DayWet
    {.haze = 1.0f, .headlights = 0.5f, .flames = 0.5f, .cloudCover = 0.92f, .cloudShadows = 0.35f},
    // NightDry
    {.lampShadows = true, .hazeLight = true, .headlights = 1.0f, .cloudCover = 0.35f, .cloudOpacity = 0.6f},
    // NightWet
    {.lampShadows = true,
     .roadLights = true,
     .hazeLight = true,
     .haze = 1.0f,
     .headlights = 1.0f,
     .cloudCover = 0.9f,
     .cloudOpacity = 0.85f},
    // DayTunnel
    {.sunCascades = true, .headlights = 1.0f, .flames = 0.8f, .cloudCover = 0.45f, .cloudShadows = 1.0f,
     .cloudOpacity = 0.95f},
};
static_assert(std::size(kSets) == render::kLightCases.size());

}  // namespace

render::LightCase lightCase(bool night, bool wet, bool headlights) {
    using render::LightCase;
    if (night) return wet ? LightCase::NightWet : LightCase::NightDry;
    if (wet) return LightCase::DayWet;
    return headlights ? LightCase::DayTunnel : LightCase::DayDry;
}

const LightSet& lightSet(render::LightCase c) {
    return kSets[render::lightCaseIndex(c)];
}

}  // namespace oj::scene
