#pragma once
// SMEAR: the screen's smears.

#include "Audio/Voices.h"
#include "Core/Base/Random.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Tracks/Atmosphere.h"
#include "Core/Tracks/SunMap.h"
#include "Game/Scene/Particles/Emitter.h"
#include "Game/Scene/Particles/ParticleSystem.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace oj::scene {

// SMEAR (0x0050A400 on the base 0x0050EA80, vtable 0x006F7494): raindrops on the screen, drawn after the view's post
// effects in its pixels (z 10 and 5) with a TexShape2D material (class 0x10002, +0x5E90) and the texture rainsmear
// (+0x5C). Each blob (0x98 bytes from +0x60) slides by the camera's speed away from the screen's point 3/4 down the
// middle, sways and falls, fades over its life and leaves a trail of up to six pairs of points.
class SmearEmitter final : public Emitter {
public:
    SmearEmitter(ParticleSystem& system, const EffectDef& def, CrtRandom& random);
    ~SmearEmitter() override;
    // [4] 0x0050ECC0: every blob empty with a life drawn by rand() and white; the camera's last place the origin.
    void clear(CrtRandom& random);
    // [10] 0x005BE6C0: how much shows (+0x80), not held.
    void setAmount(float value) { m_amount = value; }
    // [5] 0x0050A4A0 from the manager's draw of its SMEAR list (0x005041F0): the blobs moved (0x0050ED60), their
    // trails and the blobs drawn.
    bool frame(render::Camera& camera) override;

private:
    struct Blob {
        float life = 0.0f;                 // +0x10
        float size = 0.0f;                 // +0x14
        float x = 0.0f, y = 0.0f;          // +0x18
        float anchorX = 0.0f, anchorY = 0.0f;  // +0x20: where the trail last took a point
        uint32_t colour = 0;               // +0x28
        uint32_t first = 0;                // +0x2C: the trail's oldest pair
        uint32_t next = 0;                 // +0x30: the pair written next
        uint32_t count = 0;                // +0x34
        std::array<std::array<float, 4>, 6> trail{};  // +0x38: x1, y1, x2, y2
    };
    struct Vertex {
        float x, y, z;
        uint32_t colour;
        float u, v;
    };
    // 0x0050ED60
    void simulate(render::Camera& camera);

    uint32_t m_points = 0;                   // +0x64
    float m_size = 0.0f;                     // +0x68: START_SIZE
    float m_growth = 0.0f;                   // +0x6C: GROWTH_RATE
    float m_spread = 0.0f;                   // +0x70: EMIT_SPEED
    float m_gravity = 0.0f;                  // +0x74: GRAVITY
    float m_drift = 0.0f;                    // +0x78: RANDOM_DRIFT
    float m_shown = 1.0f;                    // +0x7C
    float m_amount = 1.0f;                   // +0x80
    Vec4 m_place{};                          // +0x88
    Vec4 m_velocity{};                       // +0x98: the camera's move
    Vec4 m_way{};                            // +0xA8: the move made unit
    Vec4 m_last{};                           // +0xB8
    float m_across = 0.0f;                   // +0xC8: the move across the view last frame
    float m_up = 0.0f;                       // +0xCC: and up it
    const render::GameTexture* m_texture = nullptr;  // +0x5C
    std::vector<Blob> m_blobs;               // +0x60
    std::unique_ptr<render::Material> m_material;  // +0x5E90
    uint32_t m_strip = 0;                    // +0x5E9C
    std::vector<Vertex> m_vertices;
    render::BufferId m_buffer = 0;           // +0x5E98
};

}  // namespace oj::scene
