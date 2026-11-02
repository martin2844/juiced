#include "Game/Scene/AmbientOcclusion.h"

#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"
#include "Game/Scene/TrackScene.h"
#include "Render/Renderer3d.h"

#include <algorithm>
#include <cmath>

namespace oj::scene {

namespace {

constexpr uint32_t kCullNone = 1;  // D3DCULL_NONE: a fence's single side and a mesh's back alike

}  // namespace

uint32_t occlusionSize(uint32_t view) {
    return std::max(view / 2, 16u);
}

Mat44 nearerToClip(const Mat44& worldToClip, const Vec4& eye, float reach) {
    const float s = 1.0f - reach;
    const Mat34 nearer = {s, 0.0f, 0.0f, eye[0] * reach, 0.0f, s, 0.0f, eye[1] * reach,
                          0.0f, 0.0f, s, eye[2] * reach};
    return multiplyAffine(worldToClip, nearer);
}

render::AmbientOcclusionView occlusionView(const render::Camera& camera) {
    const Mat44& p = camera.projection();
    render::AmbientOcclusionView v;
    v.tanX = p[0] != 0.0f ? 1.0f / p[0] : 1.0f;
    v.tanY = p[5] != 0.0f ? 1.0f / p[5] : 1.0f;
    v.range = kOcclusionRange;
    v.radius = kOcclusionRadius;
    v.strength = kOcclusionStrength;
    v.bias = kOcclusionBias;
    v.fadeFrom = kOcclusionFadeFrom;
    v.fadeLength = kOcclusionFadeLength;
    v.foliageRadius = kFoliageRadius;
    v.foliageStrength = kFoliageStrength;
    v.foliageFadeFrom = kFoliageFadeFrom;
    v.foliageFadeLength = kFoliageFadeLength;
    return v;
}

render::LightReflectionView reflectionView(render::Camera& camera, float wetness) {
    const Mat44& p = camera.projection();
    render::LightReflectionView v;
    v.tanX = p[0] != 0.0f ? 1.0f / p[0] : 1.0f;
    v.tanY = p[5] != 0.0f ? 1.0f / p[5] : 1.0f;
    v.range = kOcclusionRange;
    v.reach = kReflectionReach;
    // The world's up (y) along the camera's side, up and ahead (its matrix's columns).
    const Mat34& c = camera.world();
    v.up = {c[4], c[5], c[6]};
    v.wetness = std::clamp(wetness, 0.0f, 1.0f);
    return v;
}

bool lampStreak(render::Camera& camera, const Vec4& lamp, float ground, float strength,
                const std::array<float, 3>& colour, std::array<float, 8>& out) {
    const Mat44& m = camera.worldToClip();
    const auto onView = [&m](float x, float y, float z, float& u, float& v) {
        const float w = m[12] * x + m[13] * y + m[14] * z + m[15];
        if (w <= 1.0f) return false;
        u = (m[0] * x + m[1] * y + m[2] * z + m[3]) / w * 0.5f + 0.5f;
        v = 0.5f - (m[4] * x + m[5] * y + m[6] * z + m[7]) / w * 0.5f;
        return true;
    };
    // As far under the road as the lamp is above it.
    const float mirror = ground - std::fabs(lamp[1] - ground);
    const Mat34& c = camera.world();
    float u = 0.0f, v = 0.0f, footU = 0.0f, footV = 0.0f, sideU = 0.0f, sideV = 0.0f;
    if (!onView(lamp[0], mirror, lamp[2], u, v) || !onView(lamp[0], ground, lamp[2], footU, footV) ||
        !onView(lamp[0] + c[0] * kStreakWidth, mirror + c[4] * kStreakWidth, lamp[2] + c[8] * kStreakWidth, sideU,
                sideV)) {
        return false;
    }
    const float width = std::max(std::fabs(sideU - u), 0.002f);
    if (footV > 1.0f || u + 3.0f * width < 0.0f || u - 3.0f * width > 1.0f) return false;
    const float dx = lamp[0] - c[3], dz = lamp[2] - c[11];
    const float fade = std::clamp(1.0f - std::sqrt(dx * dx + dz * dz) / kStreakReach, 0.0f, 1.0f);
    const float k = std::clamp(strength, 0.0f, 1.0f) * kStreakStrength * fade;
    if (k <= 0.0f) return false;
    out = {u, v, width, std::max((v - footV) * kStreakLength, 0.01f), colour[0] * k, colour[1] * k, colour[2] * k,
           footV};
    return true;
}

AmbientOcclusion::AmbientOcclusion(render::IRenderDevice& device) : m_device(device), m_shaders(device) {}

AmbientOcclusion::~AmbientOcclusion() {
    if (m_target != 0) m_device.destroyTexture(m_target);
}

render::TextureId AmbientOcclusion::drawDepth(TrackScene& scene, render::Camera& camera, uint32_t width,
                                              uint32_t height, bool marks) {
    if (!usable()) return 0;
    const uint32_t w = occlusionSize(width), h = occlusionSize(height);
    if (m_target == 0 || m_width != w || m_height != h) {
        if (m_target != 0) m_device.destroyTexture(m_target);
        m_target = m_device.createFloatTarget(w, h);
        m_width = w;
        m_height = h;
    }
    if (m_target == 0) return 0;
    render::Renderer3d& r = scene.engine().renderer();
    r.endMaterial();
    scene.engine().postEffects().forgetTarget();
    const render::Renderer3d::ShadowShaders kept = r.shadowShaders;
    r.shadowShaders = m_shaders.shaders(kCullNone, true);
    m_device.setRenderTarget(m_target, 0);
    m_device.clear(render::ClearFlags::Target | render::ClearFlags::ZBuffer, 0xFFFFFFFFu, 1.0f, 0);
    camera.update();
    const Mat34& c = camera.world();
    const float constants[8] = {c[3], c[7], c[11], 1.0f / kOcclusionRange, c[2], c[6], c[10], 0.0f};
    r.setVertexConstants(240, constants, 2);
    scene.drawShadowCasters(camera, nullptr, TrackScene::kOcclusionMarks);
    if (marks) scene.markGround(camera);
    r.shadowShaders = kept;
    r.endMaterial();
    m_device.setRenderTarget(0, 0);
    return m_target;
}

}  // namespace oj::scene
