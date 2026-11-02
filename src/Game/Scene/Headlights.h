#pragma once
// The fixed build's headlights (docs/fixes.md Added): JuicedConfigExt's Headlights, the cars' headlights lighting the
// night road and walls at the game's shaders 3.0. Each car with its headlights shown (its flares HLIGHTON_R and _L on)
// is a light between them shaped as a low beam (JuicedConfigExt's Shared/HeadlightBeam.h: 45 degrees to each side,
// brightest just below a cut-off under the horizon, 100 m); the replaced night shaders (Render/ShaderReplacements,
// ShaderFeature::Headlights) read the first six from the pixel shader's constants c196 .. c220 and the bools b10 .. b15.

#include "Core/Base/Matrix.h"

#include <HeadlightBeam.h>

#include <array>
#include <cstdint>
#include <vector>

namespace oj::render {
class IRenderDevice;
class ProgramContext;
}

namespace oj::scene {

// A car's headlights: the flares' world matrices, its root node's (the forward, side and up of its third, first and
// second column), whether a flare is shown, the light's share of its strength (coming on or going out in a tunnel's
// day, Game/Scene/TunnelLights).
struct CarHeadlights {
    Mat34 right{};
    Mat34 left{};
    Mat34 root{};
    bool shown = false;
    float level = 1.0f;
};

constexpr int kHeadlights = HeadlightBeam::kLights;
constexpr uint32_t kHeadlightConstant = HeadlightBeam::kFirstConstant;  // c196: the colour, then 4 a light
constexpr uint32_t kHeadlightBool = HeadlightBeam::kFirstBool;          // b10 .. b15: the lights on
// A light's frame (the shadow maps' camera too): 45 degrees to each side, 10 up and down (their tangents), 100 m.
constexpr float kHeadlightTanWidth = HeadlightBeam::kTanHalfWidth;
constexpr float kHeadlightTanHeight = HeadlightBeam::kTanHalfHeight;
constexpr float kHeadlightRange = HeadlightBeam::kRange;
// The fixed build's strength of the lights: 75 % of JuicedConfigExt's (the user's tuning, 2026-10-28); the light case's
// share of it (Game/Scene/LightSets).
constexpr float kHeadlightStrength = 0.75f;

// A car's light between its flares: its place, its beam (the car's forward tilted HeadlightBeam::kTilt down), the
// car's side and the beam's up, each unit.
struct HeadlightFrame {
    std::array<float, 3> place{};
    std::array<float, 3> beam{};
    std::array<float, 3> side{};
    std::array<float, 3> up{};
};
HeadlightFrame headlightFrame(const CarHeadlights& car);

struct HeadlightConstants {
    std::array<std::array<float, 4>, 1 + 4 * kHeadlights> constants{};
    std::array<int32_t, kHeadlights> on{};
    int count = 0;
};

// The constants of the first six cars with their headlights shown, at the strength (0 .. 3, the registry's
// HeadlightsStrength / 100): c196 the colour (1, 0.95, 0.85), then each light's (HeadlightBeam::Light) with its shadow
// map's texel width of `texels` (none: no shadow).
HeadlightConstants headlightConstants(const std::vector<CarHeadlights>& cars, float strength,
                                      const std::array<float, kHeadlights>* texels = nullptr);

// The frame's headlights in the night shaders' constants (TrackView::draw): at a strength above 0 the cars' lights
// (headlightConstants, with the shadow maps' texel widths of HEADLIGHTS ULTRA); at 0 after a frame with them every
// light off, once (the graphics options turned them off: the replaced night shaders read the constants until their
// programs are made again). Whether they are set now, the next frame's `wereSet`.
bool setHeadlights(render::IRenderDevice& device, const std::vector<CarHeadlights>& cars, float strength,
                   bool wereSet, const std::array<float, kHeadlights>* texels = nullptr);
// REFLECTIONS ULTRA: the cars' paint lit by the headlights (JuicedConfigExt's CarPaint3, its shared CarHeadlight: the
// constants of headlightConstants, a light off all 0); none when not `lit` or at the strength 0 (the stock's
// headlights).
void setCarHeadlights(render::ProgramContext& programs, const std::vector<CarHeadlights>& cars, float strength,
                      bool lit);

}  // namespace oj::scene
