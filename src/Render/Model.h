#pragma once
// Model meshes and nodes drawn as the stock engine draws them (docs/spec/12-rendering.md): a node sets the object
// constants, a mesh draws its parts within their distance range with their materials.

#include "Core/Formats/Model.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/Renderer3d.h"

#include <memory>
#include <vector>

namespace oj::render {

class ModelNode;

class Mesh {
public:
    struct Part {
        Material* material = nullptr;
        uint32_t indexStart = 0;
        uint32_t indexCount = 0;
        uint32_t faceType = 0;
        float lodIn = 0.0f;
        float lodOut = 0.0f;
        uint32_t flags = 0;
        uint32_t stride = 0;
        uint32_t vertexCount = 0;
        BufferId vertices = 0;
        DeclarationId declaration = 0;
    };

    // The buffers of a parsed mesh; the materials by part, in the mesh's part order.
    Mesh(IRenderDevice& device, const oj::Mesh& data, const std::vector<Material*>& materials);

    const std::vector<Part>& parts() const { return m_parts; }
    // The bounding sphere's radius of the whole mesh (a mesh node's sphere, its +0x74).
    float radius() const { return m_radius; }
    // The mesh's box (its bounds' least and greatest corner), as a mesh node gives it (its +4 method).
    const std::array<float, 3>& boundsMin() const { return m_min; }
    const std::array<float, 3>& boundsMax() const { return m_max; }

    // 0x00559010: mode 0 every part, 1 the parts of opaque classes, 2 the others; a part of flag 4 only for a node
    // of flag 8 (none of the game's meshes has one).
    void draw(Renderer3d& r, Camera& camera, ModelNode& node, uint32_t mode, bool knocked);
    // 0x00559310 (vtable +0x28): one part as draw takes it, its distance without the detail's share and no mode; a
    // node of flag 0x10 gives a material of class 0 its parameter 8 = 1 first. Only ModelNode::drawPart calls it.
    void drawPart(Renderer3d& r, Camera& camera, ModelNode& node, bool knocked, uint32_t part);
    // 0x005582B0 (slot 5 of the mesh's interface 0x006FA284; nothing in the game calls it): the mesh's file read again
    // into its parts' vertex buffers, as after their contents were lost: the index count and the indices passed over,
    // then for each part its 100 bytes, its declaration and two values passed over and stride x count bytes (the
    // mesh's own, +0x68 x +0x64) read into its buffer locked whole (0x00558429); false at a part the file lacks (the
    // stock game's read past the file's end) or with no buffer.
    bool reloadVertices(IRenderDevice& device, const oj::Mesh& file);

private:
    BufferId m_indices = 0;
    std::vector<Part> m_parts;
    // +0x58: a part has flag 4, the parts drawn in place of the others for a node knocked over (the node's bit 8, set
    // on a light's node knocked sideways; none of the game's meshes has one).
    bool m_hasKnockedParts = false;
    float m_radius = 0.0f;
    std::array<float, 3> m_min{};
    std::array<float, 3> m_max{};
};

class ModelNode : public Node {
public:
    // The four random values of the node (0x005550D0: four rand() × 1/32767).
    explicit ModelNode(const Vec4& random) : m_random(random) {}

    Mesh* mesh = nullptr;     // +0xA8
    uint32_t flags = 0;       // +0xC0: 1 mirrored, 4 hidden, 8 passed to the mesh, 0x10 a car part knocked off
    float lightScale = 1.0f;  // +0xC4

    // 0x00556900: the object constants (0x005564B0), then the mesh (0x005567D0).
    void draw(Renderer3d& r, Camera& camera, uint32_t mode);
    // 0x00556920 (vtable 0x006FA208 slot 3): the same with one part of the mesh (Mesh::drawPart). Nothing in the
    // game calls the slot.
    void drawPart(Renderer3d& r, Camera& camera, uint32_t part);

private:
    void setConstants(Renderer3d& r, Camera& camera);

    Vec4 m_random;  // +0xAC
};

}  // namespace oj::render
