#pragma once
// A rigid body of the physics (the dynamic object, vtable 0x006F6738; docs/spec/16-physics.md): the state the
// integrator steps, the values the body derives from it, and the forces on it. Distances are metres, the world's
// centimetres divided by 100.

#include "Core/Physics/Contact.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Base/Matrix.h"

#include <vector>

namespace oj {

class PhysicsWorld;
class RigidBody;

// What finds a body's touches with the world in its step (0x0053EC30).
class Collision {
public:
    virtual ~Collision() = default;
    virtual void collide(RigidBody& body) = 0;
};

// The integrator of a step (the body's +0x100; every body has 2).
enum class Integrator : int { Euler = 0, Midpoint = 1, RungeKutta4 = 2 };

class RigidBody {
public:
    // 0x004F2060: at rest, with the model's origin at a point of the world (metres). The orientation is that of the
    // angles +0x218 (0x004F35B0), all 0, but the rotation stays the identity until the first state is set.
    RigidBody(const BodyDef& def, const Vec4& origin);
    virtual ~RigidBody() = default;

    // The layout of the state (the index table +0x104 .. +0x134): the position, the momentum, the orientation (w, x,
    // y, z), the angular momentum. A derived body appends its own values.
    static constexpr size_t kPosition = 0, kMomentum = 3, kOrientation = 6, kAngularMomentum = 10, kBodyState = 13;

    // [3] 0x004F2810: one step of dt seconds: the touches with the world, the rigid contacts' shares of the weight,
    // the impacts, the integration, the push out of the deepest contact; then the sleep (a body with Sleep values
    // still for 3 s leaves the world's active list) and, unless it went to sleep, the slow body's brake: under
    // 0.3 m/s in its frame its speed loses 2 dt (Stock problem: below 2 dt it turns to the other way; the fixed
    // build's stops at 0).
    virtual void step(float dt);

    // 0x004F2DC0 / 0x004F2EC0: a new momentum / angular momentum and the velocities that follow, also in the state.
    void setMomentum(const Vec4& p);
    void setAngularMomentum(const Vec4& l);

    // 0x004F2F80 / 0x004F3080: a velocity / an angular velocity in the body's frame and the momenta that follow.
    void setLocalVelocity(const Vec4& v);
    void setLocalAngularVelocity(const Vec4& w);

    // 0x00429D40: the centre at a point of the world (metres); the node follows.
    void setPosition(const Vec4& p);

    // 0x004F2CD0: an orientation (made unit), the rotation, its inverse and the angles that follow; with `node` the
    // node too (0x004F01E0).
    void setOrientation(const Quat& q, bool node);
    // 0x004F3150: the orientation of angles about x, y and z; the rotation follows (the world inertia with the next
    // state).
    void setAngles(const Vec4& a);

    // 0x004F0F90 against the world: an impact at a point while it closes along the normal (world frame), bouncing
    // by the restitution more from 2 to 5 m/s; the turn it gives is cut to 0.1 (0.05 over 6 m/s). The linear and
    // angular parts are added to the impulse and the angular momentum. False when the point is not closing.
    bool impulse(float normalSpeed, const Vec4& point, const Vec4& normal, const Vec4& linear, const Vec4& angular,
                 float restitution);

    // [2] 0x004F2710: a touch with the world becomes a rigid or a spring contact of the step (at most 35 of each).
    virtual void addTouch(const Touch& t);

    // 0x004F3220: the contact definition of a collider's touch: the first of its ID whose clipping plane, if it has
    // one, the point lies beyond.
    const ContactDef* contactDefOf(int32_t id, const Vec4& point) const;

    // [8] 0x004F0B90: integrates the state over dt; then the change of the local velocity per second and the clearing
    // of the external force and torque.
    void integrate(Integrator method, float dt);

    // [5] 0x004F0320: the body takes a state (its quaternion normalised, also in the vector) and derives the
    // matrices, the velocities and the accelerations from it.
    virtual void setState(std::vector<float>& s);

    // [6] 0x004F0670: the derivative of the state set last, the forces summed anew.
    virtual void derivative(std::vector<float>& out, float dt);

    // [7] 0x004F0800: the forces in the body's frame: the external ones, the gravity, the contacts, the airfoils and
    // the drag.
    virtual void forces(float dt);

    // [9] 0x004F1E10: holds the momentum and the angular velocity of a state within their limits.
    virtual void constrain(std::vector<float>& s, float dt);

    // 0x004F0270: a force at a point of the body's frame; the torque counts the force times the weight.
    void addForceAt(const Vec4& point, const Vec4& force, float weight);

    // 0x004F2C10 (1): the model's origin moved to a point of the world (metres); the body follows with its centre,
    // the node with the body.
    void placeAt(const Vec4& origin);

    // 0x00407B90: the model's origin in the world (metres): the position less the rotated centre of gravity.
    Vec4 origin() const;

    // 0x004F01E0: the model's matrix in the world (centimetres): the orientation's rotation, the origin x 100.
    Mat34 modelMatrix() const;

    // [4] 0x004F2C00: the model's node takes the body's matrix (after every body of the world stepped, and when the
    // body is placed); other bodies collide with the node. A fixed body's node stays.
    void syncNode() {
        if (!fixed) node = modelMatrix();
    }

    // 0x004F0F90 with another body: an impact at a point while the two close along the normal (the world frame),
    // bouncing by the restitution more from 2 to 5 m/s; the impulse by the two bodies' masses at the point, its
    // opposite to the other body, the linear part added to this one and taken from the other, a lift of 0.15 of it
    // over 10000 (4500 over 30000) to both; the turn of each cut to 0.1 (0.05 over 6 m/s), the angular part added to
    // this one and taken from the other. False when the point is not closing.
    bool impulseWith(RigidBody& other, float normalSpeed, const Vec4& point, const Vec4& normal, const Vec4& linear,
                     const Vec4& angular, float restitution);

    const BodyDef& def() const { return m_def; }

    // 0x005371A0: the sphere around the colliders, in centimetres of the model, once: the middle of their box and
    // the farthest of their points from it.
    void computeBounds();

    Integrator integrator = Integrator::RungeKutta4;  // +0x100
    float inverseMass = 0.0f;                          // +0xCC
    Mat34 inverseInertia{};                            // +0xD0: of the body's frame
    Vec4 position{};                                   // +0x138: of the centre of gravity
    Quat orientation{1.0f, 0.0f, 0.0f, 0.0f};          // +0x148
    Vec4 momentum{};                                   // +0x158
    Vec4 angularMomentum{};                            // +0x168
    Vec4 velocity{};                                   // +0x178
    Vec4 angularVelocity{};                            // +0x188
    Vec4 localVelocity{};                              // +0x198: in the body's frame
    Vec4 localAngularVelocity{};                       // +0x1A8
    Mat34 rotation = identity34();                     // +0x1B8
    Mat34 inverseRotation = identity34();              // +0x1E8
    Vec4 angles{};                                     // +0x218: -asin, atan2, atan2 of the inverse rotation
    Mat34 inverseWorldInertia{};                       // +0x228
    Vec4 force{};                                      // +0x258: the sum, in the body's frame
    Vec4 torque{};                                     // +0x268
    Vec4 externalForce{};                              // +0x278: added from outside until the next integration
    Vec4 externalTorque{};                             // +0x288
    Vec4 acceleration{};                               // +0x298: force / mass in the world
    Vec4 localAcceleration{};                          // +0x2A8
    Vec4 localVelocityRate{};                          // +0x2B8: the change of the local velocity per second
    Vec4 displacement{};                               // +0x2C8: of the last step
    bool canSleep = false;                             // +0x2E8: Sleep with a non-zero first number
    float sleepTimer = 0.0f;                           // +0x2EC
    float dragFactor = 1.0f;                           // +0x2F0
    std::vector<Contact*> contacts;                    // +0x2F4: the step's
    std::vector<float> state;                          // +0x304

    Vec4 boundsCentre{};               // +0x10
    float boundsRadius = 0.0f;         // +0x20
    bool boundsKnown = false;          // +0x30
    Mat34 node = identity34();         // the model's node in the world (centimetres) at the last sync
    PhysicsWorld* world = nullptr;     // +0x0C: the world it is in
    int32_t cell = -1;                 // +0x68: of the world's grid
    RigidBody* cellNext = nullptr;     // +0x6C: in the cell's list
    RigidBody* cellPrevious = nullptr;  // +0x70
    bool active = false;               // +0x74: on the world's active list; a primary body is from its making
    // A collision object (vtable 0x006F9CA8, 0x00536F90): its step finds its touches with the world and the bodies
    // near it and drops them ([2] 0x005A10E0); its node, set where it was made, does not follow it.
    bool fixed = false;
    bool collides = true;              // +0x78: with other bodies
    // +0x7C, +0x80: a body named "wheel" without Sleep values (or before it sleeps) standing on its rim (its x axis
    // within 0.8 of level) or lying. Against faces and walls (0x0053B8D0, 0x0053D0C0) +0x7C lets the clipping
    // planes, the sphere colliders and the line colliders test, +0x80 the mesh collider.
    bool standing = true;
    bool lying = true;
    std::vector<RigidBody*> nearBodies;   // +0xB8: the bodies the world found near (0x0053EE90), MaxCollidees at most
    std::vector<float> nearDistances;     // +0xBC: their squared distances in x and z
    ContactPools* pools = nullptr;     // the world's
    Collision* collision = nullptr;    // the world's
    size_t rigidFirst = 0;             // +0x2D8: in the pool
    size_t rigidCount = 0;             // +0x2DC
    size_t springFirst = 0;            // +0x2E0
    size_t springCount = 0;            // +0x2E4

private:
    // 0x004F1770: each contact pressed past MaxCompression with a restitution of 0 or more takes an impact (those with
    // other bodies first, shared with them; one moving apart under 1 m/s by its excess, at most 0.3, bouncing fully);
    // a rigid one also its friction and a turn against the spin about its normal.
    void resolveImpacts(float dt);

    // 0x004F1C70: the body moves out along the normal of the contact pressed deepest past MaxCompression, by half.
    void pushOut();

    const BodyDef& m_def;
    std::vector<float> m_rate, m_k1, m_k2, m_k3, m_next;  // the integrator's buffers 0x007A0B98 .. 0x007A0D98
};

}  // namespace oj
