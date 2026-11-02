// SMOKE, FRAMEBUFFER and WHEELSPIN: billboards that rise, cool and drift.

#include "Game/Scene/Particles/SmokeEmitter.h"
#include "Game/Scene/Particles/ParticlesInternal.h"

#include "Core/Physics/TrackCollision.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace oj::scene {

using render::Primitive;

using namespace particles;

namespace {

// The quad's corners of each of the four turns of the texture (0x00719298: u, v of corners 0 .. 3).
constexpr float kCorners[4][8] = {{0, 1, 1, 1, 0, 0, 1, 0},
                                  {0, 0, 0, 1, 1, 0, 1, 1},
                                  {1, 0, 0, 0, 1, 1, 0, 1},
                                  {1, 1, 1, 0, 0, 1, 0, 0}};

}  // namespace

SmokeEmitter::SmokeEmitter(ParticleSystem& system, const EffectDef& d) : ParticleEmitter(system, d) {
    if (m_points != 0) {
        const std::vector<uint8_t> zeros(static_cast<size_t>(m_points) * 4 * sizeof(Vertex));
        m_buffer = system.engine().device().createVertexBuffer(zeros.data(), static_cast<uint32_t>(zeros.size()));
        system.quadIndices(m_points);
    }
    for (Vec4* c : {&m_sunColour, &m_shadeColour}) {
        Vec4& v = *c;
        // Stock problem: the alpha is scaled twice (x 0.64). The fixed build scales it once, as the colour.
        v = {v[0] * 0.8f, v[1] * 0.8f, v[2] * 0.8f, v[3] * 0.8f};
    }
}

SmokeEmitter::~SmokeEmitter() {
    if (m_buffer != 0) m_system.engine().device().destroyBuffer(m_buffer);
}

bool SmokeEmitter::step(size_t index, Vec4& place, float& size, Vec4& colour, float dt) {
    Particle& p = m_particles[index];
    ParticleRandom& random = m_system.random();
    const float r1 = random.next() - 0.5f;
    const float r2 = random.next();
    const float r3 = random.next() - 0.5f;
    size = 0.0f;
    if (!(p.age < 0.0f)) {
        p.age = dt + p.age;
        if (p.age > m_life) {
            p.age = -1.0f;
            if (m_alive > 0) --m_alive;
        } else {
            const Vec4 air = m_system.wind().at(p, velocity, m_system.time, nullptr, nullptr);
            const float resist = def.windResistance;
            const Vec4 pushed{air[0] * resist, -def.gravity + air[1] * resist, air[2] * resist, 0.0f};
            float follow = p.age / m_life * def.followFade;
            if (follow > 1.0f) follow = 1.0f;
            const float k = (1.0f - follow) * (noFollow ? 0.0f : def.followFrac);
            const float drift = ((1.0f - def.minAmount) * p.level + def.minAmount) * def.randomDrift;
            const float driftX = r3 * drift, driftY = r2 * drift, driftZ = r1 * drift;
            const float keep = 1.0f - k;
            float vx = pushed[0] * keep + velocity[0] * k;
            float vy = pushed[1] * keep + velocity[1] * k;
            float vz = pushed[2] * keep + velocity[2] * k;
            const float rise = def.initialTemp / (def.coolingRate * p.age + 1.0f);
            vx = vx + driftX;
            vy = rise + (driftY + vy);
            vz = vz + driftZ;
            p.place = {vx * dt + p.place[0], vy * dt + p.place[1], vz * dt + p.place[2], p.place[3]};
            if (!(p.phase < 0.0f)) {
                const float turn = dt * p.frequency * kTwoPi;
                const float reach = turn * p.swirl;
                const float s = std::sin(p.phase) * reach;
                const float c = std::cos(p.phase) * reach;
                const float ox = p.across[0] * s + p.along[0] * c;
                const float oy = p.across[1] * s + p.along[1] * c;
                const float oz = p.across[2] * s + p.along[2] * c;
                p.phase = turn + p.phase;
                p.frequency = p.frequency * 0.995f;
                p.place = {ox + p.place[0], oy + p.place[1], oz + p.place[2], p.place[3]};
            }
            float grown = p.level + 0.25f;
            if (grown > 1.0f) grown = 1.0f;
            size = def.growthRate * p.age * grown + def.startSize;
            const float fade = 1.0f - p.age / m_life;
            const Vec4& c = p.lit != 0 ? m_sunColour : m_shadeColour;
            float alpha = c[3] * fade;
            if (p.age < def.fadeinTime) alpha = p.age / def.fadeinTime * alpha;
            colour = {c[0], c[1], c[2], alpha};
        }
    }
    place = p.place;
    return !(p.age < 0.0f);
}

void SmokeEmitter::simulate(const render::Camera& camera, float dt) {
    (void)camera;
    Vec4 a1{}, a2{}, a3{};
    // A finished emitter keeps the quads it made last (the stock code then draws its other buffer).
    if (!prepare(a1, a2, &a3)) return;
    m_vertices.clear();
    auto chop = [](float f) { return static_cast<uint32_t>(static_cast<int32_t>(f)) & 0xFFu; };
    for (size_t i = 0; i < m_points; ++i) {
        Vec4 p{}, colour{};
        float s = 0.0f;
        step(i, p, s, colour, dt);
        const uint32_t argb = (chop(colour[3] * 1.5f) << 24) | (chop(colour[0]) << 16) | (chop(colour[1]) << 8) |
                              chop(colour[2]);
        if (s == 0.0f) continue;
        const float* uv = kCorners[i & 3];
        const float cx = p[0] - a3[0] * s, cy = p[1] - a3[1] * s, cz = p[2] - a3[2] * s;
        const float ax = a1[0] * s, ay = a1[1] * s, az = a1[2] * s;
        const float bx = a2[0] * s, by = a2[1] * s, bz = a2[2] * s;
        m_vertices.push_back({(cx - ax) + bx, (cy - ay) + by, (cz - az) + bz, argb, uv[4], uv[5]});
        m_vertices.push_back({(bx + ax) + cx, (by + ay) + cy, (bz + az) + cz, argb, uv[6], uv[7]});
        m_vertices.push_back({(cx - ax) - bx, (cy - ay) - by, (cz - az) - bz, argb, uv[0], uv[1]});
        m_vertices.push_back({(ax + cx) - bx, (ay + cy) - by, (az + cz) - bz, argb, uv[2], uv[3]});
    }
}

bool SmokeEmitter::hazeFrame(render::Camera& camera, float viewWidth) {
    if (m_points == 0) return !dying;
    simulate(camera, m_system.dt);
    const size_t quads = m_vertices.size() / 4;
    if (quads == 0 || m_material == nullptr) return true;
    // The node's motion (JuicedConfigExt's Follow): the haze's strength by its car's speed, the throttle followed.
    float strength = 1.0f, speed = 0.0f, throttle = 0.0f;
    if (node != nullptr) {
        const Mat34& w = node->world();
        haze::follow(motion, {w[3], w[7], w[11]}, exhaust.valid ? exhaust.throttle : 0.0f, haze::clock());
        strength = motion.strength;
        speed = motion.speed;
        throttle = motion.throttle;
    }
    static_assert(sizeof(Vertex) == sizeof(haze::Vertex));
    std::vector<haze::Vertex> game(m_vertices.size());
    std::memcpy(game.data(), m_vertices.data(), m_vertices.size() * sizeof(Vertex));
    const haze::Axes axes{{m_system.billboard[0][0], m_system.billboard[0][1], m_system.billboard[0][2]},
                          {m_system.billboard[1][0], m_system.billboard[1][1], m_system.billboard[1][2]},
                          {m_system.billboard[2][0], m_system.billboard[2][1], m_system.billboard[2][2]}};
    float toClip[16];
    std::memcpy(toClip, camera.worldToClip().data(), sizeof(toClip));
    std::vector<haze::Vertex> drawn = haze::hazeQuads(game, axes, exhaust, strength, speed, throttle, toClip, viewWidth);
    if (drawn.empty()) drawn = std::move(game);  // more quads than the haze places: the game's
    m_system.engine().renderer().beginMaterialPass(*m_material, 0);
    m_system.engine().device().drawHeatHaze(drawn.data(), static_cast<uint32_t>(drawn.size() / 4),
                                            exhaust.valid ? haze::boost(throttle, speed) : 1.0f);
    return true;
}

bool SmokeEmitter::frame(render::Camera& camera) {
    if (m_points == 0) return !dying;
    simulate(camera, m_system.dt);
    const auto quads = static_cast<uint32_t>(m_vertices.size() / 4);
    if (quads == 0 || m_material == nullptr) return true;
    render::IRenderDevice& device = m_system.engine().device();
    render::Renderer3d& r = m_system.engine().renderer();
    device.updateVertexBuffer(m_buffer, m_vertices.data(), static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
    r.beginMaterialPass(*m_material, 0);
    device.setStream(m_buffer, sizeof(Vertex));
    device.setIndices(m_system.quadIndices(quads));
    device.drawIndexed(Primitive::TriangleList, quads * 4, 0, quads * 2);
    return true;
}

}  // namespace oj::scene
