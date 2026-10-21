#include "Game/Scene/Silhouette.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Core/Base/Matrix.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"
#include "Render/Model.h"
#include "Render/Sky.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace oj::scene {

using render::ClearFlags;
using render::Primitive;
using render::RenderState;

namespace {

constexpr size_t kVertices = 1000;  // the buffer's vertices; a face or a triangle more only below 998
constexpr uint32_t kTargetSize = 0x100;  // the graphics settings' +0x18 ([0x0079E618])
constexpr float kFar = 700.0f;           // [0x006FFC00]: a sphere beyond it ends the walk
constexpr float kEpsilon = 1.1920929e-7f;  // [0x006FF99C]: 0x00407C00 divides by the length plus it

float length3(float x, float y, float z) { return std::sqrt((x * x + y * y) + z * z); }

}  // namespace

Silhouette::Silhouette(Engine& engine, render::Node& node, int type, DrawCar drawCar)
    : m_engine(engine), m_node(node), m_type(type), m_drawCar(std::move(drawCar)) {
    render::IRenderDevice& device = engine.device();
    const render::VertexElement elements[] = {
        {0, 0, 2, 0, 0, 0},    // POSITION, FLOAT3
        {0, 12, 3, 0, 10, 0},  // COLOR, FLOAT4
        {0, 28, 1, 0, 5, 0},   // TEXCOORD0, FLOAT2
        {0xFF, 0, 17, 0, 0, 0},
    };
    m_declaration = device.createDeclaration(elements);
    m_shadow = engine.newMaterial(34, nullptr);
    if (m_shadow) {
        const float offset = 0.5f / static_cast<float>(kTargetSize);
        m_shadow->setParam(0, &offset);
    }
    m_silhouette = engine.newMaterial(2, nullptr);
    if (m_silhouette) {
        const uint32_t white = 0xFFFFFFFFu;
        m_silhouette->setParam(0, &white);
    }
    // 0x00548FD0: the target (0x0051F4E0) the shadow's texture; the orthographic camera (0x0054CCC0) and the view of
    // the target (0x00512130, cleared to 0).
    m_target.id = device.createRenderTarget(kTargetSize, kTargetSize, false);
    if (m_shadow && m_target.id != 0) m_shadow->setTexture(0, &m_target);
    m_camera.setOrthographic(true);
    sphere(node);
    const Mat34& w = node.world();
    m_centre = {m_centre[0] - w[3], m_centre[1] - w[7], m_centre[2] - w[11], m_centre[3]};
    m_camera.setNear(0.01f);
    m_vertices.reserve(kVertices);
    const std::vector<uint8_t> zeros(kVertices * sizeof(Vertex));
    m_buffer = device.createVertexBuffer(zeros.data(), static_cast<uint32_t>(zeros.size()));
}

Silhouette::~Silhouette() {
    render::IRenderDevice& device = m_engine.device();
    if (m_buffer != 0) device.destroyBuffer(m_buffer);
    if (m_target.id != 0) device.destroyTexture(m_target.id);
}

// 0x00549500: a mesh node's sphere (its place in the world, its mesh's radius) is the first one taken, or is merged
// with the one so far: its radius over the longest of the node's own axes (Stock problem: divided, not multiplied),
// the two spheres' far points along the line between their centres; a sphere of radius above 700, or 700 away, ends
// the walk of that node (its children too). Then the children.
void Silhouette::sphere(render::Node& node) {
    auto* model = dynamic_cast<render::ModelNode*>(&node);
    if (model != nullptr && model->mesh != nullptr) {
        const Mat34& w = node.world();
        const Vec4 c{w[3], w[7], w[11], 0.0f};
        float r = model->mesh->radius();
        if (m_radius == 0.0f) {
            m_centre = c;
            m_radius = r;
        } else {
            const Mat34& l = node.local;
            float longest = length3(l[0], l[4], l[8]);
            const float second = length3(l[1], l[5], l[9]);
            const float third = length3(l[2], l[6], l[10]);
            if (second > longest) longest = second;
            if (third > longest) longest = third;
            r = r / longest;
            if (r > kFar) return;
            const Vec4 diff{c[0] - m_centre[0], c[1] - m_centre[1], c[2] - m_centre[2], 0.0f};
            const float d = length3(diff[0], diff[1], diff[2]);
            const float k = 1.0f / (d + kEpsilon);
            const Vec4 dir{diff[0] * k, diff[1] * k, diff[2] * k, 0.0f};
            if (d > kFar) return;
            float a = d + r;
            if (a < m_radius) a = m_radius;
            const Vec4 p1{m_centre[0] + dir[0] * a, m_centre[1] + dir[1] * a, m_centre[2] + dir[2] * a, 0.0f};
            float b = d + m_radius;
            if (b < r) b = r;
            const Vec4 p2{c[0] - dir[0] * b, c[1] - dir[1] * b, c[2] - dir[2] * b, 0.0f};
            m_centre = {(p1[0] + p2[0]) * 0.5f, (p1[1] + p2[1]) * 0.5f, (p1[2] + p2[2]) * 0.5f, m_centre[3]};
            m_radius = length3(p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]) * 0.5f;
        }
    }
    for (render::Node* child : node.children()) sphere(*child);
}

void Silhouette::take(const Cld& cld, const std::vector<uint32_t>& faces) {
    for (uint32_t id : faces) {
        const CldFace& f = cld.faces[id];
        if ((m_marked[id] & 0x80) != 0 || !(0.5f < f.normal[1])) continue;
        if (m_vertices.size() > 997) return;
        // Raised by 9; the colour, u and v are values left on the stack, which the update sets.
        for (int k = 0; k < 3; ++k) {
            const auto& p = cld.vertices[f.vertices[k]];
            m_vertices.push_back({p[0], p[1] + 9.0f, p[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
        }
        m_marked[id] |= 0x80;
        m_taken.push_back(id);
    }
}

void Silhouette::gather(const Cld& cld, int32_t at, float minX, float minZ, float maxX, float maxZ) {
    if (at < 0) return;
    const Cld::Node& n = cld.nodes[static_cast<size_t>(at)];
    if (n.leaf) {
        take(cld, n.faces);
        return;
    }
    const float sx = n.split[0], sz = n.split[1];
    const bool lowX = minX < sx;
    const bool highX = !lowX || sx < maxX;
    const bool lowZ = minZ < sz;
    const bool highZ = !lowZ || sz < maxZ;
    if (lowX) {
        if (lowZ) gather(cld, n.children[0], minX, minZ, maxX, maxZ);
        if (highZ) gather(cld, n.children[2], minX, minZ, maxX, maxZ);
    }
    if (highX) {
        if (lowZ) gather(cld, n.children[1], minX, minZ, maxX, maxZ);
        if (highZ) gather(cld, n.children[3], minX, minZ, maxX, maxZ);
    }
}

// 0x00549D80 with 0x00549C00: each triangle clipped to a * u + b * v > d (an edge that crosses it cut: the place,
// the alpha, u and v in between, the colour 0) and laid out again from its first corner while the list is at most
// 997.
void Silhouette::clip(const std::vector<Vertex>& from, std::vector<Vertex>& to, float a, float b, float d) {
    to.clear();
    std::array<Vertex, 6> polygon{};
    for (size_t t = 0; t + 2 < from.size(); t += 3) {
        size_t count = 0;
        const Vertex* prev = &from[t + 2];
        bool prevIn = d < b * prev->v + a * prev->u;
        for (size_t k = 0; k < 3; ++k) {
            const Vertex& cur = from[t + k];
            const bool curIn = d < b * cur.v + a * cur.u;
            if (prevIn != curIn) {
                const float atPrev = b * prev->v + a * prev->u;
                const float s = (d - atPrev) / ((b * cur.v + a * cur.u) - atPrev);
                auto lerp = [s](float to, float from) { return (to - from) * s + from; };
                polygon[count++] = {lerp(cur.x, prev->x), lerp(cur.y, prev->y), lerp(cur.z, prev->z), 0.0f, 0.0f,
                                    0.0f, lerp(cur.a, prev->a), lerp(cur.u, prev->u), lerp(cur.v, prev->v)};
            }
            if (curIn) polygon[count++] = cur;
            prev = &cur;
            prevIn = curIn;
        }
        for (size_t k = 2; k < count; ++k) {
            if (to.size() > 997) return;
            to.push_back(polygon[0]);
            to.push_back(polygon[k - 1]);
            to.push_back(polygon[k]);
        }
    }
}

void Silhouette::update(float fade, const render::AtmosphereSet* atmosphere, const NearestLights& nearest,
                        const Cld& cld) {
    m_fade = fade;
    m_vertices.clear();
    const Mat34& w = m_node.world();
    const Vec4 place{w[3], w[7], w[11], 0.0f};
    // The light's direction: the sun's (0x005341F0); of a light near the car, from it to the car, the fade by
    // 2 - 0.001 x their distance across the ground (at least 0); none, nothing this frame.
    Vec4 light{};
    if (m_type == 0) {
        if (atmosphere == nullptr || atmosphere->current == nullptr) return;
        light = render::atmosphereLightDirection(*atmosphere);
    } else {
        render::Node* first = nullptr;
        render::Node* second = nullptr;
        if (nearest) nearest(place, first, second);
        render::Node* lamp = m_type == 1 ? first : second;
        if (lamp == nullptr) return;
        const Mat34& lw = lamp->world();
        const float dx = place[0] - lw[3], dy = place[1] - lw[7], dz = place[2] - lw[11];
        float k = 2.0f - std::sqrt(dx * dx + dz * dz) * 0.001f;
        if (k < 0.0f) k = 0.0f;
        m_fade = m_fade * k;
        const float n = 1.0f / (length3(dx, dy, dz) + kEpsilon);
        light = {dx * n, dy * n, dz * n, 0.0f};
    }
    // S across the light (the light x (1, 0, 0), made unit), U = S x the light.
    Vec4 s{light[1] * 0.0f - light[2] * 0.0f, light[2] - light[0] * 0.0f, light[0] * 0.0f - light[1], 0.0f};
    {
        const float n = 1.0f / (length3(s[0], s[1], s[2]) + kEpsilon);
        s = {s[0] * n, s[1] * n, s[2] * n, 0.0f};
    }
    const Vec4 u{s[1] * light[2] - s[2] * light[1], s[2] * light[0] - s[0] * light[2],
                 s[0] * light[1] - s[1] * light[0], 0.0f};
    const float r = m_radius;
    const Vec4 c{m_centre[0] + place[0], m_centre[1] + place[1], m_centre[2] + place[2], 0.0f};
    // The camera: its axes U, S and the light, at the centre less the light x (radius + 5); width and height the
    // radius, far 3 x the radius + 10.
    const float back = r + 5.0f;
    m_camera.setFov(r, r, false);
    m_camera.local = {u[0], s[0], light[0], c[0] - light[0] * back,
                      u[1], s[1], light[1], c[1] - light[1] * back,
                      u[2], s[2], light[2], c[2] - light[2] * back};
    m_camera.invalidate();
    m_camera.setFar(r * 3.0f + 10.0f);
    // 0x00515AA0, the renderer's pass kind 1 and 0x00511CA0: the car drawn with <silhouette> into the target, then
    // the renderer's material and pass kind back.
    if (m_target.id != 0 && m_silhouette && m_drawCar) {
        render::Renderer3d& rr = m_engine.renderer();
        render::IRenderDevice& device = m_engine.device();
        for (uint32_t stage = 0; stage < 4; ++stage) device.setTexture(stage, 0);
        m_engine.postEffects().forgetTarget();
        rr.endMaterial();
        device.setRenderTarget(m_target.id, 0);
        device.clear(ClearFlags::Target | ClearFlags::ZBuffer, 0, 1.0f, 0);
        device.setRenderState(RenderState::FogEnable, 0);
        m_camera.update();
        setViewConstants(rr, m_camera);
        rr.override = m_silhouette.get();
        rr.passKind = 1;
        m_drawCar(rr, m_camera);
        rr.override = nullptr;
        rr.passKind = 0;
        rr.endMaterial();
        device.setRenderTarget(0, 0);
    }
    // The box the light sweeps across the ground: the centre ± U' ± S' (U' = U x the radius, S' likewise), from
    // the light x -radius to the light x (radius + 300).
    const Vec4 su{u[0] * r, u[1] * r, u[2] * r, 0.0f};
    const Vec4 ss{s[0] * r, s[1] * r, s[2] * r, 0.0f};
    float maxX = c[0], maxZ = c[2], minX = c[0], minZ = c[2];
    auto widen = [&](float x, float z) {
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (z < minZ) minZ = z;
        if (z > maxZ) maxZ = z;
    };
    for (const float along : {-r, r + 300.0f}) {
        const float lx = light[0] * along, lz = light[2] * along;
        widen(((c[0] + su[0]) + ss[0]) + lx, ((c[2] + su[2]) + ss[2]) + lz);
        widen(((c[0] + su[0]) - ss[0]) + lx, ((c[2] + su[2]) - ss[2]) + lz);
        widen(((c[0] - su[0]) + ss[0]) + lx, ((c[2] - su[2]) + ss[2]) + lz);
        widen(((c[0] - su[0]) - ss[0]) + lx, ((c[2] - su[2]) - ss[2]) + lz);
    }
    if (m_marked.size() != cld.faces.size()) m_marked.assign(cld.faces.size(), 0);
    if (!cld.nodes.empty()) gather(cld, 0, minX, minZ, maxX, maxZ);
    for (uint32_t id : m_taken) m_marked[id] &= 0x7F;
    m_taken.clear();
    // u and v from O = the centre - U' - S' along 2U' and 2S' over their lengths squared (v from 1 down); the
    // colour 0, the alpha 0.25 x the fade.
    const Vec4 o{(c[0] - su[0]) - ss[0], (c[1] - su[1]) - ss[1], (c[2] - su[2]) - ss[2], 0.0f};
    const Vec4 u2{su[0] + su[0], su[1] + su[1], su[2] + su[2], 0.0f};
    const Vec4 s2{ss[0] + ss[0], ss[1] + ss[1], ss[2] + ss[2], 0.0f};
    const float lu = length3(u2[0], u2[1], u2[2]);
    const float ls = length3(s2[0], s2[1], s2[2]);
    const float ku = 1.0f / (lu * lu);
    const float ks = 1.0f / (ls * ls);
    const Vec4 au{u2[0] * ku, u2[1] * ku, u2[2] * ku, 0.0f};
    const Vec4 as{s2[0] * ks, s2[1] * ks, s2[2] * ks, 0.0f};
    const float alpha = 0.25f * m_fade;
    for (Vertex& p : m_vertices) {
        const float dx = p.x - o[0], dy = p.y - o[1], dz = p.z - o[2];
        p.r = p.g = p.b = 0.0f;
        p.u = (au[2] * dz + au[1] * dy) + au[0] * dx;
        p.v = 1.0f - ((as[2] * dz + as[1] * dy) + as[0] * dx);
        p.a = alpha;
    }
    static std::vector<Vertex> other;
    other.reserve(kVertices);
    clip(m_vertices, other, 1.0f, 0.0f, 0.0f);
    clip(other, m_vertices, -1.0f, 0.0f, -1.0f);
    clip(m_vertices, other, 0.0f, 1.0f, 0.0f);
    clip(other, m_vertices, 0.0f, -1.0f, -1.0f);
}

// 0x00552FD0, as the projected textures': on, the node's place and the radius in the view and near enough.
bool Silhouette::seen(render::Camera& camera) {
    if (!on) return false;
    const Mat34& w = m_node.world();
    const Vec4 centre{w[3], w[7], w[11], 1.0f};
    if (!camera.sphereVisible(centre, m_radius)) return false;
    const Mat34& cw = camera.world();
    const float dx = cw[3] - centre[0];
    const float dz = cw[11] - centre[2];
    const float across = (dz * dz + dx * dx) - m_radius * m_radius;
    return camera.lodScale() * across <= 100000000.0f;
}

void Silhouette::draw(render::Camera& camera) {
    if (graphicsConfig().shadows <= 1 || m_vertices.empty() || m_fade == 0.0f || !seen(camera) || !m_shadow) return;
    render::Renderer3d& r = m_engine.renderer();
    render::IRenderDevice& device = m_engine.device();
    const size_t count = std::min<size_t>(m_vertices.size(), 999);
    device.updateVertexBuffer(m_buffer, m_vertices.data(), static_cast<uint32_t>(count * sizeof(Vertex)));
    if (r.lastDeclaration != m_declaration) {
        device.setDeclaration(m_declaration);
        r.lastDeclaration = m_declaration;
    }
    r.beginMaterialPass(*m_shadow, 0);
    device.setStream(m_buffer, sizeof(Vertex));
    device.draw(Primitive::TriangleList, 0, static_cast<uint32_t>(count / 3));
}

}  // namespace oj::scene
