// The particles of a race (the effects manager [0x0075F0C8], docs/spec/20-effects.md): the emitters by definition,
// made, stepped and drawn.

#include "Game/Scene/Particles/ParticleSystem.h"
#include "Game/Scene/Particles/ParticlesInternal.h"

#include "Core/Physics/TrackCollision.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <algorithm>
#include <cmath>

namespace oj::scene {

using render::Cull;
using render::RenderState;

using namespace particles;

namespace {

// 0x00407910: an angle within -pi .. pi.
float wrap(float a) {
    const float pi = 3.1415927f;
    while (a > pi) a = a - 6.2831855f;
    while (a < -pi) a = a + 6.2831855f;
    return a;
}

}  // namespace

ParticleSystem::ParticleSystem(Engine& engine, const EffectLibrary& library) : m_engine(engine), m_library(library) {
    const render::VertexElement elements[] = {
        {0, 0, 2, 0, 0, 0},    // POSITION, FLOAT3
        {0, 12, 4, 0, 10, 0},  // COLOR, D3DCOLOR
        {0, 16, 1, 0, 5, 0},   // TEXCOORD0, FLOAT2
        {0xFF, 0, 17, 0, 0, 0},
    };
    m_declaration = engine.device().createDeclaration(elements);
}

ParticleSystem::~ParticleSystem() {
    m_framebuffer.clear();
    m_smoke.clear();
    m_sparks.clear();
    m_scenery.clear();
    m_tracks.clear();
    m_smears.clear();
    if (m_indices != 0) m_engine.device().destroyBuffer(m_indices);
}

render::Material* ParticleSystem::material(const std::string& name) {
    auto it = m_materials.find(name);
    if (it == m_materials.end()) {
        it = m_materials.emplace(name, m_engine.loadMaterial(std::filesystem::path("mesh") / "mats", name, {})).first;
    }
    return it->second.get();
}

void ParticleSystem::drawSmears(float x0, float y0, float x1, float y1, render::Camera& camera) {
    screen = {x0, y0, x1, y1};
    frameList(m_smears, camera);
}

bool ParticleSystem::groundBelow(float x, float z, float& y) const {
    return collision != nullptr && oj::groundBelow(*collision, x, z, y);
}

render::BufferId ParticleSystem::quadIndices(uint32_t quads) {
    // [0x0075F0E0]: one buffer for every emitter, of the largest count asked for (0x0050AFC0, 0x0051DC10: each quad
    // 0, 1, 2, 3, 2, 1).
    if (quads > m_indexQuads) {
        if (m_indices != 0) m_engine.device().destroyBuffer(m_indices);
        std::vector<uint16_t> indices;
        indices.reserve(static_cast<size_t>(quads) * 6);
        for (uint32_t q = 0; q < quads; ++q) {
            const auto v = static_cast<uint16_t>(q * 4);
            for (uint16_t k : {v, static_cast<uint16_t>(v + 1), static_cast<uint16_t>(v + 2),
                               static_cast<uint16_t>(v + 3), static_cast<uint16_t>(v + 2),
                               static_cast<uint16_t>(v + 1)}) {
                indices.push_back(k);
            }
        }
        m_indices = m_engine.device().createIndexBuffer(indices.data(), static_cast<uint32_t>(indices.size()));
        m_indexQuads = quads;
    }
    return m_indices;
}

std::shared_ptr<Emitter> ParticleSystem::make(const std::string& name, CrtRandom* random) {
    const EffectDef* d = m_library.find(name);
    if (d == nullptr) return nullptr;
    std::shared_ptr<Emitter> e;
    switch (d->type) {
        case EffectType::Smoke:
        case EffectType::Wheelspin:
            e = std::make_shared<SmokeEmitter>(*this, *d);
            m_smoke.push_back(e);
            break;
        case EffectType::Framebuffer:
            e = std::make_shared<SmokeEmitter>(*this, *d);
            m_framebuffer.push_back(e);
            break;
        case EffectType::Sparks:
            e = std::make_shared<SparksEmitter>(*this, *d);
            m_sparks.push_back(e);
            break;
        case EffectType::Tracks:
            e = std::make_shared<TracksEmitter>(*this, *d);
            m_tracks.push_back(e);
            break;
        case EffectType::GroundRush:
            e = std::make_shared<GroundRushEmitter>(*this, *d);
            m_scenery.push_back(e);
            break;
        case EffectType::Rain:
            e = std::make_shared<RainEmitter>(*this, *d);
            m_scenery.push_back(e);
            break;
        case EffectType::Leaves:
            e = std::make_shared<LeavesEmitter>(*this, *d);
            m_scenery.push_back(e);
            break;
        case EffectType::Smear:
            e = std::make_shared<SmearEmitter>(*this, *d, random != nullptr ? *random : gameRandom());
            m_smears.push_back(e);
            break;
        case EffectType::Splash:
            e = std::make_shared<SplashEmitter>(*this, *d, random != nullptr ? *random : gameRandom());
            m_scenery.push_back(e);
            break;
        default:
            return nullptr;
    }
    return e;
}

void ParticleSystem::setColours(const Atmosphere& a) {
    // 0x005339A0: the sun's height (the first light's direction, reversed), the light's colour by it.
    const float k = (-a.lightDirection[0][2] + -a.lightDirection[0][0]) * 0.0f + -a.lightDirection[0][1];
    Vec4 first{}, second{};
    if (a.night != 0) {
        first = {a.ambient[0] * 0.5f, a.ambient[1] * 0.5f, a.ambient[2] * 0.5f, a.ambient[3] * 0.5f};
        second = {a.ambient[0], a.ambient[1], a.ambient[2], a.ambient[3]};
    } else {
        first = {a.ambient[0], a.ambient[1], a.ambient[2], a.ambient[3]};
        second = {a.lightColour[0][0] * k, a.lightColour[0][1] * k, a.lightColour[0][2] * k, a.lightColour[0][3]};
    }
    // 0x00503480
    sunColour = {(second[0] + first[0]) * 0.6f, (second[1] + first[1]) * 0.6f, (second[2] + first[2]) * 0.6f, 0.0f};
    shadeColour = {first[0] * 0.6f, first[1] * 0.6f, first[2] * 0.6f, 0.0f};
}

void ParticleSystem::frame(render::Camera& camera, float clock) {
    // 0x00503D10: the time of the race's clock, its step at most 0.04 and not below 0.
    dt = clock - time;
    if (dt > 0.04f) dt = 0.04f;
    time = clock;
    if (dt < 0.0f) dt = 0.0f;
    quality = m_engine.renderer().drawQuality;
    // The quads' axes: the camera's turned about its view by 0.3 x the time.
    const float a = wrap(time * 0.3f);
    const float c = std::cos(a), s = std::sin(a);
    const Mat34 turn{c, -s, 0.0f, 0.0f, s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    const Mat34 b = multiply(camera.world(), turn);
    const Mat34& cw = camera.world();
    for (size_t k = 0; k < 3; ++k) cameraAxes[k] = {cw[k], cw[4 + k], cw[8 + k], 0.0f};
    for (size_t k = 0; k < 3; ++k) billboard[k] = {b[k], b[4 + k], b[8 + k], 0.0f};
    m_wind.step(dt);
    // 0x00504300: the smoke's list; the last emitter that is done leaves it.
    render::IRenderDevice& device = m_engine.device();
    render::Renderer3d& r = m_engine.renderer();
    device.setRenderState(RenderState::CullMode, Cull::None);
    if (r.lastDeclaration != m_declaration) {
        device.setDeclaration(m_declaration);
        r.lastDeclaration = m_declaration;
    }
    // The groups (+0x68, 0x00503D10): TRACKS (+0x48), then LEAVES, GROUNDRUSH, RAIN and SPLASH (+0x38), then SPARKS
    // (+0x28); then the smoke's list (0x00504300).
    frameList(m_tracks, camera);
    frameList(m_scenery, camera);
    frameList(m_sparks, camera);
    frameList(m_smoke, camera);
    device.setRenderState(RenderState::CullMode, r.defaultCull);
}

void ParticleSystem::frameList(List& list, render::Camera& camera) {
    size_t done = list.size();
    for (size_t i = 0; i < list.size(); ++i) {
        if (!list[i]->frame(camera)) done = i;
    }
    if (done < list.size()) list.erase(list.begin() + static_cast<std::ptrdiff_t>(done));
}

void ParticleSystem::sortLists() {
    const auto higher = [](const std::shared_ptr<Emitter>& x, const std::shared_ptr<Emitter>& y) {
        return static_cast<int32_t>(x->def.type) > static_cast<int32_t>(y->def.type);
    };
    for (List* list : {&m_smoke, &m_sparks, &m_scenery, &m_tracks, &m_framebuffer, &m_smears}) {
        std::stable_sort(list->begin(), list->end(), higher);
    }
}

void ParticleSystem::drawFramebuffer(render::Camera& camera) {
    render::IRenderDevice& device = m_engine.device();
    render::Renderer3d& r = m_engine.renderer();
    device.setRenderState(RenderState::CullMode, Cull::None);
    if (r.lastDeclaration != m_declaration) {
        device.setDeclaration(m_declaration);
        r.lastDeclaration = m_declaration;
    }
    frameList(m_framebuffer, camera);
    device.setRenderState(RenderState::CullMode, r.defaultCull);
}

}  // namespace oj::scene
