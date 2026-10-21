// SMEAR: the screen's smears.

#include "Game/Scene/Particles/SmearEmitter.h"
#include "Game/Scene/Particles/ParticlesInternal.h"

#include "Core/Physics/TrackCollision.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <cmath>

namespace oj::scene {

using render::Cull;
using render::Primitive;
using render::RenderState;

using namespace particles;

namespace {

constexpr float kByte = 0.003921569f;                          // [0x006FF918]: 1 / 255

}  // namespace

SmearEmitter::SmearEmitter(ParticleSystem& system, const EffectDef& d, CrtRandom& random) : Emitter(system, d) {
    m_points = d.points;
    m_size = d.startSize;
    m_spread = d.emitSpeed;
    m_growth = d.growthRate;
    m_gravity = d.gravity;
    m_drift = d.randomDrift;
    m_material = makeTexShape2d(system.engine());
    m_texture = system.engine().texture("rainsmear");
    m_blobs.resize(m_points);
    m_strip = m_points * 6 - 2;
    if (m_points != 0) {
        m_vertices.resize(static_cast<size_t>(m_points) * 6);
        m_buffer = system.engine().device().createVertexBuffer(
            m_vertices.data(), static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
    }
    clear(random);
}

SmearEmitter::~SmearEmitter() {
    if (m_buffer != 0) m_system.engine().device().destroyBuffer(m_buffer);
}

void SmearEmitter::clear(CrtRandom& random) {
    // Stock problem: the last place's w is a value of the stack; 0 here.
    m_last = {0.0f, 0.0f, 0.0f, 0.0f};
    for (Blob& b : m_blobs) {
        b = Blob{};
        b.life = static_cast<float>(random.next()) * kRandomScale;
        b.colour = 0xFFFFFFFFu;
    }
}

void SmearEmitter::simulate(render::Camera& camera) {
    float dt = m_system.dt;
    if (dt > 0.05f) {
        dt = 0.05f;
    } else if (dt == 0.0f) {
        return;
    }
    const Mat34& w = camera.world();
    m_place = {w[3], w[7], w[11], m_place[3]};
    m_way = {m_place[0] - m_last[0], m_place[1] - m_last[1], m_place[2] - m_last[2], m_place[3] - m_last[3]};
    m_velocity = m_way;
    const float length = std::sqrt((m_way[0] * m_way[0] + m_way[1] * m_way[1]) + m_way[2] * m_way[2]) + 0.1f;
    auto unit = [](Vec4& v) {
        const float k = 1.0f / (std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]) + kEpsilon);
        v = {k * v[0], k * v[1], k * v[2], v[3]};
    };
    unit(m_way);
    // The move along the camera's axes; the change of its parts across and up over the frame.
    const Vec4& v = m_velocity;
    auto along = [&](const Vec4& a) { return (a[2] * v[2] + a[1] * v[1]) + a[0] * v[0]; };
    Vec4 parts{along(m_system.cameraAxes[0]), along(m_system.cameraAxes[1]), along(m_system.cameraAxes[2]), 0.0f};
    const float inverse = 1.0f / dt;
    const float accX = (m_across - parts[0]) * inverse;
    const float accY = (m_up - parts[1]) * inverse;
    m_across = parts[0];
    m_up = parts[1];
    // A jump of the camera moves nothing.
    if (length > 1000.0f) {
        m_last = m_place;
        return;
    }
    const float x0 = m_system.screen[0], y0 = m_system.screen[1];
    const float x1 = m_system.screen[2], y1 = m_system.screen[3];
    const float cx = (x1 + x0) * 0.5f;
    const float cy = (y0 + y1) * 0.75f;
    const float width = x1 - x0;
    const float height = y1 - y0;
    const float marginX = width * 0.1f;
    const float marginY = height * 0.1f;
    unit(parts);
    // Moving forward the blobs spread out from (cx, cy).
    const float spread = parts[2] > 0.1f ? m_spread : 0.0f;
    m_shown = m_system.quality * m_amount;
    const auto count = static_cast<uint32_t>(x87::chop(static_cast<float>(def.points) * m_shown));
    const float grow = (m_shown + 1.0f) * 0.5f;
    const float lengthSquared = length * length;
    CrtRandom& random = gameRandom();
    for (size_t i = 0; i < m_blobs.size(); ++i) {
        Blob& b = m_blobs[i];
        // Stock problem: the bottom of the view is taken at x1 + its margin, not y1.
        if (i < count && (!(b.life < 1.0f) || x0 - marginX > b.x || y0 - marginY > b.y || marginX + x1 < b.x ||
                          marginY + x1 < b.y)) {
            b.life = 0.0f;
            b.size = ((static_cast<float>(random.next()) * m_growth) * kRandomScale + m_size) * grow;
            b.first = 0;
            b.next = 1;
            b.count = 1;
            const float x = (static_cast<float>(random.next()) * width) * kRandomScale + x0;
            const float y = (static_cast<float>(random.next()) * height) * kRandomScale + y0;
            b.x = b.anchorX = x;
            b.y = b.anchorY = y;
            b.trail[0] = {x, y, x, y};
        }
        // Away from (cx, cy) by the speed squared, swaying, falling by GRAVITY, against the turn of the view.
        const float dx = b.x - cx;
        const float dy = b.y - cy;
        const auto angle = static_cast<float>(std::atan2(static_cast<double>(dx), static_cast<double>(dy)));
        const float radial = ((std::sqrt(dy * dy + dx * dx) + 10.0f) * lengthSquared) * spread;
        const float wobble = length * m_drift + 4.0f;
        const auto swayY = static_cast<float>(std::sin(static_cast<double>((0.003f * length + 0.1f) * b.y))) * wobble;
        const float vy = swayY + ((static_cast<float>(std::cos(static_cast<double>(angle))) * radial + m_gravity) -
                                  0.1f * accY);
        const auto swayX = static_cast<float>(std::cos(static_cast<double>((0.004f * length + 0.1f) * b.x))) * wobble;
        const float vx = swayX + (static_cast<float>(std::sin(static_cast<double>(angle))) * radial - 0.1f * accX);
        b.x = (vx * b.size) * dt + b.x;
        b.y = (vy * b.size) * dt + b.y;
        // Its alpha the life left x 0.4, its low byte only (Stock problem: past its life it wraps to opaque).
        const auto alpha = static_cast<uint32_t>(x87::chop(((1.0f - b.life) * 0.4f) * 255.0f)) & 0xFFu;
        b.colour = (b.colour & 0x00FFFFFFu) | (alpha << 24);
        b.life = (((m_shown * 0.2f + 0.8f) * (10.0f + length)) * 0.01f) * dt + b.life;
        // Over 10 from the trail's last point, a pair half the blob's size each side of it.
        const float x = b.x, y = b.y;
        const float tx = b.anchorX - x;
        const float ty = b.anchorY - y;
        const float far = ty * ty + tx * tx;
        if (far > 100.0f) {
            const float l = std::sqrt(far);
            const float half = 0.5f * b.size;
            const float k = 1.0f / l;
            const float a = (half * tx) * k;
            const float c = (half * ty) * k;
            b.trail[b.next] = {x - c, a + y, c + x, y - a};
            b.next += 1;
            b.anchorX = x;
            b.anchorY = y;
            if (b.next == 6) b.next = 0;
            if (b.next == b.first) {
                b.first += 1;
                if (b.first == 6) b.first = 0;
            }
            if (b.count < 6) b.count += 1;
        }
        b.trail[b.next] = {x, y, x, y};
    }
    m_last = m_place;
}

bool SmearEmitter::frame(render::Camera& camera) {
    simulate(camera);
    if (!m_material || m_buffer == 0) return true;
    render::IRenderDevice& device = m_system.engine().device();
    render::Renderer3d& r = m_system.engine().renderer();
    m_material->setTexture(0, m_texture);
    const uint32_t zero = 0;
    m_material->setParam(0, &zero);
    m_material->setParam(1, &zero);
    r.beginMaterialPass(*m_material, 0);
    if (r.lastDeclaration != m_system.declaration()) {
        device.setDeclaration(m_system.declaration());
        r.lastDeclaration = m_system.declaration();
    }
    device.setRenderState(RenderState::CullMode, Cull::None);
    // Each blob's trail: its pairs from the oldest, the alpha growing from 0 to the blob's.
    for (const Blob& b : m_blobs) {
        const float step = (static_cast<float>(b.colour >> 24) * kByte) / static_cast<float>(b.count);
        float a = 0.0f;
        uint32_t k = b.first;
        for (uint32_t j = 0; j < b.count; ++j) {
            const auto alpha = static_cast<uint32_t>(x87::chop(a * 255.0f)) & 0xFFu;
            const uint32_t c = (b.colour & 0x00FFFFFFu) | (alpha << 24);
            a = step + a;
            const std::array<float, 4>& p = b.trail[k];
            m_vertices[j * 2] = {p[0], p[1], 10.0f, c, 0.5f, 0.5f};
            m_vertices[j * 2 + 1] = {p[2], p[3], 10.0f, c, 0.6f, 0.6f};
            k = k + 1 == 6 ? 0 : k + 1;
        }
        if (b.count > 2) {
            device.updateVertexBuffer(m_buffer, m_vertices.data(), b.count * 2 * static_cast<uint32_t>(sizeof(Vertex)));
            device.setStream(m_buffer, sizeof(Vertex));
            device.draw(Primitive::TriangleStrip, 0, b.count * 2 - 2);
        }
    }
    // The blobs, one triangle strip through all of them.
    for (size_t i = 0; i < m_blobs.size(); ++i) {
        const Blob& b = m_blobs[i];
        const float s = b.size;
        Vertex* v = &m_vertices[i * 6];
        v[0] = {b.x - s, b.y - s, 5.0f, b.colour, 0.0f, 0.0f};
        v[1] = {b.x - s, b.y + s, 5.0f, b.colour, 0.0f, 1.0f};
        v[2] = {b.x + s, b.y - s, 5.0f, b.colour, 1.0f, 0.0f};
        v[3] = {b.x + s, b.y + s, 5.0f, b.colour, 1.0f, 1.0f};
        if (i + 1 < m_blobs.size()) {
            v[4].x = v[3].x;
            v[4].y = v[3].y;
            v[4].z = 5.0f;
        }
        if (i > 0) {
            v[-1].x = v[0].x;
            v[-1].y = v[0].y;
            v[-1].z = 5.0f;
        }
    }
    device.updateVertexBuffer(m_buffer, m_vertices.data(), m_strip * static_cast<uint32_t>(sizeof(Vertex)));
    device.setStream(m_buffer, sizeof(Vertex));
    device.draw(Primitive::TriangleStrip, 0, m_strip - 2);
    return true;
}

}  // namespace oj::scene
