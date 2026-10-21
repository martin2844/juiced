#pragma once
// SPARKS: streaks of a burst, drawn anew each frame.

#include "Audio/Voices.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Tracks/SunMap.h"
#include "Game/Scene/Particles/Emitter.h"
#include "Game/Scene/Particles/ParticleSystem.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/RenderDevice.h"

#include <cstdint>
#include <vector>

namespace oj::scene {

// SPARKS (0x00509C20 on the base 0x00506910, 0xAC bytes): no particles kept; once its level passes the trigger, for
// 500 x AGE_RANGE seconds (more each time it is raised), streaks drawn anew each frame from the particles' generator
// seeded with 0, each along its own speed and spread over the burst's time.
class SparksEmitter final : public Emitter {
public:
    SparksEmitter(ParticleSystem& system, const EffectDef& def);
    ~SparksEmitter() override;
    // [3] 0x00506A00: the level, the spray against the velocity (its direction x -0.5 x the level); raised, the burst
    // lasts on from now.
    void setLevel(float value, float mul = 1.0f) override;
    void setFrequency(float hz) override;  // 0x00506AE0
    bool frame(render::Camera& camera) override;
    // The colours its streaks go from and to (+0x7C, +0x8C), as made from COLOUR_START and COLOUR_END.
    void setColours(const Vec4& start, const Vec4& end) {
        m_start = start;
        m_endColour = end;
    }
    bool bursting() const { return m_burst; }

private:
    struct Vertex {
        float x, y, z;
        uint32_t colour;
        float u, v;
    };
    // 0x00506B40
    bool prepare(const Vec4& place, render::Camera& camera);

    uint32_t m_points = 0;                   // +0x5C
    Vec4 m_spray{};                          // +0x60
    float m_time = 0.0f;                     // +0x70
    float m_end = 0.0f;                      // +0x74
    bool m_burst = false;                    // +0x78
    Vec4 m_start{};                          // +0x7C
    Vec4 m_endColour{};                      // +0x8C
    render::Material* m_material = nullptr;  // +0x9C
    std::vector<Vertex> m_vertices;
    render::BufferId m_buffer = 0;           // +0xA4
};

}  // namespace oj::scene
