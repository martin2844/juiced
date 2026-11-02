#pragma once

// The cars' exhaust and nitro flames as lights (the user, 2026-10-28: the nitro's flame lights the road a little;
// WORLD LIGHT HI and ULTRA): each shown flame (its mesh exhaust_flame, the car's effects' flameShown) a point light at
// its node, flickering, by the light case's share (Game/Scene/LightSets: weak in the daylight); the view's surfaces lit
// by JuicedConfigExt's Shared/PointLightPass from the ambient occlusion's depth (IRenderDevice::pointLights). Its
// colour is the flames' own: the car's neon (the user, 2026-10-29: the light should repeat the flame's colour;
// TrackViewParticles' flames) mixed into the warm orange the flames' meshes are drawn in (flameLightColour).

#include "Core/Base/Matrix.h"
#include "Core/Cars/Car.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>
#include <vector>

namespace oj::render {
class Camera;
}

namespace oj::scene {

constexpr std::array<float, 3> kFlameColour = {1.0f, 0.55f, 0.22f};  // a warm orange, the flames' meshes' own
constexpr float kFlameStrength = 1.6f;
constexpr float kFlameReach = 700.0f;    // 7 m
constexpr float kFlameFalloff = 120.0f;  // its light halved at 1.2 m
constexpr size_t kFlameLights = 8;
// The share of the flame's own colour in the light's (the user, 2026-10-29: the warm orange tinted by the neon; 0 the
// orange alone, as the light was, the debug builds' --flame-colour).
constexpr float kFlameColourShare = 0.5f;

// The flames' colour of a car: `kFlameColour` with the flame's own (the car's neon, each of r, g and b at least 96 of
// 255: Game/Race/TrackView/TrackViewParticles' flames) mixed into it by `share` (0 .. 1), kept at the orange's
// brightness, so the light's hue moves and its strength does not (a car that never had a neon fitted keeps the game's
// default, 0xFFFF0000: a red-tinted light).
std::array<float, 3> flameLightColour(const Car& record, float share);
// A flame's light at its node's world matrix: `colour` by `strength` (the case's share) and its flicker (0.75 .. 1 by
// the clock's `seconds` and the flame's `seed`: no draw of the game's generators).
render::PointLight flameLight(const Mat34& node, const std::array<float, 3>& colour, float strength, float seconds,
                              uint32_t seed);
// The view of the pass: the camera's half angles' tangents and rows, the depth's range, the lights nearest the camera
// (kFlameLights at most).
render::PointLightView flameView(render::Camera& camera, std::vector<render::PointLight> lights);

}  // namespace oj::scene
