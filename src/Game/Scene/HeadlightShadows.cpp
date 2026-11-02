#include "Game/Scene/HeadlightShadows.h"

#include "Core/Base/Log.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"
#include "Game/Scene/TrackScene.h"
#include "Render/Renderer3d.h"

#include <cmath>

namespace oj::scene {

namespace {

constexpr uint32_t kCullNone = 1;  // D3DCULL_NONE: both sides (a fence's single side casts; the receivers look up
                                   // off their surface, the night shaders' normal offset)

}  // namespace

ShadowSize shadowSize(ShadowTier tier) {
    switch (tier) {
    case ShadowTier::Own: return {1536, 768};
    case ShadowTier::Near: return {768, 384};
    case ShadowTier::Far: return {384, 192};
    }
    return {};
}

ShadowTier shadowTier(bool own, float distance) {
    if (own) return ShadowTier::Own;
    return distance <= kNearShadows ? ShadowTier::Near : ShadowTier::Far;
}

bool shadowDue(ShadowTier tier, uint32_t car, uint64_t frame, bool drawn) {
    return tier != ShadowTier::Far || !drawn || (frame + car) % kFarShadowTurns == 0;
}

HeadlightShadows::HeadlightShadows(render::IRenderDevice& device) : m_device(device), m_shaders(device) {
    m_camera.setFov(static_cast<float>(std::atan(kHeadlightTanWidth) * 2.0),
                    static_cast<float>(std::atan(kHeadlightTanHeight) * 2.0), false);
    m_camera.setFar(kHeadlightRange);
}

HeadlightShadows::~HeadlightShadows() {
    unbind();
    for (const auto& [key, map] : m_maps) {
        if (map.id != 0) m_device.destroyTexture(map.id);
    }
}

void HeadlightShadows::unbind() {
    const std::array<render::TextureId, kHeadlights> none{};
    m_device.setShadowMaps(none.data(), kHeadlights);
}

std::array<float, kHeadlights> HeadlightShadows::draw(TrackScene& scene, const std::vector<Light>& lights,
                                                       uint64_t frame) {
    std::array<float, kHeadlights> texels{};
    std::array<render::TextureId, kHeadlights> bound{};
    if (!usable()) return texels;
    // No map read while one is drawn into.
    unbind();
    render::Renderer3d& r = scene.engine().renderer();
    r.endMaterial();
    scene.engine().postEffects().forgetTarget();
    const render::Renderer3d::ShadowShaders kept = r.shadowShaders;
    r.shadowShaders = m_shaders.shaders(kCullNone, false);
    bool drew = false;
    for (size_t i = 0; i < lights.size() && i < static_cast<size_t>(kHeadlights); ++i) {
        const Light& light = lights[i];
        const ShadowSize size = shadowSize(light.tier);
        Map& map = m_maps[{light.car, light.tier}];
        if (map.id == 0) {
            map.id = m_device.createFloatTarget(size.width, size.height);
            log::info("headlights: car {}'s shadow map {} x {} (light {})", light.car, size.width, size.height, i);
        }
        if (map.id == 0) continue;
        if (shadowDue(light.tier, light.car, frame, map.drawn)) {
            const HeadlightFrame f = headlightFrame(light.headlights);
            m_camera.local = {f.side[0], f.up[0], f.beam[0], f.place[0], f.side[1], f.up[1], f.beam[1], f.place[1],
                              f.side[2], f.up[2], f.beam[2], f.place[2]};
            m_camera.invalidate();
            m_device.setRenderTarget(map.id, 0);
            m_device.clear(render::ClearFlags::Target | render::ClearFlags::ZBuffer, 0xFFFFFFFFu, 1.0f, 0);
            const float constants[8] = {f.place[0], f.place[1], f.place[2], 1.0f / kHeadlightRange,
                                        f.beam[0],  f.beam[1],  f.beam[2],  0.0f};
            r.setVertexConstants(240, constants, 2);
            scene.drawShadowCasters(m_camera, light.model, TrackScene::kHeadlightMarks);
            map.drawn = true;
            drew = true;
        }
        texels[i] = 1.0f / static_cast<float>(size.width);
        bound[i] = map.id;
    }
    r.shadowShaders = kept;
    r.endMaterial();
    if (drew) m_device.setRenderTarget(0, 0);
    m_device.setShadowMaps(bound.data(), kHeadlights);
    m_bound = bound;
    return texels;
}

}  // namespace oj::scene
