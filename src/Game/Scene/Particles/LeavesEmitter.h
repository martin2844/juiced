#pragma once
// LEAVES: leaves blown about the camera.

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
#include <vector>

namespace oj::scene {

// LEAVES (0x00508B80 on the base 0x0050DD60, vtables 0x006F7440 and 0x006F7624, 0x16C bytes): NUM_POINTS leaves
// (0xB4 bytes each from +0xCC) within RANDOM_DRIFT x (the particle density x 0.4 + 1) of its place, falling, blown by
// the wind, tumbling in the air and lying flat on the road, drawn as quads of the leaves' tables from two vertex
// records (+0x104, +0x130: the step writes one while the other is drawn) with their material (MATERIAL_NAME, +0xC8).
// Its colours in the sun and in the shade are the game's ([0x007845C0] (the sun's on particles + COLOUR_START) x
// 0.75, [0x00790150] (the shade's + COLOUR_START) x 0.4), the last LEAVES emitter made setting them.
class LeavesEmitter final : public Emitter {
public:
    LeavesEmitter(ParticleSystem& system, const EffectDef& def);
    ~LeavesEmitter() override;
    // [1] 0x0050E2A0: [4].
    void sampleLight() override { clear(gameRandom()); }
    // [4] 0x0050E2B0: every leaf put anew (0x0050E6B0, rand() of the calling thread), then its bounds about its place
    // with the radius.
    void clear(CrtRandom& random);
    // [5] 0x00509890: nothing below the detail 0.1 or further than LOD_OFF_MUL x 6 (squared) across the ground from
    // the camera ([0x00725708] 1); else the step (0x005095D0) and the record it wrote drawn. It is never done.
    bool frame(render::Camera& camera) override;

private:
    // A leaf: a particle (its place, its life 0 or -1 when gone, its turn about a car's wake) and its own.
    struct Leaf {
        Particle p;                    // +0x00 .. +0x44
        float ground = 0.0f;           // +0x48: the road's height under it + 2
        Vec4 velocity{};               // +0x4C (w a value of the stock code's stack)
        Vec4 spin{};                   // +0x5C: its drift while in the air
        Vec4 colour{};                 // +0x6C
        Vec4 last{};                   // +0x8C: where its ground was last found
        float size = 0.0f;             // +0xAC
        float light = 0.0f;            // +0xB0: the sun map's value there (1 without one)
    };
    // Its bounds (0x30 bytes at +0xD4; 0x0050D040, 0x0050D0C0).
    struct Bounds {
        Vec4 middle{};
        float radius = 0.0f;
        Vec4 top{};
        Vec4 bottom{};
        float pad = 0.0f;
        void reset(const Vec4& point, float padding);
        void add(const Vec4& point);
    };
    struct Vertex {
        float x, y, z;
        uint32_t colour;
        float u, v;
    };
    // A vertex record (0x2C bytes): its leaves (+0x00) and its vertices (+0x24, NUM_POINTS x 4).
    struct Record {
        uint32_t count = 0;
        std::vector<Vertex> vertices;
    };
    // 0x0050E6B0
    void reset(Leaf& leaf, CrtRandom& random);
    // 0x00508F70
    void step(float dt, CrtRandom& random);
    // 0x005095D0 (the job of 0x00509870)
    void simulate(float dt);
    // 0x0050E3D0
    void corners(const Leaf& leaf, uint32_t index, Vec4& a, Vec4& b, Vec4& colour) const;
    bool groundUnder(Leaf& leaf) const;

    uint32_t m_points = 0;                   // +0x5C: NUM_POINTS
    float m_radius = 0.0f;                   // +0x60
    float m_size = 0.0f;                     // +0x64: START_SIZE
    float m_growth = 0.0f;                   // +0x68: GROWTH_RATE
    float m_rotation = 0.0f;                 // +0x6C: ROTATION_RATE (read by nothing found)
    Vec4 m_colourStart{};                    // +0x70
    Vec4 m_colourEnd{};                      // +0x80 (read by nothing found)
    Vec4 m_colourVary{};                     // +0x90
    float m_brightness = 0.0f;               // +0xA0: BRIGHTNESS_RANGE
    float m_resistance = 0.0f;               // +0xA4: WIND_RESISTANCE
    float m_drift = 0.0f;                    // +0xA8: RANDOM_DRIFT x 0.05
    float m_gravity = 0.0f;                  // +0xAC: GRAVITY
    float m_follow = 0.0f;                   // +0xB0: FOLLOW_FRAC (read by nothing found)
    float m_reach = 0.0f;                    // +0xB4: the radius squared
    render::Material* m_material = nullptr;  // +0xC8
    std::vector<Leaf> m_leaves;              // +0xCC
    Bounds m_bounds;                         // +0xD4
    std::array<Record, 2> m_records;         // +0x104, +0x130
    uint32_t m_written = 0;                  // +0x15C: the record drawn
    uint32_t m_vertexCount = 0;              // +0x164
    render::BufferId m_buffer = 0;           // +0x168
};

}  // namespace oj::scene
