#include "Game/Scene/Projector.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Core/Base/Matrix.h"
#include "Game/Scene/Scene3d.h"

#include <algorithm>
#include <cmath>

namespace oj::scene {

using render::Primitive;

namespace {

constexpr size_t kVertices = 300;  // the vertices kept (+0x68)
constexpr size_t kTaken = 100;     // the faces marked (+0x17DC)

}  // namespace

Projector::Projector(Engine& engine, render::Node& node, const Vec4& low, const Vec4& high,
                     const render::GameTexture* shadow, const render::GameTexture* glow)
    : m_engine(engine), m_node(node), m_low(low), m_high(high) {
    const float lowLength = std::sqrt((low[2] * low[2] + low[1] * low[1]) + low[0] * low[0]);
    const float highLength = std::sqrt((high[2] * high[2] + high[1] * high[1]) + high[0] * high[0]);
    const Vec4& far = highLength <= lowLength ? low : high;
    m_radius = std::sqrt((far[2] * far[2] + far[1] * far[1]) + far[0] * far[0]);
    m_vertices.reserve(kVertices);
    m_taken.reserve(kTaken);
    render::IRenderDevice& device = engine.device();
    const render::VertexElement elements[] = {
        {0, 0, 2, 0, 0, 0},   // POSITION, FLOAT3
        {0, 12, 1, 0, 5, 0},  // TEXCOORD0, FLOAT2
        {0xFF, 0, 17, 0, 0, 0},
    };
    m_declaration = device.createDeclaration(elements);
    if (shadow != nullptr) {
        m_shadow = engine.newMaterial(15, nullptr);
        if (m_shadow) m_shadow->setTexture(0, shadow);
    }
    if (glow != nullptr) {
        m_glow = engine.newMaterial(28, nullptr);
        if (m_glow) {
            m_glow->setTexture(0, glow);
            const uint32_t additive = 1;
            m_glow->setParam(4, &additive);
            const uint32_t white = 0xFFFFFFFF;
            m_glow->setParam(0, &white);
        }
    }
    const std::vector<uint8_t> zeros(0x400 * 0x14);
    m_buffer = device.createVertexBuffer(zeros.data(), static_cast<uint32_t>(zeros.size()));
}

Projector::~Projector() {
    if (m_buffer != 0) m_engine.device().destroyBuffer(m_buffer);
}

void Projector::setTexture(bool glow, const render::GameTexture* texture, bool flipU, bool flipV) {
    render::Material* material = glow ? m_glow.get() : m_shadow.get();
    if (material != nullptr) {
        material->setTexture(0, texture);
        m_engine.renderer().forgetMaterial(material);
    }
    m_flipU = flipU;
    m_flipV = flipV;
}

void Projector::setColour(uint32_t argb) {
    if (!m_glow) return;
    m_glow->setParam(0, &argb);
    m_engine.renderer().forgetMaterial(m_glow.get());
}

void Projector::take(const Cld& cld, const std::vector<uint32_t>& faces) {
    for (uint32_t id : faces) {
        if (kVertices - m_vertices.size() < 4) return;
        const CldFace& f = cld.faces[id];
        if ((m_marked[id] & 0x80) != 0 || !(0.5f < f.normal[1])) continue;
        const auto& a = cld.vertices[f.vertices[0]];
        const auto& b = cld.vertices[f.vertices[1]];
        const auto& c = cld.vertices[f.vertices[2]];
        const float ay = a[1] + 5.0f, by = b[1] + 5.0f, cy = c[1] + 5.0f;
        if (!(ay < m_top || by < m_top || cy < m_top)) continue;
        // The u and v of these are values left on the stack; the update sets them.
        m_vertices.push_back({a[0], ay, a[2], 0.0f, 0.0f});
        m_vertices.push_back({b[0], by, b[2], 0.0f, 0.0f});
        m_vertices.push_back({c[0], cy, c[2], 0.0f, 0.0f});
        m_marked[id] |= 0x80;
        if (kTaken - m_taken.size() < 2) return;
        m_taken.push_back(id);
    }
}

void Projector::gather(const Cld& cld, int32_t at, float minX, float minZ, float maxX, float maxZ) {
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

void Projector::clip(const std::vector<Vertex>& from, std::vector<Vertex>& to, float a, float b, float d) {
    to.clear();
    std::array<Vertex, 6> polygon{};  // [0x0079F220]
    for (size_t t = 0; t + 2 < from.size(); t += 3) {
        size_t count = 0;
        const Vertex* prev = &from[t + 2];
        bool prevIn = d < b * prev->v + a * prev->u;
        for (size_t k = 0; k < 3; ++k) {
            const Vertex& cur = from[t + k];
            const bool curIn = d < b * cur.v + a * cur.u;
            if (prevIn != curIn) {
                const float atPrev = a * prev->u + b * prev->v;
                const float s = (d - atPrev) / ((b * cur.v + a * cur.u) - atPrev);
                auto lerp = [s](float to, float from) { return (to - from) * s + from; };
                polygon[count++] = {lerp(cur.x, prev->x), lerp(cur.y, prev->y), lerp(cur.z, prev->z),
                                    lerp(cur.u, prev->u), lerp(cur.v, prev->v)};
            }
            if (curIn) polygon[count++] = cur;
            prev = &cur;
            prevIn = curIn;
        }
        for (size_t k = 2; k < count; ++k) {
            if (kVertices - to.size() < 4) return;
            to.push_back(polygon[0]);
            to.push_back(polygon[k - 1]);
            to.push_back(polygon[k]);
        }
    }
}

void Projector::update(const Cld& cld) {
    m_vertices.clear();
    if (m_marked.size() != cld.faces.size()) m_marked.assign(cld.faces.size(), 0);
    const Mat34& w = m_node.world();
    // The node's x and z axes laid on the ground, their lengths across it made up from the y axis.
    const float lenX = std::sqrt(w[8] * w[8] + w[0] * w[0]);
    const float lenZ = std::sqrt(w[10] * w[10] + w[2] * w[2]);
    const float signX = w[9] * w[8] + w[1] * w[0] > 0.0f ? 1.0f : -1.0f;
    const float signZ = w[10] * w[9] + w[2] * w[1] > 0.0f ? 1.0f : -1.0f;
    const float kX = (1.0f - lenX) * signX;
    const float kZ = (1.0f - lenZ) * signZ;
    const Vec4 u{lenX * w[0] + w[1] * kX, lenX * 0.0f + kX * 0.0f, lenX * w[8] + kX * w[9], 0.0f};
    const Vec4 v{lenZ * w[2] + w[1] * kZ, lenZ * 0.0f + kZ * 0.0f, lenZ * w[10] + kZ * w[9], 0.0f};
    Vec4 origin{w[3], w[7] + m_high[1], w[11], 0.0f};
    m_top = w[7] + m_high[1];
    // The box across the ground, each axis's ends in the order of its sign.
    float minX = 0.0f, maxX = 0.0f, minZ = 0.0f, maxZ = 0.0f;
    if (u[0] >= 0.0f) {
        minX = u[0] * m_low[0] + origin[0];
        maxX = u[0] * m_high[0] + origin[0];
    } else {
        minX = u[0] * m_high[0] + origin[0];
        maxX = u[0] * m_low[0] + origin[0];
    }
    if (u[2] >= 0.0f) {
        minZ = u[2] * m_low[0] + origin[2];
        maxZ = u[2] * m_high[0] + origin[2];
    } else {
        minZ = u[2] * m_high[0] + origin[2];
        maxZ = u[2] * m_low[0] + origin[2];
    }
    if (v[0] >= 0.0f) {
        minX = v[0] * m_low[2] + minX;
        maxX = v[0] * m_high[2] + maxX;
    } else {
        minX = v[0] * m_high[2] + minX;
        maxX = v[0] * m_low[2] + maxX;
    }
    if (v[2] >= 0.0f) {
        minZ = v[2] * m_low[2] + minZ;
        maxZ = v[2] * m_high[2] + maxZ;
    } else {
        minZ = v[2] * m_high[2] + minZ;
        maxZ = v[2] * m_low[2] + maxZ;
    }
    // 0x0054BFF0: from the tree's root.
    if (!cld.nodes.empty()) gather(cld, 0, minX, minZ, maxX, maxZ);
    for (uint32_t id : m_taken) m_marked[id] &= 0x7F;
    m_taken.clear();

    // u and v of each vertex: from the box's low corner along its edges, over their lengths squared.
    const Vec4 lowV = scale(v, m_low[2]);
    const Vec4 lowU = scale(u, m_low[0]);
    origin = {(lowU[0] + lowV[0]) + origin[0], (lowU[1] + lowV[1]) + origin[1], (lowU[2] + lowV[2]) + origin[2],
              0.0f};
    Vec4 edgeU = scale(u, m_high[0] - m_low[0]);
    Vec4 edgeV = scale(v, m_high[2] - m_low[2]);
    if (m_flipU) {
        origin = {origin[0] + edgeU[0], origin[1] + edgeU[1], origin[2] + edgeU[2], 0.0f};
        edgeU = {-edgeU[0], -edgeU[1], -edgeU[2], 0.0f};
    }
    if (m_flipV) {
        origin = {origin[0] + edgeV[0], origin[1] + edgeV[1], origin[2] + edgeV[2], 0.0f};
        edgeV = {-edgeV[0], -edgeV[1], -edgeV[2], 0.0f};
    }
    const float lengthU = std::sqrt((edgeU[2] * edgeU[2] + edgeU[1] * edgeU[1]) + edgeU[0] * edgeU[0]);
    const float lengthV = std::sqrt((edgeV[2] * edgeV[2] + edgeV[1] * edgeV[1]) + edgeV[0] * edgeV[0]);
    const float overU = 1.0f / (lengthU * lengthU);
    const float overV = 1.0f / (lengthV * lengthV);
    const Vec4 alongU = scale(edgeU, overU);
    const Vec4 alongV = scale(edgeV, overV);
    for (Vertex& p : m_vertices) {
        const float dx = p.x - origin[0], dy = p.y - origin[1], dz = p.z - origin[2];
        p.u = (alongU[2] * dz + alongU[1] * dy) + alongU[0] * dx;
        p.v = (alongV[2] * dz + alongV[1] * dy) + alongV[0] * dx;
    }
    // Kept where 0 < u < 1 and 0 < v < 1, through the static list [0x007A0E20] and back.
    static std::vector<Vertex> other;
    other.reserve(kVertices);
    clip(m_vertices, other, 1.0f, 0.0f, 0.0f);
    clip(other, m_vertices, -1.0f, 0.0f, -1.0f);
    clip(m_vertices, other, 0.0f, 1.0f, 0.0f);
    clip(other, m_vertices, 0.0f, -1.0f, -1.0f);
}

bool Projector::seen(render::Camera& camera) {
    if (!on) return false;
    const Mat34& w = m_node.world();
    const Vec4 centre{w[3], w[7], w[11], 1.0f};
    if (!camera.sphereVisible(centre, m_radius)) return false;
    const Mat34& c = camera.world();
    const float dx = c[3] - centre[0];
    const float dz = c[11] - centre[2];
    const float across = (dz * dz + dx * dx) - m_radius * m_radius;
    return camera.lodScale() * across <= 100000000.0f;
}

void Projector::draw(render::Camera& camera) {
    if (graphicsConfig().shadows < 1 || m_vertices.empty() || !seen(camera)) return;
    render::Renderer3d& r = m_engine.renderer();
    render::IRenderDevice& device = m_engine.device();
    if (r.lastDeclaration != m_declaration) {
        device.setDeclaration(m_declaration);
        r.lastDeclaration = m_declaration;
    }
    const size_t count = std::min<size_t>(m_vertices.size(), 0x3FF);
    device.updateVertexBuffer(m_buffer, m_vertices.data(), static_cast<uint32_t>(count * sizeof(Vertex)));
    device.setStream(m_buffer, 0x14);
    const auto triangles = static_cast<uint32_t>(m_vertices.size() / 3);
    if (m_shadow) {
        r.beginMaterialPass(*m_shadow, 0);
        device.draw(Primitive::TriangleList, 0, triangles);
    }
    if (m_glow) {
        r.beginMaterialPass(*m_glow, 0);
        device.draw(Primitive::TriangleList, 0, triangles);
    }
}

}  // namespace oj::scene
