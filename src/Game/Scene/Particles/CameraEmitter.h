#pragma once
// The emitters about the camera: the ground rush, the rain and its splashes.

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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace oj::scene {

// The emitters a camera carries (0x0050D7F0, vtable 0x006F75F4; RAIN's, SPLASH's and GROUNDRUSH's base): drawn only
// in their camera's frame, their points within RANDOM_DRIFT of it.
class CameraEmitter : public Emitter {
public:
    CameraEmitter(ParticleSystem& system, const EffectDef& def);
    // [10] 0x0050D970: the camera followed (+0x100) and the caller's value (+0x74); +0xA0 the Night of the camera's
    // world's atmosphere.
    void attach(render::Camera* camera, int32_t value, bool night);
    // [11] 0x005088D0: how much shows (+0xA8), held within 0 .. 1.
    void setAmount(float value);
    // GROUNDRUSH's [5] (0x0042E470): nothing.
    bool frame(render::Camera& camera) override;

protected:
    struct Vertex {
        float x, y, z;
        uint32_t colour;
        float u, v;
    };
    // [5]'s start (0x0050DA40): false for another camera; the camera's place (+0xB0), how much shows times the
    // renderer's +0x188 (+0xA4), its move since the last frame (+0xC0) and whether that move is over 1000 across
    // (+0xAC).
    bool prepare(render::Camera& camera);
    // 0x0050DB50: point `index` put on the road near the camera (+0x104).
    Vec4 placeOnGround(size_t index);
    // The colour of the sun on particles (+0xA0 of the manager) with an alpha, each channel cut toward zero.
    uint32_t sunColour(uint32_t alpha) const;

    uint32_t m_points = 0;                   // +0x5C: NUM_POINTS
    float m_drift = 0.0f;                    // +0x60: RANDOM_DRIFT
    float m_reach = 0.0f;                    // +0x64: RANDOM_DRIFT² x 2
    float m_size = 0.0f;                     // +0x68: START_SIZE
    float m_growth = 0.0f;                   // +0x6C: GROWTH_RATE
    float m_speed = 0.0f;                    // +0x70: FOLLOW_FADE
    int32_t m_value = 0;                     // +0x74
    float m_width = 0.0f;                    // +0x78: BOUNCINESS without its sign
    Vec4 m_colourStart{};                    // +0x7C
    Vec4 m_colourVary{};                     // +0x8C
    bool m_flipped = false;                  // +0x9C: BOUNCINESS below 0
    bool m_night = false;                    // +0xA0
    float m_shown = 1.0f;                    // +0xA4
    float m_amount = 1.0f;                   // +0xA8
    bool m_jumped = false;                   // +0xAC
    Vec4 m_place{};                          // +0xB0
    Vec4 m_move{};                           // +0xC0
    Vec4 m_last{-3.4028235e38f, -3.4028235e38f, -3.4028235e38f, 0.0f};  // +0xD0
    render::Camera* m_camera = nullptr;      // +0x100
    std::vector<Vec4> m_spots;               // +0x104
    render::Material* m_material = nullptr;  // +0x2E04
    int32_t m_textures = 1;                  // +0x2E08: NUM_TEXTURES
};

// GROUNDRUSH (0x00508920, vtable 0x006F7410): the base and no more.
class GroundRushEmitter final : public CameraEmitter {
public:
    using CameraEmitter::CameraEmitter;
};

// RAIN (0x0050BE60, vtable 0x006F7524): NUM_POINTS x how much shows streaks a frame, each at a point drawn anew within
// RANDOM_DRIFT x 1.5 of the place RANDOM_DRIFT ahead of the camera, START_SIZE up and the camera's move long,
// BOUNCINESS wide.
class RainEmitter final : public CameraEmitter {
public:
    RainEmitter(ParticleSystem& system, const EffectDef& def);
    ~RainEmitter() override;
    bool frame(render::Camera& camera) override;  // 0x0050BFD0

private:
    uint32_t m_colour = 0;                   // +0x2E10
    std::vector<Vertex> m_vertices;
    render::BufferId m_buffer = 0;           // +0x2E14
};

// SPLASH (0x0050A890, vtable 0x006F74C0): splashes on the road around the camera, each playing the frames of its
// texture (NUM_TEXTURES in rows of four) at FOLLOW_FADE a frame and put somewhere else when its frames are done or the
// camera jumps; drawn as one triangle strip.
class SplashEmitter final : public CameraEmitter {
public:
    SplashEmitter(ParticleSystem& system, const EffectDef& def, CrtRandom& random);
    ~SplashEmitter() override;
    bool frame(render::Camera& camera) override;  // 0x0050AB10

private:
    uint32_t m_strip = 0;                    // +0x2E0C: the strip's vertices
    float m_across = 1.0f;                   // +0x2E10: a frame's width in the texture
    float m_down = 1.0f;                     // +0x2E14: its height
    std::vector<float> m_frames;             // +0x2E18: each splash's frame
    uint32_t m_colour = 0;                   // +0x2E1C
    std::vector<Vertex> m_vertices;
    render::BufferId m_buffer = 0;           // +0x2E20
};

}  // namespace oj::scene
