#pragma once
// SMOKE, FRAMEBUFFER and WHEELSPIN: billboards that rise, cool and drift.

#include "Audio/Voices.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Tracks/SunMap.h"
#include "Game/Scene/Particles/Emitter.h"
#include "Game/Scene/Particles/ParticleSystem.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/RenderDevice.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace oj::scene {

// SMOKE, FRAMEBUFFER and WHEELSPIN (0x0050B1D0, 0x178 bytes): billboards that rise, cool, follow the emitter and
// drift, drawn as camera-facing quads of 4 vertices (24 bytes: place, colour, u v) from a buffer of its own.
class SmokeEmitter final : public ParticleEmitter {
public:
    SmokeEmitter(ParticleSystem& system, const EffectDef& def);
    ~SmokeEmitter() override;
    // 0x0050BCE0. With [0x00719318] set (static 1) the stock game copies the frame's camera into one of two buffers
    // (+0x11C + 0x2C x ((+0x174 - 1) & 1)), hands the simulation (0x0050BCB0 -> 0x0050B8E0, which turns +0x174 over)
    // to the worker thread when it runs ([0x0071E5B4], 0x005A0A00; else at once) and draws the quads of the buffer of
    // +0x174 & 1: the frame's smoke when the simulation is done by then (no worker, or a quick one), the previous
    // frame's when not, as the threads' timing falls. Here the simulation is done at once: the frame's smoke.
    bool frame(render::Camera& camera) override;

private:
    struct Vertex {
        float x, y, z;
        uint32_t colour;
        float u, v;
    };
    // 0x0050B4F0
    bool step(size_t index, Vec4& place, float& size, Vec4& colour, float dt);
    // 0x0050B8E0
    void simulate(const render::Camera& camera, float dt);

    std::vector<Vertex> m_vertices;
    render::BufferId m_buffer = 0;           // +0x118
};

}  // namespace oj::scene
