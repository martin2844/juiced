#include "Game/Scene/FlameLights.h"

#include "Game/Scene/AmbientOcclusion.h"
#include "Render/Camera.h"

#include <algorithm>
#include <cmath>

namespace oj::scene {

namespace {

// A colour's brightness as the eye's (Rec. 601), for the tint that keeps the light's strength.
float luminance(const std::array<float, 3>& c) {
    return 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
}

}  // namespace

std::array<float, 3> flameLightColour(const Car& record, float share) {
    const float mix = std::clamp(share, 0.0f, 1.0f);
    if (mix <= 0.0f) return kFlameColour;
    // The flames' own colour: the car's neon, each of r, g and b at least 96 of 255 (the stock's flames,
    // Game/Race/TrackView/TrackViewParticles).
    std::array<float, 3> tint{};
    for (size_t k = 0; k < 3; ++k) {
        const float channel = static_cast<float>((record.neon >> (16u - 8u * k)) & 0xFFu);
        tint[k] = (channel < 96.0f ? 96.0f : channel) / 255.0f;
    }
    // At the orange's brightness: the light keeps its strength and only its hue moves.
    const float lum = luminance(tint);
    if (lum > 0.0f) {
        const float scale = luminance(kFlameColour) / lum;
        for (float& c : tint) c *= scale;
    }
    std::array<float, 3> out{};
    for (size_t k = 0; k < 3; ++k) out[k] = kFlameColour[k] * (1.0f - mix) + tint[k] * mix;
    return out;
}

render::PointLight flameLight(const Mat34& node, const std::array<float, 3>& colour, float strength, float seconds,
                              uint32_t seed) {
    // Two waves of the flame's own phase: 0.75 .. 1.
    const float phase = static_cast<float>(seed % 97u) * 0.37f;
    const float flicker =
        0.875f + 0.0625f * (std::sin(seconds * 31.0f + phase) + std::sin(seconds * 47.0f + 2.1f * phase));
    render::PointLight l;
    l.place = {node[3], node[7], node[11]};
    l.reach = kFlameReach;
    l.falloff = kFlameFalloff;
    for (size_t k = 0; k < 3; ++k) l.colour[k] = colour[k] * kFlameStrength * strength * flicker;
    return l;
}

render::PointLightView flameView(render::Camera& camera, std::vector<render::PointLight> lights) {
    const Mat44& p = camera.projection();
    render::PointLightView v;
    v.tanX = p[0] != 0.0f ? 1.0f / p[0] : 1.0f;
    v.tanY = p[5] != 0.0f ? 1.0f / p[5] : 1.0f;
    v.range = kOcclusionRange;
    const Mat34& c = camera.world();
    for (size_t r = 0; r < 3; ++r) {
        for (size_t k = 0; k < 4; ++k) v.camera[r][k] = c[4 * r + k];
    }
    // The nearest the camera first.
    const auto distance = [&](const render::PointLight& l) {
        const float dx = l.place[0] - c[3], dy = l.place[1] - c[7], dz = l.place[2] - c[11];
        return dx * dx + dy * dy + dz * dz;
    };
    std::stable_sort(lights.begin(), lights.end(),
                     [&](const auto& a, const auto& b) { return distance(a) < distance(b); });
    v.count = static_cast<int32_t>(std::min(lights.size(), v.lights.size()));
    for (size_t i = 0; i < static_cast<size_t>(v.count); ++i) v.lights[i] = lights[i];
    return v;
}

}  // namespace oj::scene
