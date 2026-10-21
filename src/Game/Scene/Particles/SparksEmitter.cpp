// SPARKS: streaks of a burst, drawn anew each frame.

#include "Game/Scene/Particles/SparksEmitter.h"
#include "Game/Scene/Particles/ParticlesInternal.h"

#include "Core/Physics/TrackCollision.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <cmath>

namespace oj::scene {

using render::Primitive;

using namespace particles;

void SparksEmitter::setFrequency(float hz) {
    soundFrequency(sound, hz);
}

SparksEmitter::SparksEmitter(ParticleSystem& system, const EffectDef& d) : Emitter(system, d) {
    m_points = d.points;
    m_material = system.material(d.material);
    m_start = d.colourStart;
    m_endColour = d.colourEnd;
    m_end = 500.0f * d.ageRange;
    if (m_points != 0) {
        const std::vector<uint8_t> zeros(static_cast<size_t>(m_points) * 4 * sizeof(Vertex));
        m_buffer = system.engine().device().createVertexBuffer(zeros.data(), static_cast<uint32_t>(zeros.size()));
        system.quadIndices(m_points);
    }
}

SparksEmitter::~SparksEmitter() {
    if (m_buffer != 0) m_system.engine().device().destroyBuffer(m_buffer);
}

void SparksEmitter::setLevel(float value, float mul) {
    Emitter::setLevel(value, mul);
    // 0x00407C00: the velocity over its length (plus a little).
    const float x = velocity[0], y = velocity[1], z = velocity[2];
    const float over = 1.0f / (std::sqrt((x * x + y * y) + z * z) + kEpsilon);
    const float k = -0.5f;
    m_spray = {k * (over * x) * level, k * (over * y) * level, k * (over * z) * level, velocity[3]};
    if (value > 0.0f) m_end = 500.0f * def.ageRange + m_time;
}

bool SparksEmitter::prepare(const Vec4& place, render::Camera& camera) {
    if (!m_burst) {
        if (!(level > def.triggerLevel)) return false;
        m_burst = true;
    } else {
        m_time = m_system.dt + m_time;
        if (m_time > m_end) {
            m_time = 0.0f;
            m_end = 500.0f * def.ageRange;
            m_burst = false;
            setLevel(0.0f);
            return false;
        }
    }
    return camera.sphereVisible(place, def.emitSpeed);
}

bool SparksEmitter::frame(render::Camera& camera) {
    const Vec4 p = world();
    if (!prepare(p, camera) || m_points == 0) return true;
    ParticleRandom& random = m_system.random();
    random.seed(0);
    const Vec4& c = m_system.cameraAxes[2];
    const float speed = def.emitSpeed;
    const float growth = def.growthRate;
    const float gravity = def.gravity;
    const float width = def.startSize;
    const float ages = def.ageRange;
    const Vec4& spread = def.emitDirection;
    auto nearest = [](float f) { return static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(f))) & 0xFFu; };
    m_vertices.clear();
    for (uint32_t i = 0; i < m_points; ++i) {
        const float sp = (random.next() + 1.0f) * speed;
        double whole = 0.0;
        const auto f = static_cast<float>(std::modf(static_cast<double>(m_time / (sp * ages)), &whole));
        const float g = 1.0f - f;
        const uint32_t argb = (nearest(m_start[3] * g + m_endColour[3] * f) << 24) |
                              (nearest(m_start[0] * g + m_endColour[0] * f) << 16) |
                              (nearest(m_start[1] * g + m_endColour[1] * f) << 8) |
                              nearest(m_start[2] * g + m_endColour[2] * f);
        const float jz = (random.next() - 0.5f) * spread[2] * 2.0f;
        const float jy = random.next() * spread[1];
        const float jx = (random.next() - 0.5f) * spread[0] * 2.0f;
        const float vx = jx * level + m_spray[0];
        const float vy = jy * level + m_spray[1];
        const float vz = jz * level + m_spray[2];
        const float s = (random.next() + 1.0f) * level * width;
        const float h = f + growth;
        const float drop = (random.next() + 0.25f) * gravity;
        // The streak from where it is at f to where it is at f plus GROWTH_RATE, falling by the drop x the time
        // squared.
        const float ax = (p[0] + vx * sp * f) + 0.0f * f * f;
        const float ay = (p[1] + vy * sp * f) + drop * f * f;
        const float az = (p[2] + vz * sp * f) + 0.0f * f * f;
        const float bx = (vx * sp * h + p[0]) + 0.0f * h * h;
        const float by = (vy * sp * h + p[1]) + drop * h * h;
        const float bz = (vz * sp * h + p[2]) + 0.0f * h * h;
        const float dx = bx - ax, dy = by - ay, dz = bz - az;
        const float over = 1.0f / (std::sqrt((dx * dx + dz * dz) + dy * dy) + kEpsilon);
        const float nx = dx * over, ny = dy * over, nz = dz * over;
        const float wx = (c[1] * nz - ny * c[2]) * s;
        const float wy = (c[2] * nx - nz * c[0]) * s;
        const float wz = (ny * c[0] - c[1] * nx) * s;
        m_vertices.push_back({(ax - dx) + wx, (ay - dy) + wy, (az - dz) + wz, argb, 0.0f, 0.0f});
        m_vertices.push_back({(dx + wx) + ax, (wy + dy) + ay, (wz + dz) + az, argb, 1.0f, 0.0f});
        m_vertices.push_back({(bx - dx) - wx, (by - dy) - wy, (bz - dz) - wz, argb, 0.0f, 1.0f});
        m_vertices.push_back({(dx + bx) - wx, (dy + by) - wy, (dz + bz) - wz, argb, 1.0f, 1.0f});
    }
    if (m_material == nullptr) return true;
    render::IRenderDevice& device = m_system.engine().device();
    render::Renderer3d& r = m_system.engine().renderer();
    r.beginMaterialPass(*m_material, 0);
    device.updateVertexBuffer(m_buffer, m_vertices.data(), static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
    device.setStream(m_buffer, sizeof(Vertex));
    device.setIndices(m_system.quadIndices(m_points));
    device.drawIndexed(Primitive::TriangleList, m_points * 4, 0, m_points * 2);
    return true;
}

}  // namespace oj::scene
