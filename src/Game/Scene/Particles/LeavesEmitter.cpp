// LEAVES: leaves blown about the camera.

#include "Game/Scene/Particles/LeavesEmitter.h"
#include "Game/Scene/Particles/ParticlesInternal.h"

#include "Core/Physics/TrackCollision.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"

#include <cmath>

namespace oj::scene {

using render::Cull;
using render::Primitive;
using render::RenderState;

using namespace particles;

namespace {

// [0x0075F0DC]: the emitters' bounds drawn as circles (LEAVES, 0x00509890); nothing sets it.
bool g_drawBounds = false;

// 0x0050D1B0 → 0x005118F0: a circle round a point on the screen: the point and the point the radius along the camera's
// x axis put on the screen of 640 x 480 (the view's camera, +0x5C; the depth the third value), neither behind; the
// radius the second's distance across; round(radius x 0.1) + 8 points (at most 36) round it and the first again, a
// line strip of 2D vertices in one colour (the view's line material).
void drawCircle(render::Camera& camera, render::IRenderDevice& device, const Vec4& middle, float radius,
                uint32_t argb) {
    const Mat44& m = camera.worldToClip();
    auto screen = [&](float x, float y, float z, float out[3]) {
        const float sx = m[0] * x + m[1] * y + m[2] * z + m[3];
        const float sy = m[4] * x + m[5] * y + m[6] * z + m[7];
        const float w = m[12] * x + m[13] * y + m[14] * z + m[15];
        const float inv = w != 0.0f ? 1.0f / w : 0.0f;
        out[0] = (sx * inv * 0.5f + 0.5f) * 640.0f;
        out[1] = (0.5f - sy * inv * 0.5f) * 480.0f;
        out[2] = w;
    };
    const Mat34& c = camera.world();
    float centre[3];
    float edge[3];
    screen(middle[0], middle[1], middle[2], centre);
    screen(middle[0] + c[0] * radius, middle[1] + c[4] * radius, middle[2] + c[8] * radius, edge);
    if (centre[2] < 0.0f || edge[2] < 0.0f) return;
    const float r = edge[0] - centre[0];
    uint32_t count = static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(r * 0.1f)) + 8);
    if (count > 36) count = 36;
    float points[37][2] = {};
    for (uint32_t i = 0; i < count; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(count) * kTwoPi;
        points[i][0] = std::cos(a) * r + centre[0];
        points[i][1] = std::sin(a) * r + centre[1];
    }
    points[count][0] = points[0][0];
    points[count][1] = points[0][1];
    const float sx = static_cast<float>(device.width()) / 640.0f;
    const float sy = static_cast<float>(device.height()) / 480.0f;
    for (uint32_t i = 0; i < count; ++i) {
        device.drawLine(points[i][0] * sx, points[i][1] * sy, points[i + 1][0] * sx, points[i + 1][1] * sy, argb);
    }
}

const float kTurnScale = std::bit_cast<float>(0x3949116Du);  // [0x006FFDC4]: 2 pi / 32767
const float kHalfScale = std::bit_cast<float>(0x37800100u);  // [0x006FFB88]: 1 / 65534
// The leaves' colours in the sun ([0x007845C0]) and in the shade ([0x00790150]).
Vec4 g_leafLit{};
Vec4 g_leafShade{};

}  // namespace

void LeavesEmitter::Bounds::reset(const Vec4& point, float padding) {
    middle = point;
    top = {point[0], point[1], point[2], 0.0f};
    bottom = top;
    radius = padding;
    pad = padding;
}

void LeavesEmitter::Bounds::add(const Vec4& point) {
    for (size_t k = 0; k < 3; ++k) {
        if (point[k] > top[k]) top[k] = point[k];
        if (point[k] < bottom[k]) bottom[k] = point[k];
    }
    for (size_t k = 0; k < 3; ++k) middle[k] = (bottom[k] + top[k]) * 0.5f;
    const float dz = top[2] - bottom[2];
    const float dy = top[1] - bottom[1];
    const float dx = top[0] - bottom[0];
    radius = std::sqrt((dx * dx + dy * dy) + dz * dz) * 0.5f + pad;
}

LeavesEmitter::LeavesEmitter(ParticleSystem& system, const EffectDef& d) : Emitter(system, d) {
    // 0x0050DD60 (the shared tables made with the definition, makeEmitter)
    const float spread = system.density * 0.4f + 1.0f;
    m_points = d.points;
    m_colourStart = d.colourStart;
    m_colourEnd = d.colourEnd;
    m_colourVary = d.colourVary;
    m_brightness = d.brightnessRange;
    m_size = d.startSize;
    m_growth = d.growthRate;
    m_rotation = d.rotationRate;
    m_resistance = d.windResistance;
    m_drift = d.randomDrift;
    m_follow = d.followFrac;
    m_gravity = d.gravity;
    m_radius = spread * m_drift;
    m_reach = m_radius * m_radius;
    m_drift = m_drift * 0.05f;
    const Vec4& start = d.colourStart;
    const Vec4& sun = system.sunColour;
    const Vec4& shade = system.shadeColour;
    g_leafLit = {(sun[0] + start[0]) * 0.75f, (sun[1] + start[1]) * 0.75f, (sun[2] + start[2]) * 0.75f,
                 (sun[3] + start[3]) * 0.75f};
    g_leafShade = {(shade[0] + start[0]) * 0.4f, (shade[1] + start[1]) * 0.4f, (shade[2] + start[2]) * 0.4f,
                   (shade[3] + start[3]) * 0.4f};
    m_material = system.material(d.material);
    // The leaves' memory is not cleared (here zeros): an emitter of a scene object is not cleared before its frames.
    m_leaves.resize(m_points);
    // 0x00508B80: the records of NUM_POINTS x 4 vertices, the quads' indices (0x0050B0C0, the manager's shared
    // buffer), the vertex buffer (0x005089B0, its key 0x1000 + 20 x [0x00725714], a counter of the LEAVES emitters;
    // dynamic when the renderer's +0x684 is 0x10000).
    m_vertexCount = m_points * 4;
    for (Record& r : m_records) r.vertices.assign(m_vertexCount, Vertex{});
    if (m_points != 0) {
        system.quadIndices(m_points);
        const std::vector<uint8_t> zeros(static_cast<size_t>(m_vertexCount) * sizeof(Vertex));
        m_buffer = system.engine().device().createVertexBuffer(zeros.data(), static_cast<uint32_t>(zeros.size()));
    }
    // Each leaf's corners of its texture: entry % NUM_TEXTURES of an atlas four across, the quad's turn by the leaf's
    // index x 4 cut to a whole number. Stock problem: only the first leaf takes the turn 0, every other the last;
    // the turns 1 and 2 are never taken. The fixed build turns the leaves by their index % 4: each turn in turn.
    const auto n = static_cast<uint32_t>(d.textures);
    const float across = 1.0f / static_cast<float>(((n - 1) & 3) + 1);
    const float down = 1.0f / static_cast<float>(((n - 1) >> 2) + 1);
    for (Record& r : m_records) {
        for (uint32_t i = 0; i < m_points; ++i) {
            float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
            if (n > 1) {
                const uint32_t k = i % n;
                u0 = static_cast<float>(k & 3) * across;
                v0 = static_cast<float>(k >> 2) * down;
                u1 = u0 + across;
                v1 = v0 + down;
            }
            Vertex* q = &r.vertices[static_cast<size_t>(i) * 4];
            const auto set = [q](float a, float b, float c, float e, float f, float g, float h, float k) {
                q[0].u = a, q[0].v = b, q[1].u = c, q[1].v = e, q[2].u = f, q[2].v = g, q[3].u = h, q[3].v = k;
            };
            switch (i % 4) {
                case 0:
                    set(u0, v0, u1, v0, u0, v1, u1, v1);
                    break;
                case 1:
                    set(u1, v0, u1, v1, u0, v0, u0, v1);
                    break;
                case 2:
                    set(u1, v1, u0, v1, u1, v0, u0, v0);
                    break;
                default:
                    set(u0, v1, u0, v0, u1, v1, u1, v0);
                    break;
            }
        }
    }
}

LeavesEmitter::~LeavesEmitter() {
    // 0x00508E80: the job thread's queue is waited for twice before the records go.
    if (m_buffer != 0) m_system.engine().device().destroyBuffer(m_buffer);
}

bool LeavesEmitter::groundUnder(Leaf& leaf) const {
    return m_system.groundBelow(leaf.p.place[0], leaf.p.place[2], leaf.ground);
}

void LeavesEmitter::clear(CrtRandom& random) {
    for (Leaf& leaf : m_leaves) reset(leaf, random);
    m_bounds.reset(world(), m_radius);
}

void LeavesEmitter::reset(Leaf& leaf, CrtRandom& random) {
    // In the air over its place: across within a quarter of the radius either way, the radius up (without a node;
    // a leaf of an emitter with one is gone), a size and a drift drawn, its colour (only its alpha is drawn with:
    // COLOUR_START + COLOUR_VARY x a draw, the colour times a brightness of 1 - BRIGHTNESS_RANGE .. 1, each held
    // within 0 .. 255); on the road below, else halfway back, else gone.
    leaf.p.place = world();
    const float r1 = static_cast<float>(random.next()) * kRandomScale;
    const float r2 = static_cast<float>(random.next()) * kHalfScale - 0.25f;
    const float up = m_radius;
    const float along = r2 * m_radius;
    const float side = (r1 * 0.5f - 0.25f) * m_radius;
    if (node == nullptr) {
        leaf.p.age = 0.0f;
        leaf.p.place[0] = side + leaf.p.place[0];
        leaf.p.place[1] = up + leaf.p.place[1];
        leaf.p.place[2] = along + leaf.p.place[2];
    } else {
        leaf.p.age = -1.0f;
    }
    leaf.velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    leaf.size = static_cast<float>(random.next()) * m_growth * kRandomScale + m_size;
    const float a = static_cast<float>(random.next()) * kTurnScale;
    const float c = std::cos(a);
    const float s = std::sin(a);
    leaf.spin = {s * m_drift, 0.0f * m_drift, c * m_drift, leaf.spin[3]};
    const float r3 = static_cast<float>(random.next()) * kRandomScale;
    const float r4 = static_cast<float>(random.next()) * kRandomScale;
    const float bright = r4 * m_brightness + (1.0f - m_brightness);
    float alpha = r3 * m_colourVary[3] + m_colourStart[3];
    float red = (r3 * m_colourVary[0] + m_colourStart[0]) * bright;
    float green = (r3 * m_colourVary[1] + m_colourStart[1]) * bright;
    float blue = (r3 * m_colourVary[2] + m_colourStart[2]) * bright;
    for (float* v : {&alpha, &red, &green, &blue}) {
        if (*v < 0.0f) *v = 0.0f;
    }
    for (float* v : {&alpha, &red, &green, &blue}) {
        if (*v > 255.0f) *v = 255.0f;
    }
    leaf.colour = {red, green, blue, alpha};
    leaf.p.frequency = 0.0f;
    leaf.p.phase = -1.0f;
    leaf.last = leaf.p.place;
    if (!groundUnder(leaf)) {
        leaf.p.place[0] = leaf.p.place[0] - side * 0.5f;
        leaf.p.place[1] = leaf.p.place[1] - up * 0.5f;
        leaf.p.place[2] = leaf.p.place[2] - along * 0.5f;
        if (!groundUnder(leaf)) leaf.p.age = -1.0f;
    }
    leaf.ground = leaf.ground + 2.0f;
    leaf.light = m_system.sunMap != nullptr ? m_system.sunMap->sample(leaf.p.place) : 1.0f;
    leaf.p.place[1] = leaf.ground;
}

void LeavesEmitter::step(float dt, CrtRandom& random) {
    if (!(dt > 0.0f)) return;
    m_bounds.reset(world(), 200.0f);
    LeafTables& t = leafTables();
    for (uint32_t i = 0; i < m_points; ++i) {
        Leaf& leaf = m_leaves[i];
        if (!(leaf.p.age >= 0.0f)) continue;
        // The wind at it (0x00505CF0, no velocity of its own), its velocity kept by 1 - WIND_RESISTANCE; in the air
        // the wind's share, GRAVITY and a fall the slower the more its face lies across it ((0.7 - its light) x 20,
        // [0x00725710]).
        const Vec4 air = m_system.wind().at(leaf.p, Vec4{}, m_system.time, nullptr, nullptr);
        const float keep = 1.0f - m_resistance;
        Vec4& v = leaf.velocity;
        v = {v[0] * keep, v[1] * keep, v[2] * keep, v[3]};
        float lift = 0.0f;
        if (leaf.p.place[1] > leaf.ground) {
            v[0] = air[0] * m_resistance + v[0];
            v[1] = air[1] * m_resistance + v[1];
            v[2] = air[2] * m_resistance + v[2];
            const float fallen = v[1] - m_gravity;
            lift = t.light[i];
            v[1] = fallen - (0.7f - lift) * 20.0f;
        }
        const float sx = leaf.spin[0] * lift;
        const float sy = leaf.spin[1] * lift;
        const float sz = lift * leaf.spin[2];
        leaf.p.place[0] = (v[0] + sx) * dt + leaf.p.place[0];
        leaf.p.place[1] = (v[1] + sy) * dt + leaf.p.place[1];
        leaf.p.place[2] = (v[2] + sz) * dt + leaf.p.place[2];
        // Its turn about a car's wake, slowing by 0.99 a step, let go below 0.8 turns a second.
        if (leaf.p.phase >= 0.0f) {
            const float turn = dt * leaf.p.frequency * kTwoPi;
            const float reach = turn * leaf.p.swirl;
            const float s = std::sin(leaf.p.phase) * reach;
            const float c = std::cos(leaf.p.phase) * reach;
            const Vec4& across = leaf.p.across;
            const Vec4& along = leaf.p.along;
            const float ox = along[0] * c + across[0] * s;
            const float oy = along[1] * c + across[1] * s;
            const float oz = along[2] * c + across[2] * s;
            leaf.p.phase = turn + leaf.p.phase;
            leaf.p.frequency = leaf.p.frequency * 0.99f;
            if (leaf.p.frequency < 0.8f) leaf.p.phase = -1.0f;
            leaf.p.place[0] = ox + leaf.p.place[0];
            leaf.p.place[1] = oy + leaf.p.place[1];
            leaf.p.place[2] = oz + leaf.p.place[2];
        }
        // Out of the radius across the ground, or by a draw below EMIT_INTERVAL: gone with a node, else back in the
        // air over the place (half the radius up).
        const Vec4 home = world();
        const float dx = leaf.p.place[0] - home[0];
        const float dz = leaf.p.place[2] - home[2];
        auto drop = [&] {
            const float a1 = static_cast<float>(random.next()) * kRandomScale;
            const float a2 = static_cast<float>(random.next()) * kHalfScale;
            const float along = (a2 - 0.25f) * m_radius;
            const float side = (a1 * 0.5f - 0.25f) * m_radius;
            const float up = m_radius * 0.5f;
            leaf.p.frequency = 0.0f;
            leaf.p.phase = -1.0f;
            leaf.p.place = {side + home[0], up + home[1], home[2] + along, home[3]};
        };
        bool again = dx * dx + dz * dz > m_reach;
        if (!again) again = static_cast<float>(random.next()) * kRandomScale < def.emitInterval;
        if (again) {
            if (node != nullptr) {
                leaf.p.age = -1.0f;
            } else {
                drop();
            }
        }
        // Moved over 300 across the ground since its ground was found: the road under it again (else put back over
        // the place, else far away), its light from the sun map.
        const float lx = leaf.last[0] - leaf.p.place[0];
        const float lz = leaf.last[2] - leaf.p.place[2];
        if (lz * lz + lx * lx > 90000.0f) {
            if (!groundUnder(leaf)) {
                drop();
                if (!groundUnder(leaf)) leaf.p.place = {1000000.0f, 1000000.0f, 1000000.0f, 0.0f};
            }
            leaf.ground = leaf.ground + 2.0f;
            if (m_system.sunMap != nullptr) leaf.light = m_system.sunMap->sample(leaf.p.place);
            leaf.last = leaf.p.place;
        }
        // Below its ground: the road found again, the leaf on it and still.
        if (leaf.p.place[1] < leaf.ground) {
            if (!groundUnder(leaf)) leaf.p.place = {1000000.0f, 1000000.0f, 1000000.0f, 0.0f};
            leaf.ground = leaf.ground + 2.0f;
            leaf.p.place[1] = leaf.ground;
            leaf.velocity = {0.0f, 0.0f, 0.0f, 0.0f};
        }
        m_bounds.add(leaf.p.place);
    }
    m_bounds.radius = m_size + m_bounds.radius;
}

void LeavesEmitter::corners(const Leaf& leaf, uint32_t index, Vec4& a, Vec4& b, Vec4& colour) const {
    // A leaf gone has no size (the colour is left as it was). One lying on the road (at or below its ground, not
    // turning in a wake) lies flat along the table's axes, half lit; one in the air tumbles, two angles turning at
    // the time x 10 x its rate, its light the table's |cos B cos A| (kept for its fall) times the sun map's. The
    // colour goes from the shade's to the sun's by the light, its alpha the leaf's.
    if (!(leaf.p.age >= 0.0f)) {
        a = {0.0f, 0.0f, 0.0f, 0.0f};
        b = a;
        return;
    }
    LeafTables& t = leafTables();
    const float s = leaf.size;
    float light = 0.0f;
    if (!(leaf.p.place[1] > leaf.ground) && !(leaf.p.phase >= 0.0f)) {
        const Vec4& fa = t.flatA[index];
        const Vec4& fb = t.flatB[index];
        a = {fa[0] * s, fa[1] * s, fa[2] * s, fa[3]};
        b = {fb[0] * s, fb[1] * s, fb[2] * s, fb[3]};
        light = 0.5f * leaf.light;
    } else {
        const float time = m_system.time * 10.0f * t.rate[index];
        const float cA = std::cos(time + t.turnA[index]);
        const float sA = std::sin(time + t.turnA[index]);
        const float cB = std::cos(time + t.turnB[index]);
        const float sB = std::sin(time + t.turnB[index]);
        const float sBsA = sB * sA;
        t.light[index] = std::fabs(0.0f * sBsA - cB * cA);
        a = {cB * s, -sB * s, s * 0.0f, 0.0f};
        b = {sBsA * s, cB * sA * s, cA * s, 0.0f};
        light = leaf.light * t.light[index];
    }
    const float dark = 1.0f - light;
    colour[0] = g_leafShade[0] * dark + g_leafLit[0] * light;
    colour[1] = g_leafShade[1] * dark + g_leafLit[1] * light;
    colour[2] = g_leafShade[2] * dark + g_leafLit[2] * light;
    colour[3] = leaf.colour[3];
    for (size_t k = 0; k < 3; ++k) {
        if (colour[k] > 255.0f) colour[k] = 255.0f;
    }
}

void LeavesEmitter::simulate(float dt) {
    // The record not drawn: emptied, the leaves stepped (0x00508F70), each leaf's quad (its place +a, +b, -b, -a in
    // its colour, made bytes as the job thread's SSE conversion makes them: to the nearest), then it is the record
    // drawn.
    Record& r = m_records[(m_written - 1) & 1];
    r.count = 0;
    step(dt, m_system.jobRandom != nullptr ? *m_system.jobRandom : gameRandom());
    Vec4 colour{};
    auto byte = [](float f) { return static_cast<uint32_t>(x87::nearest(f)) & 0xFFu; };
    for (uint32_t i = 0; i < m_points; ++i) {
        const Leaf& leaf = m_leaves[i];
        const Vec4 p = leaf.p.place;
        Vec4 a{}, b{};
        corners(leaf, i, a, b, colour);
        const uint32_t argb = (byte(colour[3]) << 24) | (byte(colour[0]) << 16) | (byte(colour[1]) << 8) |
                              byte(colour[2]);
        Vertex* q = &r.vertices[static_cast<size_t>(i) * 4];
        q[0].x = a[0] + p[0], q[0].y = a[1] + p[1], q[0].z = a[2] + p[2];
        q[1].x = b[0] + p[0], q[1].y = b[1] + p[1], q[1].z = b[2] + p[2];
        q[2].x = p[0] - b[0], q[2].y = p[1] - b[1], q[2].z = p[2] - b[2];
        q[3].x = p[0] - a[0], q[3].y = p[1] - a[1], q[3].z = p[2] - a[2];
        for (int k = 0; k < 4; ++k) q[k].colour = argb;
    }
    r.count = m_points;
    m_written = (m_written - 1) & 1;
}

bool LeavesEmitter::frame(render::Camera& camera) {
    if (m_points == 0) return true;
    if (graphicsConfig().detail < 0.1f) return true;
    // Its bounds about its place with 500, drawn red under [0x0075F0DC] (0x005098DF, set by nothing).
    m_bounds.reset(world(), 500.0f);
    if (g_drawBounds) drawCircle(camera, m_system.engine().device(), m_bounds.middle, m_bounds.radius, 0xFFFF0000u);
    const Mat34& w = camera.world();
    const float dx = w[3] - m_bounds.middle[0];
    const float dz = w[11] - m_bounds.middle[2];
    if (dz * dz + dx * dx > def.lodOffMul * 6.0f) return true;
    // 0x0050997F: near enough, yellow.
    if (g_drawBounds) drawCircle(camera, m_system.engine().device(), m_bounds.middle, m_bounds.radius, 0xFFFFFF00u);
    // [0x00719210] 1: the step goes to the job thread (0x00509870, its count of users raised for the job) and the
    // frame draws the record drawn when it is taken; here it runs at once.
    simulate(m_system.dt);
    const Record& r = m_records[m_written & 1];
    if (r.count == 0 || m_material == nullptr) return true;
    render::IRenderDevice& device = m_system.engine().device();
    m_system.engine().renderer().beginMaterialPass(*m_material, 0);
    device.updateVertexBuffer(m_buffer, r.vertices.data(), static_cast<uint32_t>(m_vertexCount * sizeof(Vertex)));
    device.setRenderState(RenderState::CullMode, Cull::None);
    // 0x00508970. Stock problem: the count of triangles is the leaves', so only the first half of them is drawn. The
    // fixed build draws two triangles a leaf: every leaf.
    device.setStream(m_buffer, sizeof(Vertex));
    device.setIndices(m_system.quadIndices(m_points));
    device.drawIndexed(Primitive::TriangleList, r.count * 4, 0, r.count * 2);
    // 0x00509B25: drawn, its bounds again in olive (0xFF373700).
    if (g_drawBounds) drawCircle(camera, device, m_bounds.middle, m_bounds.radius, 0xFF373700u);
    return true;
}

}  // namespace oj::scene
