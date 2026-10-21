#include "Core/Physics/RigidBody.h"

#include "Core/Formats/Ocd.h"
#include "Core/Physics/PhysicsWorld.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace oj {

namespace {

// Below lo gives lo, above hi gives hi; a NaN stays (the stock code's two comparisons).
float clampKeep(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// 0x004F35B0: the orientation of angles about x, y and z: Qz (Qy Qx), each from the sine and cosine of its half angle
// rounded to floats (0x00407960, 0x004078D0).
Quat orientationOf(const Vec4& angles) {
    auto about = [](float angle, size_t axis) {
        const auto half = static_cast<double>(angle * 0.5f);
        Quat q{static_cast<float>(std::cos(half)), 0.0f, 0.0f, 0.0f};
        q[axis] = static_cast<float>(std::sin(half));
        return q;
    };
    return product(about(angles[2], 3), product(about(angles[1], 2), about(angles[0], 1)));
}

float inverseOr0(float v) {
    return v != 0.0f ? 1.0f / v : 0.0f;
}

// 0x004F3830
float ramp(float a, float x, float b) {
    if (x < a) return (a - a) / (b - a);
    if (x > b) return (b - a) / (b - a);
    return (x - a) / (b - a);
}

}  // namespace

RigidBody::RigidBody(const BodyDef& def, const Vec4& origin) : m_def(def) {
    state.assign(kBodyState, 0.0f);
    active = def.collision.primary;
    canSleep = def.sleepSet && def.sleep;
    orientation = orientationOf(angles);
    normalize(orientation);
    for (size_t i = 0; i < 4; ++i) state[kOrientation + i] = orientation[i];
    placeAt(origin);
    // 0x004F00C0: the inverse mass and the inverse inertia of the body's frame (Inertia is per kilogram).
    inverseMass = inverseOr0(def.mass);
    inverseInertia = {};
    inverseInertia[0] = inverseOr0(def.inertia[0] * def.mass);
    inverseInertia[5] = inverseOr0(def.inertia[1] * def.mass);
    inverseInertia[10] = inverseOr0(def.mass * def.inertia[2]);
    inverseWorldInertia = multiply(multiply(rotation, inverseInertia), transposeRotation(rotation));
}

void RigidBody::placeAt(const Vec4& origin) {
    const Vec4 c = transformPoint(rotation, m_def.centreOfGravity);
    position = {origin[0] + c[0], origin[1] + c[1], origin[2] + c[2], origin[3]};
    syncNode();
    for (size_t i = 0; i < 3; ++i) state[kPosition + i] = position[i];
}

Vec4 RigidBody::origin() const {
    const Vec4 c = transformPoint(rotation, m_def.centreOfGravity);
    return {position[0] - c[0], position[1] - c[1], position[2] - c[2], position[3]};
}

Mat34 RigidBody::modelMatrix() const {
    const Vec4 o = origin();
    Mat34 m = rotationOf(orientation);
    m[3] = o[0] * 100.0f;
    m[7] = o[1] * 100.0f;
    m[11] = o[2] * 100.0f;
    return m;
}

void RigidBody::setState(std::vector<float>& s) {
    for (size_t i = 0; i < 3; ++i) {
        position[i] = s[kPosition + i];
        momentum[i] = s[kMomentum + i];
        angularMomentum[i] = s[kAngularMomentum + i];
    }
    for (size_t i = 0; i < 4; ++i) orientation[i] = s[kOrientation + i];
    normalize(orientation);
    for (size_t i = 0; i < 4; ++i) s[kOrientation + i] = orientation[i];

    rotation = rotationOf(orientation);
    inverseRotation = inverseRigid(rotation);
    // The angles come from the x87 at full precision (the runtime's asin, fpatan), rounded once.
    angles[0] = -static_cast<float>(std::asin(static_cast<double>(inverseRotation[9])));
    angles[1] = static_cast<float>(
        std::atan2(static_cast<double>(inverseRotation[8]), static_cast<double>(inverseRotation[10])));
    angles[2] = static_cast<float>(
        std::atan2(static_cast<double>(inverseRotation[1]), static_cast<double>(inverseRotation[5])));
    inverseWorldInertia = multiply(multiply(rotation, inverseInertia), transposeRotation(rotation));

    velocity = {momentum[0] * inverseMass, momentum[1] * inverseMass, momentum[2] * inverseMass, momentum[3]};
    localVelocity = transformPoint(inverseRotation, velocity);
    angularVelocity = transformPoint(inverseWorldInertia, angularMomentum);
    localAngularVelocity = transformPoint(inverseRotation, angularVelocity);
    localAcceleration = {force[0] * inverseMass, force[1] * inverseMass, force[2] * inverseMass, force[3]};
    acceleration = transformPoint(rotation, localAcceleration);
}

void RigidBody::derivative(std::vector<float>& out, float dt) {
    for (size_t i = 0; i < 3; ++i) {
        force[i] = 0.0f;
        torque[i] = 0.0f;
    }
    forces(dt);
    const Vec4 p = transformPoint(rotation, force);
    const Quat q = product(orientation, {0.0f, angularVelocity[0], angularVelocity[1], angularVelocity[2]});
    const Vec4 l = transformPoint(rotation, torque);
    for (size_t i = 0; i < 3; ++i) {
        out[kPosition + i] = velocity[i];
        out[kMomentum + i] = p[i];
        out[kAngularMomentum + i] = l[i];
    }
    for (size_t i = 0; i < 4; ++i) out[kOrientation + i] = q[i] * 0.5f;
}

void RigidBody::addForceAt(const Vec4& r, const Vec4& f, float weight) {
    force[0] = force[0] + f[0];
    force[1] = f[1] + force[1];
    force[2] = f[2] + force[2];
    const float fx = f[0] * weight, fy = f[1] * weight, fz = f[2] * weight;
    torque[0] = (fz * r[1] - fy * r[2]) + torque[0];
    torque[1] = (fx * r[2] - fz * r[0]) + torque[1];
    torque[2] = (fy * r[0] - fx * r[1]) + torque[2];
}

void RigidBody::forces(float dt) {
    force = externalForce;
    torque = externalTorque;
    const Vec4 gravity{0.0f, m_def.localGravity * m_def.mass * -10.0f, 0.0f, 0.0f};
    addForceAt({}, transformPoint(inverseRotation, gravity), 1.0f);

    // The contacts, each brought up to the state first; a tyre's own force turns the body by half.
    (void)dt;
    for (Contact* c : contacts) {
        c->update(*this);
        addForceAt(c->arm, c->force, 1.0f);
        addForceAt(c->arm, c->friction, c->def->type == ContactDef::Tyre ? 0.5f : 1.0f);
    }

    // The airfoils: their force times the square of the speed; their drag joins the body's.
    const Vec4& v = localVelocity;
    const float speed2 = (v[2] * v[2] + v[1] * v[1]) + v[0] * v[0];
    float drag = m_def.drag;
    for (const Airfoil& a : m_def.airfoils) {
        addForceAt(a.position, {a.force[0] * speed2, a.force[1] * speed2, a.force[2] * speed2, a.force[3]}, 1.0f);
        drag = drag + a.drag;
    }

    // The drag against the velocity: (drag x factor) x speed², along the unit velocity.
    const float unit = 1.0f / (std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]) + 1.1920929e-7f);
    const float size = -((drag * dragFactor) * speed2);
    addForceAt({}, {v[0] * unit * size, v[1] * unit * size, unit * v[2] * size, v[3]}, 1.0f);
}

void RigidBody::constrain(std::vector<float>& s, float dt) {
    (void)dt;
    const float m = m_def.mass;
    s[kMomentum + 0] = clampKeep(s[kMomentum + 0], m * -150.0f, m * 150.0f);
    s[kMomentum + 1] = clampKeep(s[kMomentum + 1], m * -100.0f, m * 20.0f);
    s[kMomentum + 2] = clampKeep(s[kMomentum + 2], m * -150.0f, m * 150.0f);
    // The angular velocity of the state through the inverse inertia of the body set last, held, and back.
    // Stock problem: the inertia is not the held state's, and the round trip changes L also when nothing is held.
    Vec4 w = transformPoint(inverseWorldInertia,
                            {s[kAngularMomentum], s[kAngularMomentum + 1], s[kAngularMomentum + 2], 0.0f});
    w[0] = clampKeep(w[0], -12.566371f, 12.566371f);
    w[1] = clampKeep(w[1], -4.712389f, 4.712389f);
    w[2] = clampKeep(w[2], -12.566371f, 12.566371f);
    const Vec4 l = transformPoint(inverse(inverseWorldInertia), w);
    for (size_t i = 0; i < 3; ++i) s[kAngularMomentum + i] = l[i];
}

void RigidBody::integrate(Integrator method, float dt) {
    const float halfDt = dt * 0.5f;
    const Vec4 before = localVelocity;
    std::vector<float>& s = state;
    const size_t n = s.size();
    m_rate.resize(n);
    m_next.resize(n);
    switch (method) {
    case Integrator::Euler:
        derivative(m_rate, 0.0f);
        for (size_t i = 0; i < n; ++i) s[i] = dt * m_rate[i] + s[i];
        constrain(s, dt);
        setState(s);
        break;
    case Integrator::Midpoint:
        derivative(m_rate, 0.0f);
        for (size_t i = 0; i < n; ++i) m_next[i] = dt * m_rate[i] + s[i];
        constrain(m_next, dt);
        setState(m_next);
        // The second derivative goes into the buffer of the state it was taken at.
        derivative(m_next, dt);
        for (size_t i = 0; i < n; ++i) s[i] = (m_next[i] + m_rate[i]) * halfDt + s[i];
        constrain(s, dt);
        setState(s);
        break;
    case Integrator::RungeKutta4:
        m_k1.resize(n);
        m_k2.resize(n);
        m_k3.resize(n);
        derivative(m_rate, dt);
        for (size_t i = 0; i < n; ++i) {
            m_k1[i] = dt * m_rate[i];
            m_next[i] = m_k1[i] * 0.5f + s[i];
        }
        constrain(m_next, dt);
        setState(m_next);
        derivative(m_rate, dt);
        for (size_t i = 0; i < n; ++i) {
            m_k2[i] = dt * m_rate[i];
            m_next[i] = m_k2[i] * 0.5f + s[i];
        }
        constrain(m_next, dt);
        setState(m_next);
        derivative(m_rate, dt);
        for (size_t i = 0; i < n; ++i) {
            m_k3[i] = dt * m_rate[i];
            m_next[i] = m_k3[i] + s[i];
        }
        constrain(m_next, dt);
        setState(m_next);
        derivative(m_rate, dt);
        for (size_t i = 0; i < n; ++i) {
            const float k23 = m_k3[i] + m_k2[i];
            s[i] = (((k23 + k23) + dt * m_rate[i]) + m_k1[i]) * 0.16666667f + s[i];
        }
        constrain(s, dt);
        setState(s);
        break;
    }

    if (std::fabs(dt) > 9.9999997e-5f) {
        const float dx = localVelocity[0] - before[0];
        const float dy = localVelocity[1] - before[1];
        const float dz = localVelocity[2] - before[2];
        const float perSecond = 1.0f / dt;
        localVelocityRate = {dx * perSecond, dy * perSecond, dz * perSecond, localVelocity[3]};
    } else {
        for (size_t i = 0; i < 3; ++i) localVelocityRate[i] = 0.0f;
    }
    for (size_t i = 0; i < 3; ++i) {
        externalForce[i] = 0.0f;
        externalTorque[i] = 0.0f;
    }
}

void RigidBody::setLocalVelocity(const Vec4& v) {
    localVelocity = v;
    velocity = transformPoint(rotation, localVelocity);
    const float m = m_def.mass;
    momentum = {velocity[0] * m, velocity[1] * m, velocity[2] * m, velocity[3]};
    for (size_t i = 0; i < 3; ++i) state[kMomentum + i] = momentum[i];
}

void RigidBody::setLocalAngularVelocity(const Vec4& w) {
    localAngularVelocity = w;
    angularVelocity = transformPoint(rotation, localAngularVelocity);
    angularMomentum = transformPoint(inverse(inverseWorldInertia), angularVelocity);
    for (size_t i = 0; i < 3; ++i) state[kAngularMomentum + i] = angularMomentum[i];
}

void RigidBody::setPosition(const Vec4& p) {
    position = p;
    syncNode();
    for (size_t i = 0; i < 3; ++i) state[kPosition + i] = position[i];
}

void RigidBody::setOrientation(const Quat& q, bool sync) {
    orientation = q;
    normalize(orientation);
    rotation = rotationOf(orientation);
    inverseRotation = inverseRigid(rotation);
    angles[0] = -static_cast<float>(std::asin(static_cast<double>(inverseRotation[9])));
    angles[1] = static_cast<float>(
        std::atan2(static_cast<double>(inverseRotation[8]), static_cast<double>(inverseRotation[10])));
    angles[2] = static_cast<float>(
        std::atan2(static_cast<double>(inverseRotation[1]), static_cast<double>(inverseRotation[5])));
    if (sync) syncNode();
    for (size_t i = 0; i < 4; ++i) state[kOrientation + i] = orientation[i];
}

void RigidBody::setAngles(const Vec4& a) {
    angles = a;
    orientation = orientationOf(angles);
    normalize(orientation);
    rotation = rotationOf(orientation);
    inverseRotation = inverseRigid(rotation);
    syncNode();
    for (size_t i = 0; i < 4; ++i) state[kOrientation + i] = orientation[i];
}

void RigidBody::setMomentum(const Vec4& p) {
    momentum = p;
    velocity = {momentum[0] * inverseMass, momentum[1] * inverseMass, momentum[2] * inverseMass, momentum[3]};
    localVelocity = transformPoint(inverseRotation, velocity);
    for (size_t i = 0; i < 3; ++i) state[kMomentum + i] = momentum[i];
}

void RigidBody::setAngularMomentum(const Vec4& l) {
    angularMomentum = l;
    angularVelocity = transformPoint(inverseWorldInertia, angularMomentum);
    localAngularVelocity = transformPoint(inverseRotation, angularVelocity);
    for (size_t i = 0; i < 3; ++i) state[kAngularMomentum + i] = angularMomentum[i];
}

bool RigidBody::impulse(float vn, const Vec4& point, const Vec4& n, const Vec4& linear, const Vec4& angular,
                        float restitution) {
    if (!(vn < 0.0f) && !std::isnan(vn)) return false;
    const float turnShare = vn < -6.0f ? 0.05f : 0.1f;  // [0x00725680]
    const float j = -((ramp(2.0f, -vn, 5.0f) * restitution + 1.0f) * vn);
    // The impulse by the effective mass at the point: 1/m + n . ((I^-1 (r x n)) x r).
    const Vec4 r{point[0] - position[0], point[1] - position[1], point[2] - position[2], point[3]};
    const Vec4 rn{r[1] * n[2] - r[2] * n[1], r[2] * n[0] - r[0] * n[2], r[0] * n[1] - r[1] * n[0], 0.0f};
    const Vec4 w = transformPoint(inverseWorldInertia, rn);
    const float cx = r[2] * w[1] - r[1] * w[2];
    const float cy = r[0] * w[2] - r[2] * w[0];
    const float cz = r[1] * w[0] - r[0] * w[1];
    const Vec4 push0 = scale(n, j / (((cz * n[2] + cy * n[1]) + cx * n[0]) + inverseMass));
    const Vec4 push{push0[0] + linear[0], push0[1] + linear[1], push0[2] + linear[2], push0[3]};
    setMomentum({momentum[0] + push[0], momentum[1] + push[1], momentum[2] + push[2], momentum[3]});
    const Vec4 a = scale(push, turnShare);
    const Vec4 turn{r[1] * a[2] - r[2] * a[1], r[2] * a[0] - r[0] * a[2], r[0] * a[1] - r[1] * a[0], 0.0f};
    setAngularMomentum(add({angularMomentum[0] + turn[0], angularMomentum[1] + turn[1], angularMomentum[2] + turn[2],
                            angularMomentum[3]},
                           angular));
    return true;
}

void RigidBody::resolveImpacts(float dt) {
    // A rigid contact's friction over the step and a turn against the spin about its normal.
    auto extras = [&](Contact& c, Vec4& linear, Vec4& angular) {
        if (c.def->type != ContactDef::Rigid) return;
        c.rigidForce();
        c.slide();
        linear = transformPoint(rotation, {c.friction[0] * dt, c.friction[1] * dt, c.friction[2] * dt,
                                           c.friction[3]});
        float spin = (-angularVelocity[2] * c.normal[2] + -angularVelocity[1] * c.normal[1]) +
                     -angularVelocity[0] * c.normal[0];
        if (spin < -1.0f) {
            spin = -1.0f;
        } else if (spin > 1.0f) {
            spin = 1.0f;
        }
        angular = {c.normal[0] * spin * dt, c.normal[1] * spin * dt, c.normal[2] * spin * dt, c.normal[3]};
    };
    for (Contact* c : contacts) {
        if (c->other == nullptr) continue;
        if (!(c->maxCompression() < c->compression)) continue;
        if (c->restitution < 0.0f || std::isnan(c->restitution)) continue;
        c->relativeVelocity(*this);
        Vec4 linear{}, angular{};
        extras(*c, linear, angular);
        float vn = c->normalSpeed;
        float restitution = c->restitution;
        if (vn > 0.0f && vn < 1.0f) {
            // Moving apart slowly while still pressed in: an approach of the excess, at most 0.3, fully bounced.
            vn = -(c->compression - c->maxCompression());
            if (vn < -0.3f) vn = -0.3f;
            restitution = 1.0f;
        }
        impulseWith(*c->other, vn, c->point, c->normal, linear, angular, restitution);
    }
    for (Contact* c : contacts) {
        if (c->other != nullptr) continue;
        if (!(c->maxCompression() < c->compression)) continue;
        if (c->restitution < 0.0f || std::isnan(c->restitution)) continue;
        c->relativeVelocity(*this);
        Vec4 linear{}, angular{};
        extras(*c, linear, angular);
        impulse(c->normalSpeed, c->point, c->normal, linear, angular, c->restitution);
    }
}

bool RigidBody::impulseWith(RigidBody& o, float vn, const Vec4& point, const Vec4& n, const Vec4& linear,
                            const Vec4& angular, float restitution) {
    if (!(vn < 0.0f) && !std::isnan(vn)) return false;
    const float turnShare = vn < -6.0f ? 0.05f : 0.1f;  // [0x00725680]
    const float j = -((ramp(2.0f, -vn, 5.0f) * restitution + 1.0f) * vn);
    const Vec4 r1{point[0] - position[0], point[1] - position[1], point[2] - position[2], point[3]};
    const Vec4 r2{point[0] - o.position[0], point[1] - o.position[1], point[2] - o.position[2], point[3]};
    // Each body's 1/m + n . ((I^-1 (r x n)) x r) at the point.
    auto share = [&](const RigidBody& b, const Vec4& r) {
        const Vec4 rn{r[1] * n[2] - r[2] * n[1], r[2] * n[0] - r[0] * n[2], r[0] * n[1] - r[1] * n[0], 0.0f};
        const Vec4 w = transformPoint(b.inverseWorldInertia, rn);
        const float cx = r[2] * w[1] - r[1] * w[2];
        const float cy = r[0] * w[2] - r[2] * w[0];
        const float cz = r[1] * w[0] - r[0] * w[1];
        return (cz * n[2] + cy * n[1]) + cx * n[0];
    };
    const float mine = share(*this, r1);
    const float theirs = share(o, r2);
    const float impulse = j / (((mine + theirs) + o.inverseMass) + inverseMass);
    Vec4 push = scale(n, impulse);
    Vec4 back = scale(n, -impulse);
    push = {push[0] + linear[0], push[1] + linear[1], push[2] + linear[2], push[3]};
    back = {back[0] - linear[0], back[1] - linear[1], back[2] - linear[2], back[3]};
    if (impulse > 30000.0f) {
        push[1] = push[1] + 4500.0f;
        back[1] = back[1] + 4500.0f;
    } else if (impulse > 10000.0f) {
        const float lift = impulse * 0.15f;
        push[1] = push[1] + lift;
        back[1] = lift + back[1];
    }
    setMomentum({momentum[0] + push[0], momentum[1] + push[1], momentum[2] + push[2], momentum[3]});
    o.setMomentum({o.momentum[0] + back[0], o.momentum[1] + back[1], o.momentum[2] + back[2], o.momentum[3]});
    auto turn = [&](const Vec4& r, const Vec4& p) {
        const Vec4 a = scale(p, turnShare);
        return Vec4{r[1] * a[2] - r[2] * a[1], r[2] * a[0] - r[0] * a[2], r[0] * a[1] - r[1] * a[0], 0.0f};
    };
    const Vec4 t1 = turn(r1, push);
    setAngularMomentum(add({angularMomentum[0] + t1[0], angularMomentum[1] + t1[1], angularMomentum[2] + t1[2],
                            angularMomentum[3]},
                           angular));
    const Vec4 t2 = turn(r2, back);
    const Vec4 l2{o.angularMomentum[0] + t2[0], o.angularMomentum[1] + t2[1], o.angularMomentum[2] + t2[2],
                  o.angularMomentum[3]};
    o.setAngularMomentum({l2[0] - angular[0], l2[1] - angular[1], l2[2] - angular[2], l2[3]});
    return true;
}

void RigidBody::pushOut() {
    Contact* deepest = nullptr;
    float most = 0.0f;
    for (Contact* c : contacts) {
        if (c->other != nullptr || c->restitution < 0.0f || std::isnan(c->restitution)) continue;
        const float excess = c->compression - c->maxCompression();
        if (!(excess > 0.0f)) continue;
        if (deepest == nullptr || excess > most) {
            most = excess;
            deepest = c;
        }
    }
    if (deepest == nullptr) return;
    const float half = most * 0.5f;
    for (size_t i = 0; i < 3; ++i) {
        position[i] = position[i] + deepest->normal[i] * half;
        state[kPosition + i] = position[i];
    }
}

void RigidBody::computeBounds() {
    if (boundsKnown) return;
    const CollisionDef& c = m_def.collision;
    Vec4 low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
             0.0f};
    Vec4 high{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
              -std::numeric_limits<float>::max(), 0.0f};
    // A value below the least is not compared with the greatest.
    auto take = [&](size_t axis, float v) {
        if (low[axis] <= v) {
            if (high[axis] < v) high[axis] = v;
        } else {
            low[axis] = v;
        }
    };
    // The boxes of the sphere colliders (0x00536E40 with the point less and plus the radius), the lines, the boxes
    // of the sphere collidees, then the meshes' vertices.
    for (const SphereCollider& s : c.sphereColliders) {
        for (size_t i = 0; i < 3; ++i) take(i, s.point[i] - s.radius);
        for (size_t i = 0; i < 3; ++i) take(i, s.point[i] + s.radius);
    }
    for (const LineCollider& l : c.lines) {
        for (size_t i = 0; i < 3; ++i) take(i, l.point[i]);
        for (size_t i = 0; i < 3; ++i) take(i, l.point[i] + l.direction[i] * l.length);
    }
    for (const SphereCollidee& s : c.sphereCollidees) {
        for (size_t i = 0; i < 3; ++i) take(i, s.point[i] - s.radius);
        for (size_t i = 0; i < 3; ++i) take(i, s.point[i] + s.radius);
    }
    for (const auto* shape : {c.colliderShape.get(), c.collideeShape.get()}) {
        if (shape == nullptr) continue;
        for (const Vec4& v : shape->vertices) {
            for (size_t i = 0; i < 3; ++i) take(i, v[i]);
        }
    }
    boundsCentre = {(high[0] + low[0]) * 0.5f, (high[1] + low[1]) * 0.5f, (high[2] + low[2]) * 0.5f, 0.0f};
    boundsRadius = 0.0f;
    const Vec4& m = boundsCentre;
    auto reach = [&](float r) {
        if (boundsRadius < r) boundsRadius = r;
    };
    // A sphere reaches its centre's distance plus its radius.
    for (const SphereCollider& s : c.sphereColliders) {
        const float dx = s.point[0] - m[0], dy = s.point[1] - m[1], dz = s.point[2] - m[2];
        reach(std::sqrt((dz * dz + dy * dy) + dx * dx) + s.radius);
    }
    for (const LineCollider& l : c.lines) {
        const float dx = l.point[0] - m[0], dy = l.point[1] - m[1], dz = l.point[2] - m[2];
        reach(std::sqrt((dz * dz + dy * dy) + dx * dx));
        const float ex = (l.point[0] + l.direction[0] * l.length) - m[0];
        const float ey = (l.point[1] + l.direction[1] * l.length) - m[1];
        const float ez = (l.point[2] + l.direction[2] * l.length) - m[2];
        reach(std::sqrt((ez * ez + ey * ey) + ex * ex));
    }
    for (const SphereCollidee& s : c.sphereCollidees) {
        const float dx = s.point[0] - m[0], dy = s.point[1] - m[1], dz = s.point[2] - m[2];
        reach(std::sqrt((dz * dz + dy * dy) + dx * dx) + s.radius);
    }
    if (c.colliderShape != nullptr) {
        for (const Vec4& v : c.colliderShape->vertices) {
            const float dx = v[0] - m[0], dy = v[1] - m[1], dz = v[2] - m[2];
            reach(std::sqrt((dz * dz + dy * dy) + dx * dx));
        }
    }
    if (c.collideeShape != nullptr) {
        for (const Vec4& v : c.collideeShape->vertices) {
            const float dx = v[0] - m[0], dy = v[1] - m[1], dz = v[2] - m[2];
            reach(std::sqrt((dx * dx + dz * dz) + dy * dy));
        }
    }
    boundsKnown = true;
}

void RigidBody::addTouch(const Touch& t) {
    if (fixed) return;
    const ContactDef* d = contactDefOf(t.id, t.point);
    if (d == nullptr || pools == nullptr) return;
    if (d->type == ContactDef::Rigid) {
        if (pools->rigidUsed < pools->rigid.size() && rigidCount < 35) {
            Contact& c = pools->rigid[pools->rigidUsed++];
            c.begin(t, *this, *d);
            contacts.push_back(&c);
            ++rigidCount;
        }
    } else if (d->type == ContactDef::Spring) {
        if (pools->springUsed < pools->spring.size() && springCount < 35) {
            Contact& c = pools->spring[pools->springUsed++];
            c.begin(t, *this, *d);
            contacts.push_back(&c);
            ++springCount;
        }
    }
}

const ContactDef* RigidBody::contactDefOf(int32_t id, const Vec4& p) const {
    for (const ContactDef& c : m_def.contacts) {
        if (c.id != id) continue;
        if (!c.clipped) return &c;
        const float beyond = ((p[2] - c.clipPoint[2]) * c.clipNormal[2] + (p[1] - c.clipPoint[1]) * c.clipNormal[1]) +
                             (p[0] - c.clipPoint[0]) * c.clipNormal[0];
        if (beyond > 0.0f) return &c;
    }
    return nullptr;
}

void RigidBody::step(float dt) {
    if (fixed) {
        // 0x00535810
        if (collision != nullptr && world != nullptr) collision->collide(*this);
        return;
    }
    const Vec4 before = position;
    rigidFirst = pools != nullptr ? pools->rigidUsed : 0;
    rigidCount = 0;
    springFirst = pools != nullptr ? pools->springUsed : 0;
    springCount = 0;
    contacts.clear();
    if (collision != nullptr) collision->collide(*this);
    // A rigid contact facing up carries its share of the weight by how much it faces up.
    if (rigidCount > 0) {
        const float share = m_def.localGravity * m_def.mass * 10.0f / static_cast<float>(rigidCount);
        for (size_t i = 0; i < rigidCount; ++i) {
            Contact& c = pools->rigid[rigidFirst + i];
            c.weight = c.normal[1] > 0.0f && c.other == nullptr ? share * c.normal[1] : 0.0f;
        }
    }
    resolveImpacts(dt);
    integrate(integrator, dt);
    pushOut();
    displacement = {position[0] - before[0], position[1] - before[1], position[2] - before[2], position[3]};
    if (m_def.sleepSet) {
        // A body that moves less than 2 cm or turns slower than 2 rad/s in a step for 3 s goes to sleep: off the
        // world's active list (its node keeps the last place it followed).
        const Vec4& d = displacement;
        if ((d[2] * d[2] + d[1] * d[1]) + d[0] * d[0] > 0.0004f) sleepTimer = 0.0f;
        const Vec4& w = angularVelocity;
        if ((w[2] * w[2] + w[1] * w[1]) + w[0] * w[0] > 4.0f) sleepTimer = 0.0f;
        sleepTimer = dt + sleepTimer;
        if (sleepTimer > 3.0f) {
            if (world != nullptr && active) world->activate(*this, false);
            sleepTimer = 0.0f;
            return;
        }
    }
    if (_stricmp(m_def.collision.name.c_str(), "wheel") == 0) {
        const bool level = std::fabs(rotation[4]) < 0.8f;
        standing = level;
        lying = !level;
    }
    // The slow body's brake (0x004F2AFE).
    const Vec4& v = localVelocity;
    const float speed = std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]);
    const float inverse = 1.0f / (1.1920929e-7f + speed);
    const float ux = v[0] * inverse, uy = v[1] * inverse, uz = v[2] * inverse;
    if (!(speed < 0.3f)) return;
    float loss = dt + dt;
    if (0.0f > loss) loss = 0.0f;
    const float left = speed - loss;
    setLocalVelocity({ux * left, uy * left, uz * left, v[3]});
}

}  // namespace oj
