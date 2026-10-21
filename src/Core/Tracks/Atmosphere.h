#pragma once
// .atm, the atmosphere of a 3D scene (docs/spec/13-atmosphere.md): the lights, the fog, the sky dome's colours and
// the sun of a time of day. A file holds one or more blocks [BeginAtmosphere] .. [EndAtmosphere]; the last one read
// is the one in use.

#include "Core/Base/Random.h"

#include <cfloat>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

// The atmosphere object (0x1C8 bytes, 0x00531E30) with the defaults of 0x00531D00 and [BeginAtmosphere].
struct Atmosphere {
    std::string name;                         // +0x00: the word after [BeginAtmosphere], under 32 characters
    // Stock problem: the ambient and light colours, the directions, the fog colours, FogMin, FogMax, SunFlareSize and
    // +0x1C0 have no default (the memory the object was made in); 0 here. The fourth value of the ambient, the colours
    // and the directions is never read (a value of the stack); 0 here.
    float ambient[4] = {};                    // +0x20
    float lightColour[3][4] = {};             // +0x30, +0x40, +0x50
    float lightDirection[3][4] = {};          // +0x60, +0x70, +0x80
    uint32_t skyColour = 0xFFFFFFFF;          // +0x90
    uint32_t fogFront = 0;                    // +0x94
    uint32_t fogBack = 0;                     // +0x98
    float fogMin = 0.0f;                      // +0x9C
    float fogMax = 0.0f;                      // +0xA0
    float fogNear = 0.0f;                     // +0xA4
    float fogFar = 400000.0f;                 // +0xA8
    float skyAngle = 0.0f;                    // +0xAC, radians
    uint32_t horizonFront = 0xFFFFF7FF;       // +0xB0
    uint32_t horizonBack = 0xFFDAE3E9;        // +0xB4
    uint32_t skyFront = 0;                    // +0xB8
    uint32_t skyBack = 0;                     // +0xBC
    uint32_t zenith = 0xFF377EBE;             // +0xC0
    float skyHeightFactor = 4.0f;             // +0xC4
    float skyHeightManual = 500.0f;           // +0xC8
    std::string sunShape;                     // +0xCC
    std::string sunFlare;                     // +0xEC
    std::string sunReflection;                // +0x10C
    std::string envmap;                       // +0x12C
    float sunSize = 0.0f;                     // +0x14C
    float sunFlareSize = 0.0f;                // +0x150
    float sunReflectionSize = 100.0f;         // +0x154
    float skyLuminosity = 0.25f;              // +0x158
    float sunAngle = 0.0f;                    // +0x15C, radians
    uint32_t exposureEnable = 1;              // +0x160
    float exposureBias = 0.0f;                // +0x164
    float exposureMin = -128.0f;              // +0x168
    float exposureMax = 128.0f;               // +0x16C
    uint32_t night = 0;                       // +0x170
    uint32_t wet = 0;                         // +0x174
    int32_t shadowSet = -1;                   // +0x178
    float lightBlur = 0.5f;                   // +0x17C
    uint32_t worldShadowColour = 0xC8FFFFFF;  // +0x180
    uint32_t worldShadowAdditive = 0;         // +0x184
    float nightRoadShininess = 0.25f;         // +0x188
    float roadWetness = 1.0f;                 // +0x18C: MinRoadWetness
    float minRoadWetness = 1.0f;              // +0x190: MinRoadWetness
    float maxRoadWetness = 1.0f;              // +0x194
    float roadWetnessRate = 0.0f;             // +0x198: 1 / RoadWetnessTime
    float rain = 1.0f;                        // +0x19C: how much it rains, toward 1 while raining, 0 while not
    uint32_t raining = 1;                     // +0x1A0
    float rainTimer = FLT_MAX;                // +0x1A4: the time left until the rain starts or stops
    float minRainOnTime = FLT_MAX;            // +0x1A8
    float maxRainOnTime = FLT_MAX;            // +0x1AC
    float minRainOffTime = FLT_MAX;           // +0x1B0
    float maxRainOffTime = FLT_MAX;           // +0x1B4
    uint32_t puddleColour = 0xFFFFFFFF;       // +0x1B8
    uint32_t ps2BuildingGlassColour = 0;      // +0x1BC
    float drawQuality = 0.0f;                 // +0x1C0: 1 for the one in use; the renderer's +0x188
    float testMipmapBias = 0.0f;              // +0x1C4
};

// The atmospheres of a file in the order read (0x005337A0); empty when the text has no [BeginAtmosphere].
std::vector<Atmosphere> parseAtmospheres(std::string_view text);

// 0x00534300 (the race's frame, after the physics, with its step time): the rain starts and stops after a time drawn
// between the minimum and the maximum of the new state (rand()); its amount follows by half a unit a second, and the
// road gets wetter by the amount x the wetness rate x the time, within the minimum and the maximum wetness.
void stepRain(Atmosphere& a, float dt, CrtRandom& random);

}  // namespace oj
