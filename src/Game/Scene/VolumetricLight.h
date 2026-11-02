#pragma once
// The fixed build's light in the night's haze (WORLD LIGHT ULTRA by night, docs/fixes.md Added; the user, 2026-10-28:
// volumetric light, the lamps' cones and the headlights' beams): the street lamps nearest the view (those of the cars'
// shadows from the lamps, Game/Scene/LampShadows) as cones down and the shown cars' headlights (Game/Scene/Headlights,
// their low beam's frame) as narrower beams ahead, each scattering its light toward the camera along the view's rays
// to their surfaces (the ambient occlusion's depth) in a haze, thicker in the rain; the device's pass
// (IRenderDevice::volumetricLight, JuicedConfigExt's VolumetricLightPass) adds it over the view after the lights'
// reflections in the road (TrackScene::draw).

#include "Core/Base/Matrix.h"
#include "Game/Scene/Headlights.h"
#include "Render/Camera.h"
#include "Render/RenderDevice.h"

#include <array>
#include <vector>

namespace oj::scene {

// A street lamp's cone: straight down, kLampConeTan across and up, kLampConeReach, a warm white falling off over
// kLampConeFalloff.
constexpr float kLampConeTan = 1.2f;            // 50 degrees
constexpr float kLampConeReach = 1400.0f;       // 14 m
constexpr float kLampConeFalloff = 500.0f;
constexpr std::array<float, 3> kLampConeColour{1.0f, 0.8f, 0.55f};
// A headlight's beam: the low beam's frame narrowed to what the haze shows (kBeamTanWidth across, kBeamTanHeight up),
// kBeamReach, its light falling off over kBeamFalloff, a cool white by the headlights' strength.
constexpr float kBeamTanWidth = 0.35f;
constexpr float kBeamTanHeight = 0.12f;
constexpr float kBeamReach = 4000.0f;           // 40 m
constexpr float kBeamFalloff = 1500.0f;
constexpr std::array<float, 3> kBeamColour{0.9f, 0.92f, 1.0f};
// A beam's scattering forward (Henyey-Greenstein's g; the user, 2026-10-29: the own car's beams seen from behind made
// a white wall where they met a wall ahead; rain's drops scatter forward: seen from behind weaker, head-on stronger).
constexpr float kBeamForward = 0.5f;
// The haze's density over a metre: a dry night's and a wet one's (the user, 2026-10-29: the lamps' cones by a dry night
// too, a little weaker than in the rain; they had been at 0.03, hardly seen).
constexpr float kHazeDry = 0.07f;
constexpr float kHazeWet = 0.1f;
// The lights in the haze: the street lamps within kHazeLampsReach of a point kHazeLampsAhead along the first player's
// camera's look (kHazeLamps nearest it), each by its distance's fade past kHazeLampsFull and by its lead over the
// nearest left out (Game/Scene/NearestFade, kHazeLampsBand; the user, 2026-10-28: the lamps' light came and went at
// once about the camera, only the nearest few lit while the street's lamps shone on far ahead), those past kHazeNear
// of the camera in kHazeFarSteps (small on the view); the shown headlights nearest the camera (kHazeBeams).
constexpr float kHazeLampsAhead = 3000.0f;     // 30 m
constexpr float kHazeLampsReach = 15000.0f;    // 150 m
constexpr float kHazeLampsFull = 12000.0f;     // 120 m
constexpr float kHazeLampsBand = 1000.0f;      // 10 m
constexpr size_t kHazeLamps = 20;
constexpr float kHazeNear = 3000.0f;           // 30 m
constexpr int32_t kHazeFarSteps = 4;
constexpr size_t kHazeBeams = 4;
Vec4 hazeCentre(const Mat34& camera);
// A lamp's steps from the camera of world matrix `camera`: eight within kHazeNear, kHazeFarSteps past it.
int32_t hazeSteps(const Mat34& camera, const Vec4& lamp);

// A lamp's cone, its colour by `strength`.
render::VolumetricLight lampCone(const Vec4& lamp, float strength, int32_t steps);
render::VolumetricLight headlightBeam(const HeadlightFrame& frame, float strength);
// The pass's view of a camera (its half angles, the view to the world by its rows, the ambient occlusion's range), the
// haze of a road's wetness (0 .. 1) and the lights (render::VolumetricLightView's lights at most).
render::VolumetricLightView volumetricView(render::Camera& camera, const std::vector<render::VolumetricLight>& lights,
                                           float wetness);

}  // namespace oj::scene
