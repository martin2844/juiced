#include "Core/Network/NetCar.h"

#include "Core/Physics/CarBody.h"
#include "Core/Base/Matrix.h"
#include "Core/Physics/PhysicsWorld.h"
#include "Core/Race/Racer.h"
#include "Core/Base/X87.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace oj::net {

namespace {

// The state's layout: [0x00] the position (3 floats), [0x0C] the velocity in the body's frame (3 int16), [0x14]
// the orientation (w, x, y, z), [0x24] the angular velocity in the body's frame (3 int16), [0x2A] the wheels' spins
// (4 int16), [0x32] the throttle (int8), [0x33] the brake, [0x34] the handbrake (uint8), [0x35] the steering
// (int8), [0x36] the engine's speed (uint16), [0x38] the race's distance (float), [0x3D] the flags: the gear (bits
// 0 .. 2), the engine on (3), put back on the line (4), the race paused (5), the horn (6), the nitro (7).
const float kVelocity = std::bit_cast<float>(0x4372B8E4u);      // 242.72
const float kVelocityBack = std::bit_cast<float>(0x3B870087u);  // 0.004119936
const float kAngular = std::bit_cast<float>(0x454CCC00u);       // 3276.75
const float kAngularBack = std::bit_cast<float>(0x39A000A0u);   // 0.000305185
const float kSpin = std::bit_cast<float>(0x430311ECu);          // 131.07
const float kSpinBack = std::bit_cast<float>(0x3BFA00FAu);      // 0.007629511
const float kSpinSmooth = std::bit_cast<float>(0x3A0C008Cu);    // 0.00053406577
const float kEngine = std::bit_cast<float>(0x4226E22Du);        // 41.72
const float kEngineBack = std::bit_cast<float>(0x3E6A60EAu);    // 0.22888532 (to rpm)
const float kRpm = std::bit_cast<float>(0x3DD67750u);           // 0.10471976 (rpm to rad/s)
const float kSigned = std::bit_cast<float>(0x3C010204u);        // 1 / 127
const float kUnsigned = std::bit_cast<float>(0x3B808081u);      // 1 / 255
const float kStep = std::bit_cast<float>(0x3D8F5C29u);          // 0.07
const float kKeep = std::bit_cast<float>(0x3F6E147Bu);          // 0.93
const float kStepY = std::bit_cast<float>(0x3E570A3Eu);         // 0.21
const float kKeepY = std::bit_cast<float>(0x3F4A3D71u);         // 0.79

float f32(const uint8_t* p) {
    float v;
    std::memcpy(&v, p, 4);
    return v;
}

void putF(uint8_t* p, float v) {
    std::memcpy(p, &v, 4);
}

int16_t i16(const uint8_t* p) {
    int16_t v;
    std::memcpy(&v, p, 2);
    return v;
}

void put16(uint8_t* p, int32_t v) {
    const auto s = static_cast<uint16_t>(v);
    std::memcpy(p, &s, 2);
}

Vec4 vec(const uint8_t* p, float k, float w) {
    return {static_cast<float>(i16(p)) * k, static_cast<float>(i16(p + 2)) * k, static_cast<float>(i16(p + 4)) * k,
            w};
}

Quat quat(const uint8_t* p) {
    return {f32(p), f32(p + 4), f32(p + 8), f32(p + 12)};
}

// What 0x004E73E0 and 0x004E6E30 share after the spins: the engine's speed (in the state too), the pedals, the
// steering, the engine on, the gear, the race paused, the nitro and the horn.
void applyRest(RaceCar& car, const uint8_t* s, PhysicsWorld* world, void (*horn)(RaceCar&, bool)) {
    CarBody& body = *car.body;
    const float engine = static_cast<float>(static_cast<uint16_t>(i16(s + 0x36))) * kEngineBack * kRpm;
    body.engineSpeed = engine;
    body.state[body.engineIndex] = engine;
    const float throttle = static_cast<float>(static_cast<int8_t>(s[0x32])) * kSigned;
    body.throttle = throttle * throttle;
    const float brake = static_cast<float>(s[0x33]) * kUnsigned;
    body.brake = brake * brake * brake;
    body.handbrake = static_cast<float>(s[0x34]) * kUnsigned;
    float steering = static_cast<float>(static_cast<int8_t>(s[0x35])) * kSigned;
    if (!(-1.0f <= steering)) {
        steering = -1.0f;
    } else if (1.0f < steering) {
        steering = 1.0f;
    }
    body.steering = steering;
    const uint8_t flags = s[0x3D];
    if ((flags & 0x08) == 0) {
        body.engineOn = false;
        body.clutch = 0.0f;
        body.clutchIn = false;
    } else if (!body.engineOn) {
        body.engineOn = true;
        body.clutch = 0.0f;
        body.clutchIn = false;
    }
    if ((flags & 7u) != body.gear) {
        if (body.engineOn) {
            body.clutch = 0.0f;
            body.clutchIn = false;
        }
        body.setGear(flags & 7u);
    }
    car.netPaused = (flags & 0x20) != 0;
    if (world != nullptr) {
        if (car.netPaused) {
            world->remove(body);
        } else {
            world->add(body);
        }
    }
    car.nitroAsked = (flags & 0x80) != 0;
    if (horn != nullptr) horn(car, (flags & 0x40) != 0);
}

}  // namespace

void carState(const RaceCar& car, bool paused, bool horn, uint8_t* s) {
    CarBody& body = *car.body;
    for (size_t i = 0; i < 3; ++i) putF(s + i * 4, body.position[i]);
    for (size_t i = 0; i < 4; ++i) putF(s + 0x14 + i * 4, body.orientation[i]);
    for (size_t i = 0; i < 3; ++i) put16(s + 0x0C + i * 2, x87::nearest(body.localVelocity[i] * kVelocity));
    for (size_t i = 0; i < 3; ++i) put16(s + 0x24 + i * 2, x87::nearest(body.localAngularVelocity[i] * kAngular));
    for (int32_t i = 0; i < 4; ++i) {
        const CarWheel* w = body.wheelOf(i);
        put16(s + 0x2A + i * 2, x87::nearest((w != nullptr ? w->spin : 0.0f) * kSpin));
    }
    put16(s + 0x36, x87::nearest(body.engineSpeed * kEngine));
    s[0x32] = static_cast<uint8_t>(x87::nearest(body.throttle * 127.0f));
    s[0x33] = static_cast<uint8_t>(x87::nearest(body.brake * 255.0f));
    s[0x34] = static_cast<uint8_t>(x87::nearest(body.handbrake * 255.0f));
    s[0x35] = static_cast<uint8_t>(x87::nearest(body.steering * 127.0f));
    putF(s + 0x38, car.competitor != nullptr ? car.competitor->raceDistance : 0.0f);
    auto flags = static_cast<uint32_t>(s[0x3D]);
    flags = (flags & ~7u) | (body.gear & 7u);
    flags = (flags & 0xE7u) | (body.engineOn ? 0x08u : 0u);
    flags = (flags & ~0x20u) | (paused ? 0x20u : 0u);
    flags = (flags & 0x7Fu) | (car.nitroAsked ? 0x80u : 0u);
    flags = (flags & ~0x40u) | (horn ? 0x40u : 0u);
    s[0x3D] = static_cast<uint8_t>(flags);
}

void applyState(RaceCar& car, const uint8_t* s, PhysicsWorld* world, void (*horn)(RaceCar&, bool)) {
    CarBody& body = *car.body;
    const Quat q = quat(s + 0x14);
    // (The position's w and the velocities' are what the stack held: the orientation's z for the velocities.)
    if (!car.parked) body.setPosition({f32(s), f32(s + 4), f32(s + 8), body.position[3]});
    body.setOrientation(q, true);
    body.setLocalVelocity(vec(s + 0x0C, kVelocityBack, q[3]));
    body.setLocalAngularVelocity(vec(s + 0x24, kAngularBack, q[3]));
    for (int32_t i = 0; i < 4; ++i) {
        CarWheel* w = body.wheelOf(i);
        if (w == nullptr) continue;
        w->spin = static_cast<float>(i16(s + 0x2A + i * 2)) * kSpinBack;
        body.state[w->index] = w->spin;
    }
    applyRest(car, s, world, horn);
}

void smoothState(RaceCar& car, const uint8_t* s, PhysicsWorld* world, void (*horn)(RaceCar&, bool)) {
    CarBody& body = *car.body;
    const Vec4 at = body.position;
    const float sx = f32(s), sy = f32(s + 4), sz = f32(s + 8);
    Vec4 p{sx * kStep + at[0] * kKeep, sy * kStepY + at[1] * kKeepY, sz * kStep + at[2] * kKeep, at[3]};
    if (std::fabs(sx - at[0]) > 30.0f) p[0] = sx;
    if (std::fabs(sy - at[1]) > 3.0f) p[1] = sy;
    if (std::fabs(sz - at[2]) > 30.0f) p[2] = sz;
    body.setPosition(p);
    body.setOrientation(slerp(body.orientation, quat(s + 0x14), kStep), true);
    auto towards = [](const Vec4& now, const Vec4& to) {
        return Vec4{now[0] * kKeep + to[0] * kStep, now[1] * kKeep + to[1] * kStep, now[2] * kKeep + to[2] * kStep,
                    now[3]};
    };
    body.setLocalVelocity(towards(body.localVelocity, vec(s + 0x0C, kVelocityBack, 0.0f)));
    body.setLocalAngularVelocity(towards(body.localAngularVelocity, vec(s + 0x24, kAngularBack, 0.0f)));
    for (int32_t i = 0; i < 4; ++i) {
        const CarWheel* had = body.wheelOf(i);
        const float now = had != nullptr ? had->spin : 0.0f;
        CarWheel* w = body.wheelOf(i);
        if (w == nullptr) continue;
        w->spin = static_cast<float>(i16(s + 0x2A + i * 2)) * kSpinSmooth + now * kKeep;
        body.state[w->index] = w->spin;
    }
    applyRest(car, s, world, horn);
}

void extrapolate(uint8_t* s, float dt) {
    const Quat q = quat(s + 0x14);
    const Mat34 r = rotationOf(q);
    auto turned = [&](const Vec4& v) {
        return Vec4{(r[2] * v[2] + r[1] * v[1]) + r[0] * v[0], (r[6] * v[2] + r[5] * v[1]) + r[4] * v[0],
                    (r[10] * v[2] + r[9] * v[1]) + r[8] * v[0], 0.0f};
    };
    const Vec4 v = turned(vec(s + 0x0C, kVelocityBack, 0.0f));
    for (size_t i = 0; i < 3; ++i) putF(s + i * 4, f32(s + i * 4) + v[i] * dt);
    const Vec4 a = turned(vec(s + 0x24, kAngularBack, 0.0f));
    const Quat spin = product(q, {0.0f, a[0], a[1], a[2]});
    Quat n{};
    for (size_t i = 0; i < 4; ++i) n[i] = spin[i] * 0.5f * dt + q[i];
    const float inverse =
        1.0f / (std::sqrt(((n[3] * n[3] + n[2] * n[2]) + n[1] * n[1]) + n[0] * n[0]) + 1.1920929e-07f);
    for (size_t i = 0; i < 4; ++i) putF(s + 0x14 + i * 4, n[i] * inverse);
}

}  // namespace oj::net
