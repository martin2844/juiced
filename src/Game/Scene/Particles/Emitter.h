#pragma once
// A particle, the wind the particles drift in, and the emitters' base classes: an effect's emitter on a node and one of
// particles kept.

#include "Audio/Voices.h"
#include "Core/Base/Random.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Formats/Cld.h"
#include "Core/Tracks/Atmosphere.h"
#include "Core/Tracks/SunMap.h"
#include "Render/Camera.h"
#include "Render/Materials.h"

#include <cstdint>
#include <vector>

namespace oj::scene {

class Engine;
class ParticleSystem;

// 0x00509B70: the game's lagged generator (Core/Base/Random.h), which the dents of the cars' vertices seed too.
using ParticleRandom = LaggedRandom;

// A particle (0x48 bytes).
struct Particle {
    Vec4 place{};           // +0x00
    float age = -1.0f;      // +0x10: -1 when it is not alive
    float level = 0.0f;     // +0x14: its emitter's level when it was let out
    int32_t lit = 0;        // +0x18: its emitter's light over 0.5 then
    float swirl = 0.0f;     // +0x1C: the radius of its turn about a car's wake
    float phase = -1.0f;    // +0x20: the turn's angle, -1 before it is caught
    float frequency = 0.0f; // +0x24: turns a second
    Vec4 across{};          // +0x28
    Vec4 along{};           // +0x38
};

// The wind (0x00505740, 0x30 bytes): a still air with a sway, and the cars that push it (0x00505850, a source each of
// 0x24 bytes: its node, its place, its velocity).
class ParticleWind {
public:
    struct Source {
        render::Node* node = nullptr;  // +0x00
        Vec4 place{};                  // +0x04
        Vec4 velocity{};               // +0x14 (w a value of the stock code's stack)
    };

    // 0x00505850: the node's place taken at once.
    void addSource(render::Node& node);
    // 0x00506340: each source's velocity from its node's move over the step (a move over 10000 a second stops it).
    void step(float dt);
    // 0x00505CF0: the air's velocity at a particle: each source near it pushes it away (and, the particle not yet
    // caught, catches it in its wake, 0x005059B0); the sway of the time. Asked for a kick (no caller found asks), a
    // particle caught within 200 of the wake's line by a car faster than 2500 gets the wake's axis x the car's speed
    // x -0.95 x rand() / 32767 (the calling thread's).
    Vec4 at(Particle& p, const Vec4& emitterVelocity, float time, float* gain, float* speed,
            Vec4* kick = nullptr) const;

private:
    Vec4 m_still{};          // +0x08 .. +0x14
    float m_sway = 50.0f;    // +0x18
    std::vector<Source> m_sources;  // +0x1C
};

// An emitter (0x0050CE70).
class Emitter {
public:
    Emitter(ParticleSystem& system, const EffectDef& d);
    // 0x0050CFF0: its sound released (+0x4C).
    virtual ~Emitter();
    Emitter(const Emitter&) = delete;
    Emitter& operator=(const Emitter&) = delete;

    // [1]: the light where it is (none for the classes without particles of their own).
    virtual void sampleLight() {}
    // [2] 0x0050CFD0: and the sound follows the node.
    void setNode(render::Node* n);
    // [3] 0x0050D1D0: its level; above the trigger at least its MIN_AMOUNT. With a sound: over the sound's trigger
    // level the volume (level / mul - trigger) / (1 - trigger) + its minimum, in [0, 1], times SOUND_VOLUME (+0x54); a
    // loop is played when it does not sound (at any volume) and takes it.
    virtual void setLevel(float value, float mul = 1.0f);
    // 0x0050CE70's sound, with the 3D world (the effects manager's +0x80) and a loop: the first SOUND_EFFECT's set
    // plus the value drawn as it was made % its count (0x005B99B0: group 0, then -1), rolloff, the node, doppler,
    // looping, minimum distance, stopped until a level plays it.
    void makeSound(audio::World& world, int32_t draw);
    // [6] (+0x18) 0x0050D2C0: with a sound played once and the 3D world: of the entries the one of chop((n - 1) x the
    // level) (0 for one), its set plus rand() % its count, played at the place (0x005B9A80, the owner +0x4C) with the
    // rolloff, doppler, not looping, the volume of now (+0x54), group 0 and the minimum distance. Its sound (+0x4C).
    audio::Instance* oneShot(const Vec4& place, CrtRandom& random);
    // [8] (+0x20): the sound's frequency (whole Hz), a real one only for SPARKS, SMOKE and the particles' base.
    virtual void setFrequency(float hz) { (void)hz; }
    audio::Instance* sound = nullptr;        // +0x4C
    float soundVolume = 0.0f;                // +0x50: SOUND_VOLUME
    float soundNow = 0.0f;                   // +0x54
    audio::World* soundWorld = nullptr;      // the effects manager's +0x80
    // [5] (+0x14): a frame of it, its particles drawn; false when it is to be taken out of the list.
    virtual bool frame(render::Camera& camera) = 0;

    const EffectDef& def;                    // +0x40
    render::Node* node = nullptr;            // +0x04
    Vec4 local{};                            // +0x0C: its place (under its node)
    Vec4 origin{};                           // +0x1C: an owner's place
    Vec4 velocity{};                         // +0x2C
    float level = 0.0f;                      // +0x3C
    float ground = 0.0f;                     // +0x58: the owner's lowest touch of the route

    // 0x0048F9E0: its place in the world.
    Vec4 worldPlace() const { return world(); }

protected:
    // 0x0048F9E0: its place in the world.
    Vec4 world() const;

    ParticleSystem& m_system;
};

// The particles of an emitter (0x0050F490).
class ParticleEmitter : public Emitter {
public:
    ParticleEmitter(ParticleSystem& system, const EffectDef& def);

    // [1] 0x0050F8C0: the light where it is, from the sun map.
    void sampleLight() override;
    // [4] 0x0050F7D0: every particle gone.
    void clear();
    void setFrequency(float hz) override;  // 0x00506AE0 (its vtable 0x006F7688; SMOKE's 0x006F74F8 the same)

    bool active = true;                      // +0xCC
    bool dying = false;                      // +0xB4

protected:
    // 0x0050F920: the axes of the quads (the manager's +0x110, +0x120, +0x130); false when it is done; with time, its
    // light sampled again once it moved 300 across the ground (with a node), its box at its place, and particles let
    // out while the level reaches the trigger (12 a frame at most here, [10]).
    bool prepare(Vec4& axis1, Vec4& axis2, Vec4* axis3);
    // 0x0050F800
    void letOut(Particle& p, float spread);
    virtual int32_t burst() const { return 12; }

    uint32_t m_points = 0;                   // +0x5C
    float m_life = 0.0f;                     // +0x64
    Vec4 m_sunColour{};                      // +0x68
    Vec4 m_shadeColour{};                    // +0x78
    int32_t m_alive = 0;                     // +0xAC
    float m_light = 0.0f;                    // +0xB8
    Vec4 m_sampledAt{};                      // +0xBC
    render::Material* m_material = nullptr;  // +0xD0
    std::vector<Particle> m_particles;       // +0xD4
    uint32_t m_next = 0;                     // +0xD8
    float m_timer = 0.0f;                    // +0xDC
};

}  // namespace oj::scene
