#include "Game/Scene/VolumetricLight.h"

#include "Game/Scene/AmbientOcclusion.h"

#include <algorithm>
#include <tuple>

namespace oj::scene {

// The lamps and the beams fit the pass's lights.
static_assert(kHazeLamps + kHazeBeams <= std::tuple_size_v<decltype(render::VolumetricLightView::lights)>);

Vec4 hazeCentre(const Mat34& camera) {
    // The look is the matrix's third column.
    return {camera[3] + camera[2] * kHazeLampsAhead, camera[7] + camera[6] * kHazeLampsAhead,
            camera[11] + camera[10] * kHazeLampsAhead, 0.0f};
}

int32_t hazeSteps(const Mat34& camera, const Vec4& lamp) {
    const float dx = lamp[0] - camera[3], dy = lamp[1] - camera[7], dz = lamp[2] - camera[11];
    return dx * dx + dy * dy + dz * dz < kHazeNear * kHazeNear ? 8 : kHazeFarSteps;
}

render::VolumetricLight lampCone(const Vec4& lamp, float strength, int32_t steps) {
    render::VolumetricLight l;
    l.place = {lamp[0], lamp[1], lamp[2]};
    l.reach = kLampConeReach;
    l.axis = {0.0f, -1.0f, 0.0f};
    l.steps = steps;
    l.side = {1.0f / kLampConeTan, 0.0f, 0.0f};
    l.up = {0.0f, 0.0f, 1.0f / kLampConeTan};
    for (size_t i = 0; i < 3; ++i) l.colour[i] = kLampConeColour[i] * strength;
    l.falloff = kLampConeFalloff;
    return l;
}

render::VolumetricLight headlightBeam(const HeadlightFrame& frame, float strength) {
    render::VolumetricLight l;
    l.place = frame.place;
    l.reach = kBeamReach;
    l.axis = frame.beam;
    for (size_t i = 0; i < 3; ++i) {
        l.side[i] = frame.side[i] / kBeamTanWidth;
        l.up[i] = frame.up[i] / kBeamTanHeight;
        l.colour[i] = kBeamColour[i] * strength;
    }
    l.falloff = kBeamFalloff;
    l.forward = kBeamForward;
    return l;
}

render::VolumetricLightView volumetricView(render::Camera& camera, const std::vector<render::VolumetricLight>& lights,
                                           float wetness) {
    const Mat44& p = camera.projection();
    render::VolumetricLightView v;
    v.tanX = p[0] != 0.0f ? 1.0f / p[0] : 1.0f;
    v.tanY = p[5] != 0.0f ? 1.0f / p[5] : 1.0f;
    v.range = kOcclusionRange;
    const Mat34& c = camera.world();
    for (size_t r = 0; r < 3; ++r) {
        for (size_t k = 0; k < 4; ++k) v.camera[r][k] = c[4 * r + k];
    }
    const float wet = std::clamp(wetness, 0.0f, 1.0f);
    v.density = kHazeDry + (kHazeWet - kHazeDry) * wet;
    v.metre = 100.0f;
    const size_t count = std::min(lights.size(), v.lights.size());
    for (size_t i = 0; i < count; ++i) v.lights[i] = lights[i];
    v.count = static_cast<int32_t>(count);
    return v;
}

}  // namespace oj::scene
