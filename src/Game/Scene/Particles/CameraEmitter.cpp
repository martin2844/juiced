// The emitters about the camera: the ground rush, the rain and its splashes.

#include "Game/Scene/Particles/CameraEmitter.h"
#include "Game/Scene/Particles/ParticlesInternal.h"

#include "Core/Physics/TrackCollision.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <cmath>

namespace oj::scene {

using render::Primitive;

using namespace particles;

CameraEmitter::CameraEmitter(ParticleSystem& system, const EffectDef& d) : Emitter(system, d) {
    m_points = d.points;
    m_colourStart = d.colourStart;
    m_colourVary = d.colourVary;
    m_drift = d.randomDrift;
    m_reach = d.randomDrift * d.randomDrift * 2.0f;
    m_size = d.startSize;
    m_growth = d.growthRate;
    m_speed = d.followFade;
    m_flipped = d.bounciness < 0.0f;
    m_width = std::fabs(d.bounciness);
    m_material = system.material(d.material);
    m_textures = d.textures;
    m_spots.resize(m_points);
}

void CameraEmitter::attach(render::Camera* camera, int32_t value, bool night) {
    m_value = value;
    m_camera = camera;
    m_night = night;
}

void CameraEmitter::setAmount(float value) {
    if (value < 0.0f) {
        m_amount = 0.0f;
    } else if (value > 1.0f) {
        m_amount = 1.0f;
    } else {
        m_amount = value;
    }
}

bool CameraEmitter::frame(render::Camera&) {
    return true;
}

bool CameraEmitter::prepare(render::Camera& camera) {
    if (m_camera != &camera) return false;
    const Mat34& w = camera.world();
    m_place = {w[3], w[7], w[11], m_place[3]};
    m_shown = m_system.quality * m_amount;
    m_move = {m_place[0] - m_last[0], m_place[1] - m_last[1], m_place[2] - m_last[2], m_place[3] - m_last[3]};
    m_jumped = m_move[0] * m_move[0] + m_move[2] * m_move[2] > 1000000.0f;
    return true;
}

Vec4 CameraEmitter::placeOnGround(size_t index) {
    // Within RANDOM_DRIFT x 0.75 of the camera across, a tenth of its move ahead; on the road there, else halfway
    // back toward the camera, else at the world's origin (Stock problem: w then a value of the stack; 0 here).
    CrtRandom& random = gameRandom();
    const float r1 = static_cast<float>(random.next()) * kRandomScale;
    const float r2 = static_cast<float>(random.next()) * kRandomScale - 0.5f;
    const float across = (r2 * 1.5f) * m_drift;
    const float up = 0.0f * m_drift;
    const float along = ((r1 - 0.5f) * 1.5f) * m_drift;
    Vec4 p{m_move[0] * 0.1f + (across + m_place[0]), m_move[1] * 0.1f + (up + m_place[1]),
           m_move[2] * 0.1f + (m_place[2] + along), m_place[3]};
    float y = 0.0f;
    if (m_system.groundBelow(p[0], p[2], y)) {
        p[1] = y;
    } else {
        p = {p[0] - across * 0.5f, p[1] - up * 0.5f, p[2] - along * 0.5f, p[3]};
        if (m_system.groundBelow(p[0], p[2], y)) {
            p[1] = y;
        } else {
            p = {0.0f, 0.0f, 0.0f, 0.0f};
        }
    }
    p[1] = p[1] - 2.0f;
    if (index < m_spots.size()) m_spots[index] = p;
    return p;
}

uint32_t CameraEmitter::sunColour(uint32_t alpha) const {
    const auto channel = [](float v) { return static_cast<uint32_t>(x87::chop(v)) & 0xFFu; };
    const Vec4& c = m_system.sunColour;
    return (alpha << 24) | (channel(c[0]) << 16) | (channel(c[1]) << 8) | channel(c[2]);
}

RainEmitter::RainEmitter(ParticleSystem& system, const EffectDef& d) : CameraEmitter(system, d) {
    if (m_points != 0) {
        m_vertices.resize(static_cast<size_t>(m_points) * 4);
        m_buffer = system.engine().device().createVertexBuffer(
            m_vertices.data(), static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
        system.quadIndices(m_points);
    }
    m_colour = sunColour(0x38);
}

RainEmitter::~RainEmitter() {
    if (m_buffer != 0) m_system.engine().device().destroyBuffer(m_buffer);
}

bool RainEmitter::frame(render::Camera& camera) {
    if (!prepare(camera) || m_shown == 0.0f) return true;
    const Vec4& right = m_system.cameraAxes[0];
    const Vec4& ahead = m_system.cameraAxes[2];
    const auto count = static_cast<uint32_t>(x87::chop(static_cast<float>(m_points) * m_shown));
    if (count == 0 || m_buffer == 0) return true;
    CrtRandom& random = gameRandom();
    const float d = m_drift;
    for (uint32_t i = 0; i < count && i < m_points; ++i) {
        // A streak at a point drawn within d x 0.75 (up d x 0.5625) of the place d ahead of the camera; START_SIZE
        // up and the camera's move long, BOUNCINESS wide along the camera's x.
        const float r1 = static_cast<float>(random.next()) * kRandomScale;
        const float a = static_cast<float>(random.next()) * kRandomScale - 0.5f;
        const float b = (r1 - 0.5f) * 0.75f;
        const float c = static_cast<float>(random.next()) * kRandomScale - 0.5f;
        const float px = (a * d) * 1.5f + m_place[0];
        const float py = (b * d) * 1.5f + m_place[1];
        const float pz = (c * d) * 1.5f + m_place[2];
        const float tx = ahead[0] * d + px;
        const float ty = ahead[1] * d + py;
        const float tz = ahead[2] * d + pz;
        const float wx = right[0] * m_width, wy = right[1] * m_width, wz = right[2] * m_width;
        const float rise = m_size + m_move[1];
        Vertex* v = &m_vertices[static_cast<size_t>(i) * 4];
        v[0] = {tx, ty, tz, m_colour, 0.0f, 1.0f};
        v[1] = {m_move[0] + tx, rise + ty, m_move[2] + tz, m_colour, 0.0f, 0.0f};
        v[2] = {tx + wx, wy + ty, wz + tz, m_colour, 1.0f, 1.0f};
        v[3] = {(m_move[0] + wx) + tx, (rise + wy) + ty, (m_move[2] + wz) + tz, m_colour, 1.0f, 0.0f};
    }
    render::IRenderDevice& device = m_system.engine().device();
    device.updateVertexBuffer(m_buffer, m_vertices.data(), count * 4 * static_cast<uint32_t>(sizeof(Vertex)));
    if (m_material != nullptr) m_system.engine().renderer().beginMaterialPass(*m_material, 0);
    device.setStream(m_buffer, sizeof(Vertex));
    device.setIndices(m_system.quadIndices(m_points));
    // Stock problem: two triangles fewer than the quads made: the last streak is not drawn. The fixed build draws two
    // triangles a streak.
    device.drawIndexed(Primitive::TriangleList, count * 4, 0, count * 2);
    m_last = m_place;
    return true;
}

SplashEmitter::SplashEmitter(ParticleSystem& system, const EffectDef& d, CrtRandom& random)
    : CameraEmitter(system, d) {
    m_strip = m_points * 6 - 2;
    if (m_points != 0) {
        m_vertices.resize(static_cast<size_t>(m_points) * 6);
        m_buffer = system.engine().device().createVertexBuffer(
            m_vertices.data(), static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
    }
    m_colour = sunColour(0x80);
    // The frames of NUM_TEXTURES in rows of four; each splash starts at a frame drawn by rand().
    const auto n = static_cast<uint32_t>(m_textures);
    m_across = 1.0f / static_cast<float>(((n - 1) & 3) + 1);
    m_down = 1.0f / static_cast<float>(((n - 1) >> 2) + 1);
    m_frames.resize(m_points);
    for (float& f : m_frames) f = static_cast<float>(static_cast<uint32_t>(random.next()) % n);
}

SplashEmitter::~SplashEmitter() {
    if (m_buffer != 0) m_system.engine().device().destroyBuffer(m_buffer);
}

bool SplashEmitter::frame(render::Camera& camera) {
    if (!prepare(camera) || m_shown == 0.0f) return true;
    const Vec4& right = m_system.cameraAxes[0];
    const auto count = static_cast<uint32_t>(x87::chop(static_cast<float>(m_points) * m_shown));
    if (count == 0 || m_buffer == 0) return true;
    m_strip = count * 6 - 2;
    for (uint32_t i = 0; i < count && i < m_points; ++i) {
        Vec4 p = m_spots[i];
        // Stock problem: the distance (z² + x² / 2 from the camera) against twice RANDOM_DRIFT² decides nothing:
        // a splash moves when the camera jumped or its frames start again. The fixed build also puts a splash left
        // farther than that (behind a moving camera) on the road again at once.
        const float dx = m_place[0] - p[0];
        const float dz = m_place[2] - p[2];
        const float far = dz * dz + dx * dx * 0.5f;
        auto frameNow = static_cast<uint32_t>(x87::chop(m_frames[i]));
        if (frameNow >= static_cast<uint32_t>(m_textures)) {
            m_frames[i] = 0.0f;
            frameNow = 0;
        }
        m_frames[i] = m_speed + m_frames[i];
        if (m_jumped || frameNow == 0 || far > m_reach) p = placeOnGround(i);
        const float wx = right[0] * m_width, wy = right[1] * m_width, wz = right[2] * m_width;
        const float u0 = static_cast<float>(frameNow & 3) * m_across;
        const float v0 = static_cast<float>(frameNow >> 2) * m_down;
        const float u1 = u0 + m_across;
        const float v1 = v0 + m_down;
        const float h = m_size;
        Vertex* v = &m_vertices[static_cast<size_t>(i) * 6];
        v[0] = {p[0], p[1], p[2], m_colour, u0, v1};
        v[1] = {p[0], h + p[1], p[2], m_colour, u0, v0};
        v[2] = {wx + p[0], wy + p[1], wz + p[2], m_colour, u1, v1};
        v[3] = {wx + p[0], (h + wy) + p[1], wz + p[2], m_colour, u1, v0};
        // The strip goes on to the next splash through the last vertex twice and the next's first.
        if (i < m_points - 1) {
            v[4].x = v[3].x;
            v[4].y = v[3].y;
            v[4].z = v[3].z;
        }
        if (i > 0) {
            v[-1].x = p[0];
            v[-1].y = p[1];
            v[-1].z = p[2];
        }
    }
    render::IRenderDevice& device = m_system.engine().device();
    device.updateVertexBuffer(m_buffer, m_vertices.data(), m_strip * static_cast<uint32_t>(sizeof(Vertex)));
    m_last = m_place;
    if (m_material != nullptr) m_system.engine().renderer().beginMaterialPass(*m_material, 0);
    device.setStream(m_buffer, sizeof(Vertex));
    device.draw(Primitive::TriangleStrip, 0, m_strip - 2);
    return true;
}

}  // namespace oj::scene
