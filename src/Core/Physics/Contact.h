#pragma once
// The contacts of the physics (docs/spec/16-physics.md, "Contacts"): a collider's touch with the world, the record
// a body keeps of it for a step, and the forces it gives.

#include "Core/Physics/Dynamics.h"
#include "Core/Base/Matrix.h"

#include <array>
#include <cstddef>

namespace oj {

class RigidBody;
struct CldZone;

// A touch the collision finds (the record passed to the body's [2]): in the model's frame, in centimetres.
struct Touch {
    int32_t id = 0;               // +0x00: the collider's ID
    const CldZone* zone = nullptr;  // +0x04
    float friction = 1.0f;        // +0x08: the zone's
    float bounce = 1.0f;          // +0x0C
    Vec4 point{};                 // +0x10
    Vec4 normal{};                // +0x20: of the surface
    float depth = 0.0f;           // +0x30
    RigidBody* other = nullptr;   // +0x34
};

// A contact of a body in a step (the pools' records: rigid 0x9C, spring 0x98, tyre 0xCC bytes; 0x004F9170).
struct Contact {
    const CldZone* zone = nullptr;    // +0x04
    float mu = 0.0f;                  // +0x08: Mu x the zone's friction
    float restitution = 0.0f;         // +0x0C
    Vec4 normal{};                    // +0x10: in the world
    Vec4 localNormal{};               // +0x20: in the body's frame
    Vec4 point{};                     // +0x30: in the world (metres)
    Vec4 arm{};                       // +0x40: from the centre of gravity, in the body's frame
    Vec4 velocity{};                  // +0x50: of the point against the other side, in the body's frame
    float normalSpeed = 0.0f;         // +0x60
    float compression = 0.0f;         // +0x64 (metres)
    RigidBody* other = nullptr;       // +0x68
    const ContactDef* def = nullptr;  // +0x6C
    float load = 0.0f;                // +0x70: the normal force
    Vec4 force{};                     // +0x74: along the normal, in the body's frame
    Vec4 friction{};                  // +0x84: the friction or the tyre's force
    float weight = 0.0f;              // +0x98: a rigid contact's share of the body's weight
    // A tyre's (+0x9C ..)
    Vec4 slipVelocity{};              // +0x9C
    float slip = 0.0f;                // +0xAC: the slip ratio
    float groundSpin = 0.0f;          // +0xB0: the speed the ground turns the tyre at
    float wheelSpin = 0.0f;           // +0xB4: set by the car before its forces
    float radius = 0.0f;              // +0xB8
    float steer = 0.0f;               // +0xBC
    float gravity = 0.0f;             // +0xC0: LocalGravity
    float gravityInverse = 0.0f;      // +0xC4
    float throttle = 0.0f;            // +0xC8

    // 0x004F9170 with the kind's (0x004F9420, 0x004F7790, 0x004F7C00): the contact of a touch.
    void begin(const Touch& t, const RigidBody& body, const ContactDef& d);

    // 0x004F9090: the point and the normal follow the body, the compression the point's way along the normal; then
    // the forces.
    void update(const RigidBody& body);

    // 0x004F8E50
    void relativeVelocity(const RigidBody& body);

    // [2]: the most the contact presses before a push (0x004F3900, 0x004F3910): MaxCompression.
    float maxCompression() const { return def->maxCompression; }

    void springForce();  // 0x004F77B0, a spring's and a tyre's [0]
    void rigidForce();   // 0x004F9440, a rigid contact's [0]
    void slide();        // 0x004F9360, [1] of rigid and spring contacts
    void tyreForce();    // 0x004F7C40, a tyre's [1]
};

// The pools the contacts of a world step come from (0x0076A270, 0x0076FDD8, 0x00775700), emptied before each step.
struct ContactPools {
    std::array<Contact, 150> rigid{};
    std::array<Contact, 150> spring{};
    std::array<Contact, 64> tyre{};
    size_t rigidUsed = 0;   // [0x0075F050]
    size_t springUsed = 0;  // [0x0075F054]
    size_t tyreUsed = 0;    // [0x0075F058]

    void empty() { rigidUsed = springUsed = tyreUsed = 0; }
};

}  // namespace oj
