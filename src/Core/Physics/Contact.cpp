#include "Core/Physics/Contact.h"

#include "Core/Formats/Cld.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Physics/RigidBody.h"

#include <cmath>

namespace oj {

namespace {

constexpr float kTiny = 1.1920929e-7f;

// 0x004F8340: v less its part along n; w of v.
Vec4 project(const Vec4& v, const Vec4& n) {
    const float along = (n[1] * v[1] + n[0] * v[0]) + v[2] * n[2];
    return {v[0] - n[0] * along, v[1] - n[1] * along, v[2] - n[2] * along, v[3]};
}

// 0x00489910: v made a unit vector in place; its length before.
float normalizeMeasured(Vec4& v) {
    const float length = std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]);
    const float unit = 1.0f / (length + kTiny);
    v[0] = unit * v[0];
    v[1] = unit * v[1];
    v[2] = unit * v[2];
    return length;
}

// 0x00407C00
void normalizeVector(Vec4& v) {
    normalizeMeasured(v);
}

// 0x004F3830
float ramp(float a, float x, float b) {
    if (x < a) return (a - a) / (b - a);
    if (x > b) return (b - a) / (b - a);
    return (x - a) / (b - a);
}

// 0x004F7A60: the slip ratio of a tyre: its rim speed against the ground's along it, the angle held within ±pi/2.
float slipRatio(float spin, float radius, float speed, float angle) {
    float a = angle;
    if (a < -1.5707964f) {
        a = -1.5707964f;
    } else if (a > 1.5707964f) {
        a = 1.5707964f;
    }
    return spin * radius / (productToFloat(std::cos(static_cast<double>(a)), speed) + kTiny) - 1.0f;
}

}  // namespace

void Contact::begin(const Touch& t, const RigidBody& body, const ContactDef& d) {
    def = &d;
    zone = t.zone;
    mu = d.mu * t.friction;
    restitution = d.restitution * t.bounce;
    const Vec4 p{t.point[0] * 0.01f, t.point[1] * 0.01f, t.point[2] * 0.01f, t.point[3]};
    const Vec4 turned = transformPoint(body.rotation, p);
    const Vec4 o = body.origin();
    point = {turned[0] + o[0], turned[1] + o[1], turned[2] + o[2], o[3]};
    arm = transformPoint(body.inverseRotation, {point[0] - body.position[0], point[1] - body.position[1],
                                                point[2] - body.position[2], point[3]});
    localNormal = t.normal;
    normal = transformPoint(body.rotation, localNormal);
    compression = t.depth * 0.01f;
    other = t.other;
    relativeVelocity(body);
    if (d.type == ContactDef::Tyre) {
        slipVelocity[0] = slipVelocity[1] = slipVelocity[2] = 0.0f;
        slip = 0.0f;
        groundSpin = 0.0f;
    }
}

void Contact::relativeVelocity(const RigidBody& body) {
    // The point's velocity in the body's frame: v + w x r, less the other body's velocity at the point: its own
    // v + w x r in its frame, turned into the world and then into this body's frame.
    const Vec4& w = body.localAngularVelocity;
    const float cx = w[1] * arm[2] - w[2] * arm[1];
    const float cy = w[2] * arm[0] - w[0] * arm[2];
    const float cz = w[0] * arm[1] - w[1] * arm[0];
    const Vec4& v = body.localVelocity;
    float ox = 0.0f, oy = 0.0f, oz = 0.0f;
    if (other != nullptr) {
        const Vec4 r = transformPoint(other->inverseRotation, {point[0] - other->position[0],
                                                               point[1] - other->position[1],
                                                               point[2] - other->position[2], point[3]});
        const Vec4& ow = other->localAngularVelocity;
        const float ex = r[2] * ow[1] - r[1] * ow[2];
        const float ey = ow[2] * r[0] - r[2] * ow[0];
        const float ez = r[1] * ow[0] - ow[1] * r[0];
        const Vec4& ov = other->localVelocity;
        const Vec4 world = transformPoint(other->rotation, {ov[0] + ex, ov[1] + ey, ov[2] + ez, ov[3]});
        const Vec4 mine = transformPoint(body.inverseRotation, world);
        ox = mine[0];
        oy = mine[1];
        oz = mine[2];
    }
    velocity = {(v[0] + cx) - ox, (v[1] + cy) - oy, (v[2] + cz) - oz, v[3]};
    normalSpeed = (velocity[2] * localNormal[2] + velocity[1] * localNormal[1]) + velocity[0] * localNormal[0];
}

void Contact::update(const RigidBody& body) {
    localNormal = transformPoint(body.inverseRotation, normal);
    const Vec4 turned = transformPoint(body.rotation, arm);
    const float x = turned[0] + body.position[0];
    const float y = turned[1] + body.position[1];
    const float z = turned[2] + body.position[2];
    compression = compression - (((z - point[2]) * normal[2] + (y - point[1]) * normal[1]) + (x - point[0]) * normal[0]);
    point = {x, y, z, body.position[3]};
    relativeVelocity(body);
    if (def->type == ContactDef::Rigid) {
        rigidForce();
    } else {
        springForce();
    }
    if (def->type == ContactDef::Tyre) {
        tyreForce();
    } else {
        slide();
    }
}

void Contact::springForce() {
    const ContactDef& d = *def;
    float x = compression;
    if (x < 0.0f) {
        x = 0.0f;
    } else if (!(x < d.maxCompression)) {
        x = d.maxCompression;
    }
    load = x * d.stiffness;
    load = load - d.damping * normalSpeed;
    const float most = d.maxCompression * d.stiffness;
    if (load < 0.0f) {
        load = 0.0f;
    } else if (load > most) {
        load = most;
    }
    force = {localNormal[0] * load, localNormal[1] * load, localNormal[2] * load, localNormal[3]};
}

void Contact::rigidForce() {
    // The share of the body's weight by how deep the contact presses, less its speed along the normal.
    const float most = maxCompression();
    float x = compression;
    if (x < 0.0f) {
        x = 0.0f;
    } else if (x > most) {
        x = most;
    }
    const float share = most > 0.0f ? x / most : 1.0f;
    load = (share * 1.1f - normalSpeed * 0.01f) * weight;
    const float top = weight * 1.1f;
    if (load < 0.0f) {
        load = 0.0f;
    } else if (load > top) {
        load = top;
    }
    force = {localNormal[0] * load, localNormal[1] * load, localNormal[2] * load, localNormal[3]};
}

void Contact::slide() {
    // Against the sliding: the load times Mu, and on steep faces a share by the depth (x 1000), less under 1 m/s.
    Vec4 along = project(velocity, localNormal);
    const float speed = normalizeMeasured(along);
    float f = -(load * mu) - (1.0f - std::fabs(normal[1])) * compression * mu * 1000.0f;
    if (speed < 1.0f) f = f * speed;
    friction = {along[0] * f, along[1] * f, along[2] * f, along[3]};
}

namespace {

// 0x004F7AC0: the Magic Formula of a tyre with combined slip: the lateral and the longitudinal force at a slip ratio,
// a slip angle and a load. The tangent, arctangents and cosine are the x87's at full precision until they round.
void magicFormula(const ContactDef& d, float kappa, float alpha, float load, float& lateral, float& longitudinal) {
    const auto t = static_cast<float>(std::tan(static_cast<double>(alpha)));
    const float sa = t * d.lateralStiffness / (load * d.lateralMu);
    const float sk = kappa * d.longitudinalStiffness / (load * d.longitudinalMu);
    const float sigma = std::sqrt(sk * sk + sa * sa);
    // The friction circle becomes an ellipse of the two Mu's ratio at small slips, a circle from 2 pi on.
    float k = 1.0f;
    if (!(sigma > 6.2831855f) && !std::isnan(sigma)) {
        k = (d.muRatio + 1.0f) * 0.5f -
            productToFloat(std::cos(static_cast<double>(sigma * 0.5f)), 1.0f - d.muRatio) * 0.5f;
    }
    const float b = d.magic[0], c = d.magic[1], peak = d.magic[2], e = d.magic[3];
    const float curve = productToFloat(std::atan(static_cast<double>(sigma * b)), d.eOverB) + (1.0f - e) * sigma;
    const float angle = productToFloat(std::atan(static_cast<double>(curve * b)), c);
    const float force = static_cast<float>(std::sin(static_cast<double>(angle))) * peak;
    const float share = force * (1.0f / (std::sqrt(k * k * t * t + kappa * kappa) + kTiny));
    lateral = k * share * t * d.lateralMu * load;
    longitudinal = share * kappa * d.longitudinalMu * load;
}

}  // namespace

void Contact::tyreForce() {
    const ContactDef& d = *def;
    const float grip = gravityInverse * load;
    const bool reversing = wheelSpin < 0.0f;
    if (localNormal[1] <= 0.0f) {
        friction[0] = friction[1] = friction[2] = 0.0f;
        return;
    }
    // The wheel's lateral and forward axes on the ground.
    Vec4 lateral{1.0f, 0.0f, 0.0f, 0.0f};
    Vec4 forward{0.0f, 0.0f, 1.0f, 0.0f};
    if (steer != 0.0f) {
        const auto c = static_cast<float>(std::cos(static_cast<double>(steer)));
        const auto s = static_cast<float>(std::sin(static_cast<double>(steer)));
        lateral = project({c, 0.0f, -s, 0.0f}, localNormal);
        forward = project({s, 0.0f, c, 0.0f}, localNormal);
        normalizeVector(lateral);
        normalizeVector(forward);
    }
    friction[0] = friction[1] = friction[2] = 0.0f;

    // The slip angle and ratio from the velocity along the ground; the Magic Formula fades in from 8 to 10 m/s.
    const Vec4 v = project(velocity, localNormal);
    const float speed = std::sqrt((v[1] * v[1] + v[0] * v[0]) + v[2] * v[2]);
    float fade = ramp(8.0f, speed, 10.0f);
    const double vx = v[0], vz = v[2];
    float angle;
    if (steer == 0.0f) {
        angle = static_cast<float>(reversing ? -std::atan2(-vx, -vz) : -std::atan2(vx, vz));
    } else {
        const double heading = std::atan2(static_cast<double>(forward[0]), static_cast<double>(forward[2]));
        angle = static_cast<float>(reversing ? heading - std::atan2(-vx, -vz) : heading - std::atan2(vx, vz));
    }
    const float rolled = productToFloat(std::cos(static_cast<double>(angle)), speed);
    groundSpin = reversing ? -rolled : rolled;
    slip = slipRatio(reversing ? -wheelSpin : wheelSpin, radius, speed, angle);

    if (fade > 0.0f) {
        fade = (1.0f - ramp(0.3f, std::fabs(angle), 1.0f)) * fade;
        if (fade > 0.0f) {
            const float bySlip = slip > 0.0f ? 1.0f - ramp(0.7f, slip, 1.0f) : ramp(-0.9f, slip, -0.6f);
            fade = bySlip * fade;
            if (fade > 0.0f) {
                if (!(normal[1] > 0.8f)) {
                    fade = 0.0f;
                } else {
                    fade = (normal[1] - 0.8f) * fade * 5.0f;
                    if (fade > 0.0f) {
                        float side = 0.0f, along = 0.0f;
                        magicFormula(d, slip, angle, grip + kTiny, side, along);
                        if (steer == 0.0f) {
                            if (reversing) {
                                // Stock problem: backwards without steering the longitudinal force is taken off and
                                // added back, so it is lost. The fixed build takes it off once, as a steered
                                // wheel's whole force backwards.
                                friction[0] = friction[0] - side * fade;
                                friction[2] = friction[2] - along * fade;
                            } else {
                                friction[0] = side * fade + friction[0];
                                friction[2] = along * fade + friction[2];
                            }
                        } else {
                            const Vec4 sum = add(scale(lateral, side), scale(forward, along));
                            const Vec4 f = scale(sum, fade);
                            for (size_t i = 0; i < 3; ++i) {
                                friction[i] = reversing ? friction[i] - f[i] : f[i] + friction[i];
                            }
                        }
                    }
                }
            }
        }
    }

    // The tyre's sliding velocity: the ground's less the rim's.
    if (steer == 0.0f) {
        slipVelocity = v;
        slipVelocity[2] = slipVelocity[2] - wheelSpin * radius;
    } else {
        const Vec4 rim = scale(forward, wheelSpin * radius);
        slipVelocity = {v[0] - rim[0], v[1] - rim[1], v[2] - rim[2], v[3]};
    }
    // Below the full Magic Formula the tyre slides: against the sliding velocity by the load and Mu, at 0.4 when
    // locked hard or under 8 m/s, at 0.1 when spinning on the throttle.
    if (fade < 1.0f) {
        Vec4 direction = slipVelocity;
        const float sliding = normalizeMeasured(direction);
        float f = -(grip * mu);
        if (slip < -0.9f || speed < 8.0f) {
            f = f * 0.4f;
        } else if (slip > 1.0f && throttle > 0.5f) {
            f = f * 0.1f;
        }
        if (sliding < 1.0f) f = f * sliding;
        const Vec4 s = scale(direction, (1.0f - fade) * f);
        for (size_t i = 0; i < 3; ++i) friction[i] = s[i] + friction[i];
    }
    for (size_t i = 0; i < 3; ++i) friction[i] = gravity * friction[i];
    if (normal[1] < 0.1f) friction[0] = friction[1] = friction[2] = 0.0f;
}

}  // namespace oj
