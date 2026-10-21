#pragma once
// The light of the sun on a scene object that moves: a car's chassis, wheels and parts, a barrel, a bin (the scene
// objects' steps, docs/spec/21-scene-objects.md, "The light of the sun"): the step keeps its node's motion (0x0041B270)
// and its light (+0x48) from the route's sun map, and gives the light to its node (+0xC4) unless the race is at
// night.

#include "Core/Tracks/SceneObjects.h"
#include "Core/Tracks/SunMap.h"
#include "Game/Scene/Projector.h"
#include "Render/Model.h"
#include "Audio/Voices.h"

namespace oj::scene {

struct LightStep {
    // Slot 1 of the step's class: the chassis (0x00419980: four samples around it, none in a tunnel, eased), a car's
    // part (0x004191B0: the chassis's), a wheel, a barrel or a bin (0x0041AF00: a sample where it is, eased).
    enum class Kind { Chassis, Part, Eased };

    Kind kind = Kind::Eased;
    render::Node* node = nullptr;           // +0x08
    const LightStep* chassis = nullptr;     // +0xD0: a part's
    float light = 1.0f;                     // +0x48: -1 for the chassis and the parts until the first sample
    float sample = 0.0f;                    // +0xC8
    Vec4 sampledAt{};                       // +0xB8: the place of the last sample
    Vec4 place{};                           // +0x10: the node's place in the world at the last step
    Vec4 velocity{};                        // +0x28
    Vec4 change{};                          // +0x38: the change of the velocity in the last step
    float speed = 0.0f;                     // +0x24: the velocity's length a step behind
    float sideways = 0.0f;                  // +0x20: the change of the velocity across the ground (x and z)
    bool night = false;                     // +0x4C: the atmosphere's +0x170 when it started
    bool barrel = false;                    // a barrel's step (its class's 0x00418D70)
    bool barrelHit = false;                 // +0xCC: a barrel hit past 300 once
    bool started = false;                   // +0xAC
    Projector* shadow = nullptr;            // +0xB4: laid again each step after the light
    Mat34 own = identity34();               // +0x7C: the node's matrix when it started

    // The chassis's and a part's object (0x004191C0; Kind::Chassis and Kind::Part): how it wobbles, its damage zone
    // (+0xCC, given by the model's slot 6 0x004191A0: the zone of the table's part, none for the chassis), the
    // node's parent when it was made (+0xD4), the model's byte of parts knocked off (+0xF0 the model, its +0x170;
    // none for the chassis) and its body (+0x58).
    CarPartObject part;
    const DamageZone* zone = nullptr;
    render::Node* parent = nullptr;
    uint8_t* modelParts = nullptr;
    std::optional<PartPhysics> physics;
    // A barrel's or a bin's sound (its class's +0x14 "trashcan", made once by 0x0041ABF0 with the 3D world: on its
    // node, rolloff 0.0001, not looping, no doppler; +0x54) and its frequency's base (+0x50: 20000, a barrel's 15000,
    // a bin's 30000).
    audio::Instance* sound = nullptr;
    float baseFrequency = 20000.0f;

    LightStep(Kind k, render::Node* n) : kind(k), node(n), light(k == Kind::Eased ? 1.0f : -1.0f) {}

    // 0x0041ABF0, once: the node's matrix kept as its own (+0x7C), its place, the light, night as the atmosphere has it.
    void start(const SunMap* map, const SceneObjects& objects, bool atNight);
    // The step (slot 4). The chassis and a part (0x00419780): while not attached (+0xD8 0, never) the node shown again
    // (+0xC0 & ~4); else its wobble (CarPartObject::wobble, with axes). Then, for every kind, 0x0041B270: the node's
    // velocity over the step's time (none with no time), the light, the light to the node unless at night; a node
    // whose sideways change of velocity passes 200 is passed to its mesh (+0xC0 | 8) and its sound (if any) plays from
    // its start at that change (in [0, 1]: 1) and a frequency of rand() x 0.061037 + its base (the game's rand());
    // last the turn (0x0041B050) of an object with axes: the node's matrix its own times the turn.
    void frame(const SunMap* map, const SceneObjects& objects, float dt);
    // 0x004196D0, the chassis's and a part's object going (0x00420F30 from 0x0048A700): the node takes its own matrix
    // back; one not attached is put back (0x00419730: under its parent again, attached, the node's +0xC0 bit 0x10 and
    // the model's bit of it cleared).
    void release();

private:
    void update(const SunMap* map, const SceneObjects& objects);
};

}  // namespace oj::scene
