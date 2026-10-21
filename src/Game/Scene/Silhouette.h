#pragma once
// A car's shadow drawn from its silhouette (the class 0x005491D0, 0x9348 bytes, vtable 0x006F9E68; docs/spec/12-rendering.md,
// "The silhouette shadows"): each frame the car's node and the nodes under it are drawn white into a target of
// 256 x 256 from the light, along the light, by an orthographic camera; the route's faces under the box the light
// sweeps take the target as a blurred shadow. The world lists it with the projected textures (+0x98) and draws it
// with them, with the shadows' setting above 1.

#include "Core/Formats/Cld.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/Renderer3d.h"

#include <functional>
#include <memory>
#include <vector>

namespace oj::render {
struct AtmosphereSet;
}

namespace oj::scene {

class Engine;

// What the world's lists draw (the base 0x006F9E50: +0x0C on, [5] the draw).
class LaidObject {
public:
    virtual ~LaidObject() = default;
    virtual void draw(render::Camera& camera) = 0;
    bool on = true;  // +0x0C: off, not drawn
};

class Silhouette final : public LaidObject {
public:
    // What draws the car into the target: its nodes at and below `node` with the renderer's material and pass kind.
    using DrawCar = std::function<void(render::Renderer3d&, render::Camera&)>;
    // The nearest lights of the world to a point (the world's [15] 0x0054C8E0 → 0x0054E850): the first and the
    // second, either null.
    using NearestLights = std::function<void(const Vec4& point, render::Node*& first, render::Node*& second)>;

    // 0x00531380 → 0x005491D0: of the light `type` (0 the sun of the atmosphere; 1 and 2 the first and the second of
    // the lights nearest the node); <projected_shadow> of Blurred Shadow (34, its offset 0.5 / 256, the target as its
    // texture) and <silhouette> of Constant Colour (2, white); the target and the orthographic camera (0x00548FD0,
    // near 0.01); the node's bounding sphere (0x00549500) as an offset from the node's place; a dynamic buffer of
    // 1000 vertices of 36 bytes.
    Silhouette(Engine& engine, render::Node& node, int type, DrawCar drawCar);
    ~Silhouette() override;

    // [1] 0x00549E90 each frame of the car (0x004890B0, `fade` the chassis's light over 0.7 within 1, or 1): the
    // light's direction, the target drawn, the faces under the box laid out with their u and v and clipped.
    void update(float fade, const render::AtmosphereSet* atmosphere, const NearestLights& nearest, const Cld& cld);
    // [5] 0x0054A9E0: with the shadows' setting above 1, vertices, a fade and seen (0x00552FD0), at most 999 of the
    // vertices as a triangle list with <projected_shadow>.
    void draw(render::Camera& camera) override;

    float radius() const { return m_radius; }

private:
    struct Vertex {
        float x, y, z;
        float r, g, b, a;
        float u, v;
    };

    // 0x00549500: the sphere of the node and the nodes under it.
    void sphere(render::Node& node);
    bool seen(render::Camera& camera);
    void gather(const Cld& cld, int32_t at, float minX, float minZ, float maxX, float maxZ);  // 0x0054AB30
    void take(const Cld& cld, const std::vector<uint32_t>& faces);                            // 0x00549A80
    static void clip(const std::vector<Vertex>& from, std::vector<Vertex>& to, float a, float b, float d);  // 0x00549D80

    Engine& m_engine;
    render::Node& m_node;                            // +0x14
    int m_type = 0;                                  // +0x9344
    DrawCar m_drawCar;
    Vec4 m_centre{};                                 // +0x18: from the node's place
    float m_radius = 0.0f;                           // +0x28
    float m_fade = 0.0f;                             // +0x2C
    std::unique_ptr<render::Material> m_shadow;      // +0x38
    std::unique_ptr<render::Material> m_silhouette;  // +0x3C
    render::GameTexture m_target;                    // +0x48
    render::Camera m_camera;                         // +0x933C
    render::BufferId m_buffer = 0;                   // +0x34
    render::DeclarationId m_declaration = 0;         // +0x44
    std::vector<Vertex> m_vertices;                  // +0x4C (count), +0x50
    std::vector<uint32_t> m_taken;                   // +0x8CF0 (count), +0x8CF4
    std::vector<uint8_t> m_marked;                   // the faces' bit 7 (the collision's +0x20)
};

}  // namespace oj::scene
