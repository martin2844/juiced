#pragma once

// What the race draws in each light case (Render/LightCase), in one table (the user, 2026-10-28: a set of each case,
// tuned and grown apart): WORLD LIGHT ULTRA's passes (the sun's dynamic shadows, the cars' shadows from the street
// lamps, the lights in the road, the light in the haze) and the headlights' share of their strength. The pixel
// shaders' variants of the case are the device's (IRenderDevice::setLightCase). A case more is a row here, a LightCase
// and a row of JuicedConfigExt's WorldLight.py CASES.

#include "Render/LightCase.h"

namespace oj::scene {

struct LightSet {
    bool sunCascades = false;  // the sun's dynamic shadows (Game/Scene/SunShadows) in place of the light tiles' sun
    bool lampShadows = false;  // the cars' shadows from the street lamps (Game/Scene/LampShadows)
    bool roadLights = false;   // the lights in the road (IRenderDevice::lightReflections)
    bool hazeLight = false;    // the lamps' cones and the headlights' beams in the haze (Game/Scene/VolumetricLight)
    float haze = 0.0f;         // the haze's thickness: 0 the dry's, 1 the wet's
    // The headlights' share of their strength (HEADLIGHTS HI and ULTRA; the cars' lights are the game's, on by night
    // and in the rain, and by a dry day in a tunnel): by day in the rain the daylight about them.
    float headlights = 0.0f;
    // The cars' flames' light's share (WORLD LIGHT HI and ULTRA, Game/Scene/FlameLights): weak in the daylight.
    float flames = 1.0f;
    // The procedural clouds (WORLD LIGHT ULTRA, Game/Scene/Clouds): the share of the sky they cover, their shadows'
    // depth on the ground (1 a dry day's sun's, 0 none: the map even), their opacity.
    float cloudCover = 0.4f;
    float cloudShadows = 0.0f;
    float cloudOpacity = 1.0f;
};

// The case of the race's light: by a dry day with cars' headlights shown (in a tunnel) the tunnel's.
render::LightCase lightCase(bool night, bool wet, bool headlights = false);
// A case's set.
const LightSet& lightSet(render::LightCase c);

}  // namespace oj::scene
