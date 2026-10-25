#include "Render/Model.h"

#include <algorithm>
#include <cstring>

namespace oj::render {

namespace {


}  // namespace

Mesh::Mesh(IRenderDevice& device, const oj::Mesh& data, const std::vector<Material*>& materials) {
    m_indices = device.createIndexBuffer(data.indices.data(), static_cast<uint32_t>(data.indices.size()));
    m_radius = data.bounds.radius;
    m_min = data.bounds.min;
    m_max = data.bounds.max;
    m_parts.reserve(data.parts.size());
    for (size_t i = 0; i < data.parts.size(); ++i) {
        const MeshPart& p = data.parts[i];
        Part part;
        part.material = i < materials.size() ? materials[i] : nullptr;
        part.indexStart = p.indexStart;
        part.indexCount = p.indexCount;
        part.faceType = p.faceType;
        part.lodIn = p.lodIn;
        part.lodOut = p.lodOut;
        part.flags = p.renderFlag;
        part.stride = p.stride;
        part.vertexCount = p.vertexCount;
        part.vertices = device.createVertexBuffer(p.vertices.data(), static_cast<uint32_t>(p.vertices.size()));
        std::vector<VertexElement> elements(p.declaration.size());
        for (size_t e = 0; e < elements.size(); ++e) {
            const oj::VertexElement& v = p.declaration[e];
            elements[e] = {v.stream, v.offset, v.type, v.method, v.usage, v.usageIndex};
        }
        if (!elements.empty()) part.declaration = device.createDeclaration(elements.data());
        if ((part.flags & 4) != 0) m_hasKnockedParts = true;
        m_parts.push_back(part);
    }
}

bool Mesh::reloadVertices(IRenderDevice& device, const oj::Mesh& file) {
    for (size_t i = 0; i < m_parts.size(); ++i) {
        const Part& part = m_parts[i];
        if (i >= file.parts.size() || part.vertices == 0) return false;
        const std::vector<uint8_t>& bytes = file.parts[i].vertices;
        const size_t size = std::min<size_t>(static_cast<size_t>(part.stride) * part.vertexCount, bytes.size());
        device.updateVertexBuffer(part.vertices, bytes.data(), static_cast<uint32_t>(size));
    }
    return true;
}

void Mesh::draw(Renderer3d& r, Camera& camera, ModelNode& node, uint32_t mode, bool knocked) {
    const Mat34& c = camera.world();
    const float cx = c[3], cy = c[7], cz = c[11];
    const Mat34& n = node.world();
    const float dx = n[3] - cx;
    const float dy = n[7] - cy;
    const float dz = n[11] - cz;
    const float distance = (dz * dz + dy * dy) + dx * dx;
    // The constants are doubles; each step rounds to a float.
    const float shifted = static_cast<float>(static_cast<double>(r.lodBias) - 0.5);
    float t = static_cast<float>(static_cast<double>(shifted) * (2.0 / 3.0));
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
    const float tenth = static_cast<float>(static_cast<double>(t) * 0.9);
    const float factor = static_cast<float>(1.0 - static_cast<double>(tenth));
    const float scaled = camera.lodScale() * factor * distance;

    IRenderDevice& device = r.device();
    device.setIndices(m_indices);
    // Stock problem: the late parts (mode 2) are drawn in their order, the car's glass writing no depth: what is
    // drawn after it behind it (the far side's decals, the smoke of the wheels) goes over it, and the glass looks
    // gone. The fixed build draws the late parts in two rounds, a car's glass (Car Glass) first, writing its depth
    // while it is drawn, then the others (the scenes draw every car's opaque parts before any car's late ones).
    const uint32_t rounds = mode == 2 && r.passKind == 0 ? 2u : 1u;
    for (uint32_t round = 0; round < rounds; ++round) {
        for (Part& part : m_parts) {
            if (r.passKind == 0) {
                // 0x00559145: the node's flag 8 (its argument, not the distance) with a part of flag 4 in the mesh.
                const uint32_t expected = (knocked && m_hasKnockedParts) ? 4u : 0u;
                if (!(scaled >= part.lodIn) || !(scaled < part.lodOut)) continue;
                if ((part.flags & 1) != 0 || (part.flags & 4) != expected) continue;
                if (mode == 1 && part.material != nullptr && (part.material->flags() & 0x24) != 0) continue;
                if (mode == 2 && part.material != nullptr && (part.material->flags() & 0x24) == 0) continue;
                if (rounds == 2) {
                    const bool glass = part.material != nullptr && part.material->classId() == kCarGlass;
                    if (glass != (round == 0)) continue;
                }
            } else if (r.passKind == 1) {
                if ((part.flags & 2) == 0) continue;
            } else {
                continue;
            }
            device.setStream(part.vertices, part.stride);
            if (part.declaration != r.lastDeclaration) {
                device.setDeclaration(part.declaration);
                r.lastDeclaration = part.declaration;
            }
            Material* material = r.override != nullptr ? r.override : part.material;
            if (material == nullptr) continue;
            // 0x005594C6: a node with +0xC0 bit 0x10 (a car part knocked off; nothing sets it) draws a material of the
            // class 0 (Car Body) double-sided, its parameter 8 set to 1 for good.
            if ((node.flags & 0x10) != 0 && material->classId() == kCarBody) {
                const uint32_t one = 1;
                material->setParam(8, &one);
            }
            const uint32_t passes = material->passes();
            const bool glassDepth = rounds == 2 && material->classId() == kCarGlass;
            for (uint32_t pass = 0; pass < passes; ++pass) {
                r.beginMaterialPass(*material, pass);
                if (glassDepth) device.setRenderState(RenderState::ZWriteEnable, 1);
                if (part.faceType == 0) {
                    device.drawIndexed(Primitive::TriangleStrip, part.vertexCount, part.indexStart,
                                       part.indexCount - 2);
                } else if (part.faceType == 1) {
                    device.drawIndexed(Primitive::TriangleList, part.vertexCount, part.indexStart, part.indexCount / 3);
                }
            }
            if (glassDepth) device.setRenderState(RenderState::ZWriteEnable, 0);
        }
    }
}

void ModelNode::setConstants(Renderer3d& r, Camera& camera) {
    const Mat34& w = world();
    const Mat44 toClip = multiply(camera.worldToClip(), w);
    Mat34 normal = w;
    const Vec4 scale = normalizeColumns(normal);
    float block[28];
    std::memcpy(block, w.data(), 48);
    block[12] = scale[0];
    block[13] = scale[1];
    block[14] = scale[2];
    block[15] = 0.0f;
    std::memcpy(block + 16, normal.data(), 48);
    r.setVertexConstants(0, block, 7);
    r.setVertexConstants(18, toClip.data(), 4);
}

void ModelNode::draw(Renderer3d& r, Camera& camera, uint32_t mode) {
    setConstants(r, camera);
    if ((flags & 4) != 0 || mesh == nullptr) return;
    if ((flags & 1) != 0) {
        r.defaultCull = r.defaultCull != 3 ? 3 : 2;
        r.endMaterial();
    }
    const Mat34& c = camera.world();
    const Mat34& n = world();
    r.objectOffset = {n[3] - c[3], n[7] - c[7], n[11] - c[11], r.objectOffset[3]};
    r.setLightScale(lightScale);
    r.setRandom(m_random);
    mesh->draw(r, camera, *this, mode, (flags & 8) != 0);
    if ((flags & 1) != 0) {
        r.defaultCull = r.defaultCull != 3 ? 3 : 2;
        r.endMaterial();
    }
    r.setLightScale(1.0f);
}

void Mesh::drawPart(Renderer3d& r, Camera& camera, ModelNode& node, bool knocked, uint32_t index) {
    const Mat34& c = camera.world();
    const Mat34& n = node.world();
    const float dx = n[3] - c[3];
    const float dy = n[7] - c[7];
    const float dz = n[11] - c[11];
    const float scaled = camera.lodScale() * ((dz * dz + dy * dy) + dx * dx);
    IRenderDevice& device = r.device();
    device.setIndices(m_indices);
    // A part past the count reads past the parts' records (nothing calls it); none drawn here.
    if (index >= m_parts.size()) return;
    Part& part = m_parts[index];
    if (r.passKind == 0) {
        const uint32_t expected = (knocked && m_hasKnockedParts) ? 4u : 0u;
        if (!(scaled >= part.lodIn) || !(scaled < part.lodOut)) return;
        if ((part.flags & 1) != 0 || (part.flags & 4) != expected) return;
    } else if (r.passKind == 1) {
        if ((part.flags & 2) == 0) return;
    } else {
        return;
    }
    device.setStream(part.vertices, part.stride);
    if (part.declaration != r.lastDeclaration) {
        device.setDeclaration(part.declaration);
        r.lastDeclaration = part.declaration;
    }
    Material* material = r.override != nullptr ? r.override : part.material;
    if (material == nullptr) return;
    // A node with flag 0x10 gives a material of class 0 its parameter 8 = 1 first.
    if ((node.flags & 0x10) != 0 && material->classId() == kCarBody) {
        const uint32_t one = 1;
        material->setParam(8, &one);
    }
    const uint32_t passes = material->passes();
    for (uint32_t pass = 0; pass < passes; ++pass) {
        r.beginMaterialPass(*material, pass);
        if (part.faceType == 0) {
            device.drawIndexed(Primitive::TriangleStrip, part.vertexCount, part.indexStart, part.indexCount - 2);
        } else if (part.faceType == 1) {
            device.drawIndexed(Primitive::TriangleList, part.vertexCount, part.indexStart, part.indexCount / 3);
        }
    }
}

void ModelNode::drawPart(Renderer3d& r, Camera& camera, uint32_t part) {
    setConstants(r, camera);
    if ((flags & 4) != 0 || mesh == nullptr) return;
    if ((flags & 1) != 0) {
        r.defaultCull = r.defaultCull != 3 ? 3 : 2;
        r.endMaterial();
    }
    const Mat34& c = camera.world();
    const Mat34& n = world();
    r.objectOffset = {n[3] - c[3], n[7] - c[7], n[11] - c[11], r.objectOffset[3]};
    r.setLightScale(lightScale);
    r.setRandom(m_random);
    mesh->drawPart(r, camera, *this, (flags & 8) != 0, part);
    if ((flags & 1) != 0) {
        r.defaultCull = r.defaultCull != 3 ? 3 : 2;
        r.endMaterial();
    }
    r.setLightScale(1.0f);
}

}  // namespace oj::render
