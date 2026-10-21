#pragma once
// A race car's effects object (the vehicle's +0x64, 0x0048FA40 / 0x0048FEA0; docs/spec/20-effects.md): the emitters it
// made from the race's effects and what it keeps between frames. An emitter is held by its definition (none when the
// effect was not made); the particles are the race's (Game/Scene/Particles).

#include "Core/Data/EffectDefs.h"
#include "Core/Base/Matrix.h"
#include "Core/Tracks/SunMap.h"

#include <array>
#include <map>

namespace oj {

struct CarEffects {
    bool made = false;                              // the vehicle's +0x64 exists
    bool wet = false;                               // +0xF0: the atmosphere's Wet
    float flameRamp = 0.0f;                         // +0x50: the flames' fade in, 3 a second up to 1
    float scale = 0.4f;                             // +0x54: 1 in a race of type 4, else 0.4
    float sparks = 0.0f;                            // +0x58: the scrape's sparks, falling by 5 a second
    float flameLevel = 0.0f;                        // +0x5C: the exhaust's flames, from the engine's speed on nitro
    std::array<const EffectDef*, 2> flames{};       // +0x60: "nitro" at each exhaust
    std::array<const EffectDef*, 4> skid{};         // +0x70: each wheel's skid of its surface
    std::array<const EffectDef*, 4> spray{};        // +0x80: wheel_sand (wheel_spray when wet)
    const EffectDef* squeal = nullptr;              // +0x90: the tyres' squeal of the surface
    float squealPitch = 0.0f;                       // +0x94: ±4000 drawn while it is 0
    std::array<const EffectDef*, 4> tarmacSkid{};   // +0x98
    const EffectDef* tarmacSqueal = nullptr;        // +0xA8
    std::array<const EffectDef*, 4> sandSkid{};     // +0xAC
    std::array<float, 4> sprayLevel{};              // +0xBC
    const EffectDef* sandSqueal = nullptr;          // +0xCC
    const EffectDef* explodeSparks = nullptr;       // +0xD0
    const EffectDef* scrapeSparks = nullptr;        // +0xD4: SPARKS of TARMAC_DRY
    const EffectDef* explodeSmoke = nullptr;        // +0xD8
    const EffectDef* chassisSmoke = nullptr;        // +0xDC
    const EffectDef* flakes = nullptr;              // +0xE0: FLAKES of TARMAC_DRY
    const EffectDef* heatHaze = nullptr;            // +0xE4
    const EffectDef* tracks = nullptr;              // +0xE8: tracks_tarmac (none when wet)
    float smoke = 0.0f;                             // +0xEC: a quarter of the wheels' smoke levels
    // The tyre tracks' frame (0x004907D0): the TRACKS of each surface place (0x00503800 by the surface block's name,
    // none for a place without a block), a wheel whose surface changed asks its track for its surface's, each wheel's
    // mark put off (not in the race: up, or a replay going fast forward) and each wheel down marked with its two
    // levels (the axle's pair: the left side's first).
    std::array<const EffectDef*, 10> surfaceTracks{};
    std::array<const EffectDef*, 4> tracksAsked{};
    std::array<bool, 4> tracksAsk{};
    std::array<bool, 4> trackOff{};
    std::array<bool, 4> trackOn{};
    std::array<std::array<float, 2>, 4> trackLevels{};
    // 0x004907D0 each frame: a wheel down in the race (no fast forward) gives its skid emitter its level (and the spray
    // its own); else the wheel's effects are put off.
    std::array<bool, 4> wheelOn{};
    std::array<float, 4> skidLevel{};
    const SunMap* puddles = nullptr;                // [[0x0075EC94]+0x34]+0x84: the route's .pqh
    float roadWetness = 1.0f;                       // the atmosphere's +0x18C
    bool fastForward = false;                       // [G+0x9C]+0x7C: the replay going fast forward
    // 0x004912D0: each exhaust's flame on (the level over 0.005) and the level given to its emitter (the level x the
    // fade in, before they move on).
    std::array<bool, 2> flameOn{};
    std::array<float, 2> flameEmit{};
    // The flames' meshes (exhaust_flame, a mesh node under each exhaust's node made with its flame, 0x00556AE0; +0x68):
    // the four values each node drew as it was made (0x005550D0); while the flame is on shown, its matrix a turn of
    // 45 degrees about z times the scales, 35 cm behind the exhaust (0x004914B9), else hidden (+0xC0 | 4).
    std::array<Vec4, 2> flameLook{};
    std::array<bool, 2> flameShown{};
    std::array<Mat34, 2> flameMatrix{};
    // 0x004916F0, on the frames it runs (every other one): a hit (the energy at least 50) gives the explosion's
    // emitters and the scrape's the contacts' mean arm and their levels; none puts the explosion's off. The scrape's
    // emitters take the scrape's level (+0x58) as it was before it fell.
    bool impactRan = false;
    bool impactHit = false;
    Vec4 impactArm{};
    float impactLevel = 0.0f;
    float impactScrape = 0.0f;
    float impactSpeed = 0.0f;  // the speed against the other side (m/s), for the sounds' frequencies
    // The values drawn for the looping sounds of the emitters as they were made (0x0050CE70), by the place of their
    // field in the object, for the game's sound instances.
    std::map<size_t, int32_t> soundDraws;
    int32_t soundDraw(const void* field) const {
        const auto at = static_cast<size_t>(static_cast<const char*>(field) - reinterpret_cast<const char*>(this));
        const auto found = soundDraws.find(at);
        return found != soundDraws.end() ? found->second : -1;
    }
};

}  // namespace oj
