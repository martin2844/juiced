#pragma once
// A texture laid on the route's faces under a node (the class 0x0054AF00, 0x1970 bytes; docs/spec/12-rendering.md,
// "The projected textures"): a car's drop shadow with its neon and its headlights' beam. The world lists it (+0x98)
// and draws it before the scene objects.

#include "Core/Formats/Cld.h"
#include "Game/Scene/Silhouette.h"
#include "Render/Camera.h"
#include "Render/Materials.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace oj::scene {

class Engine;

class Projector final : public LaidObject {
public:
    // 0x00531260 / 0x0054AF00: under `node`, the box from `low` to `high` in the node's frame (x and z across the
    // ground, the top at the node's height + high y); with a shadow texture a Unlit ABlend material (class 15,
    // <drop_shadow>) of it (+0x30), with a glow texture a Coloured Spec ABlend one (class 28) of it, additive and white
    // (+0x34); the bounding radius the longer of the two corners (+0x28); a dynamic buffer of 0x400 vertices of 20
    // bytes (+0x2C).
    Projector(Engine& engine, render::Node& node, const Vec4& low, const Vec4& high,
              const render::GameTexture* shadow, const render::GameTexture* glow);
    ~Projector() override;

    // [2] 0x0054BF40: a texture for the glow's material (else the shadow's) and the flips of u and v (+0x60, +0x64).
    void setTexture(bool glow, const render::GameTexture* texture, bool flipU, bool flipV);
    // [3] 0x0054BF90: the glow's colour (a, r, g, b), with a glow.
    void setColour(uint32_t argb);
    // [1] 0x0054B7A0: the faces under the box (taken again each frame) clipped to it, with their u and v.
    void update(const Cld& cld);
    // [5] 0x0054BDF0: with the shadows' setting at 1 or more ([0x0079E648]), when there is something and it is seen
    // (0x00552FD0): the vertices into the buffer, a triangle list with the shadow's material, then with the glow's.
    void draw(render::Camera& camera) override;

private:
    // 0x00552FD0: on, its sphere (the node's place, the radius) in the view, and its distance across the ground
    // (less the radius, squared) by the camera's scale of distances at most 100000000.
    bool seen(render::Camera& camera);
    // 0x0054C050: the tree's nodes the box (x, z) meets, the low x side first and within it the low z side; each leaf's
    // faces through take().
    void gather(const Cld& cld, int32_t node, float minX, float minZ, float maxX, float maxZ);
    // 0x0054B2E0: of a leaf's faces, until the vertices fill (fewer than 4 places left), each not taken this frame
    // with its normal's y over 0.5 and a corner, raised by 5, below the top: its corners raised by 5, the face marked.
    void take(const Cld& cld, const std::vector<uint32_t>& faces);

    struct Vertex {
        float x, y, z, u, v;
    };
    // 0x0054B650: each triangle of `from` clipped to a * u + b * v > d (0x0054B4E0: each edge that crosses it cut,
    // all five values in between) and laid out again from its first corner, while 4 places are left in `to`.
    static void clip(const std::vector<Vertex>& from, std::vector<Vertex>& to, float a, float b, float d);

    Engine& m_engine;
    render::Node& m_node;                           // +0x14
    float m_radius = 0.0f;                          // +0x28
    std::unique_ptr<render::Material> m_shadow;     // +0x30
    std::unique_ptr<render::Material> m_glow;       // +0x34
    render::BufferId m_buffer = 0;                  // +0x2C
    render::DeclarationId m_declaration = 0;        // +0x3C
    Vec4 m_low{}, m_high{};                         // +0x40, +0x50
    bool m_flipU = false, m_flipV = false;          // +0x60, +0x64
    std::vector<Vertex> m_vertices;                 // +0x68 (count), +0x6C: up to 300
    std::vector<uint32_t> m_taken;                  // +0x17DC (count), +0x17E0: up to 100
    std::vector<uint8_t> m_marked;                  // the faces' bit 7 (the collision's +0x20)
    float m_top = 0.0f;                             // [0x0075B2D4]
};

}  // namespace oj::scene
