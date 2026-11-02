#include "Game/Scene/LampShadows.h"

#include "Core/Base/Log.h"
#include "Game/Scene/AmbientOcclusion.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"
#include "Game/Scene/TrackScene.h"
#include "Render/Renderer3d.h"

#include <algorithm>
#include <cmath>

namespace oj::scene {

namespace {

constexpr uint32_t kCullNone = 1;  // D3DCULL_NONE: the cars' both sides

}  // namespace

Mat34 lampFrame(const Vec4& place) {
    // Its side the world's x, its up the world's z, its look down (side x up).
    return {1.0f, 0.0f, 0.0f, place[0], 0.0f, 0.0f, -1.0f, place[1], 0.0f, 1.0f, 0.0f, place[2]};
}

Vec4 lampShadowCentre(const Mat34& camera) {
    // The look is the matrix's third column.
    return {camera[3] + camera[2] * kLampShadowAhead, camera[7] + camera[6] * kLampShadowAhead,
            camera[11] + camera[10] * kLampShadowAhead, 0.0f};
}

render::LampShadowView lampShadowView(render::Camera& camera, const std::vector<Vec4>& lamps,
                                      const std::vector<render::Camera*>& cameras) {
    const Mat44& p = camera.projection();
    render::LampShadowView v;
    v.tanX = p[0] != 0.0f ? 1.0f / p[0] : 1.0f;
    v.tanY = p[5] != 0.0f ? 1.0f / p[5] : 1.0f;
    v.range = kOcclusionRange;
    // The view (x along the camera's side, y its up, z its look, 1) to the world: the camera's matrix's rows.
    const Mat34& c = camera.world();
    for (size_t r = 0; r < 3; ++r) {
        for (size_t k = 0; k < 4; ++k) v.camera[r][k] = c[4 * r + k];
    }
    v.strength = kLampShadowStrength;
    v.bias = kLampShadowBias;
    v.texel = 1.0f / static_cast<float>(kLampShadowSize);
    const size_t count = std::min({lamps.size(), cameras.size(), kLampShadows});
    v.count = static_cast<int32_t>(count);
    for (size_t i = 0; i < count; ++i) {
        render::LampShadowLamp& l = v.lamps[i];
        const Mat44& m = cameras[i]->worldToClip();
        for (size_t k = 0; k < 4; ++k) {
            l.clip[0][k] = m[k];
            l.clip[1][k] = m[4 + k];
            l.clip[2][k] = m[12 + k];
        }
        l.place = {lamps[i][0], lamps[i][1], lamps[i][2]};
        l.strength = lamps[i][3];
        l.look = {0.0f, -1.0f, 0.0f};
        l.range = kLampShadowRange;
    }
    return v;
}

LampShadows::LampShadows(render::IRenderDevice& device) : m_device(device), m_shaders(device) {
    for (render::Camera& c : m_cameras) {
        const auto fov = static_cast<float>(std::atan(kLampShadowTan) * 2.0);
        c.setFov(fov, fov, false);
        c.setNear(10.0f);
        c.setFar(kLampShadowRange);
    }
}

LampShadows::~LampShadows() {
    for (const render::TextureId id : m_maps) {
        if (id != 0) m_device.destroyTexture(id);
    }
}

size_t LampShadows::draw(TrackScene& scene, const std::vector<Vec4>& lamps) {
    m_lamps.clear();
    if (!usable()) return 0;
    render::Renderer3d& r = scene.engine().renderer();
    r.endMaterial();
    scene.engine().postEffects().forgetTarget();
    const render::Renderer3d::ShadowShaders kept = r.shadowShaders;
    r.shadowShaders = m_shaders.shaders(kCullNone, false);
    for (size_t i = 0; i < lamps.size() && i < kLampShadows; ++i) {
        if (m_maps[i] == 0) {
            m_maps[i] = m_device.createFloatTarget(kLampShadowSize, kLampShadowSize);
            log::info("lamp shadows: map {} made (its lamp at {:.0f}, {:.0f}, {:.0f})", i, lamps[i][0], lamps[i][1],
                      lamps[i][2]);
        }
        if (m_maps[i] == 0) break;
        render::Camera& camera = m_cameras[i];
        camera.local = lampFrame(lamps[i]);
        camera.invalidate();
        m_device.setRenderTarget(m_maps[i], 0);
        m_device.clear(render::ClearFlags::Target | render::ClearFlags::ZBuffer, 0xFFFFFFFFu, 1.0f, 0);
        const float constants[8] = {lamps[i][0], lamps[i][1], lamps[i][2], 1.0f / kLampShadowRange,
                                    0.0f,        -1.0f,       0.0f,        0.0f};
        r.setVertexConstants(240, constants, 2);
        scene.drawCarCasters(camera);
        m_lamps.push_back(lamps[i]);
    }
    r.shadowShaders = kept;
    r.endMaterial();
    if (!m_lamps.empty()) m_device.setRenderTarget(0, 0);
    return m_lamps.size();
}

render::LampShadowView LampShadows::view(render::Camera& camera) {
    std::vector<render::Camera*> cameras;
    for (size_t i = 0; i < m_lamps.size(); ++i) cameras.push_back(&m_cameras[i]);
    return lampShadowView(camera, m_lamps, cameras);
}

}  // namespace oj::scene
