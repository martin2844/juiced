// Unit tests: RigidBody.

#include "Framework/Test.h"

#include "Core/Base/Matrix.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Physics/PhysicsWorld.h"
#include "Core/Physics/RigidBody.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// A rigid body: a free fall with RK4 is exact for a constant force up to the floats; the drag and an airfoil act
// along the body's frame; the limits hold the momentum and the angular velocity.
OJ_UNIT_TEST(RigidBody, FreeFallAndAirfoil) {
    {
        const auto parsed = oj::parseDynamics(
            "[BeginCar]\nMass 1000\nInertia 1 1 1\nCentreOfGravity 0 0.5 0\nLocalGravity 1.5\nDragCoefficient 0\n"
            "[BeginAirfoil]\nPosition 0 0.5 -2\nForce 0 -1 0\nDragCoefficient 0.25\n[EndAirfoil]\n[EndCar]\n");
        bool ok = parsed && parsed->body.airfoils.size() == 1 && parsed->body.airfoils[0].position[1] == 0.0f &&
                  parsed->body.airfoils[0].position[2] == -2.0f && parsed->body.airfoils[0].drag == 0.25f;
        oj::BodyDef still = parsed ? parsed->body : oj::BodyDef{};
        still.airfoils.clear();
        oj::RigidBody fall(still, {0.0f, 100.0f, 0.0f, 0.0f});
        ok = ok && fall.position[1] == 100.5f;
        for (int i = 0; i < 60; ++i) fall.step(1.0f / 60.0f);
        // The slow body's brake takes 2 dt off the first step's 0.25 m/s (the later steps are faster than 0.3 m/s).
        const float braked = 2.0f / 60.0f;
        ok = ok && std::fabs(fall.velocity[1] + 15.0f - braked) < 1e-3f &&
             std::fabs(fall.position[1] - (93.0f + braked * 59.0f / 60.0f)) < 1e-3f && fall.velocity[0] == 0.0f &&
             std::fabs(fall.orientation[0] - 1.0f) < 1e-6f;
        const oj::Mat34 model = fall.modelMatrix();
        ok = ok && std::fabs(model[7] - (fall.position[1] - 0.5f) * 100.0f) < 1e-2f;
        check(ok, "rigid body: definition read, free fall at 1.5 g with RK4, model matrix in centimetres");

        // The airfoil at the back pushes down, which pitches the nose up (a torque about x).
        oj::BodyDef winged = parsed ? parsed->body : oj::BodyDef{};
        oj::RigidBody car(winged, {});
        car.momentum = {0.0f, 0.0f, 30000.0f, 0.0f};
        car.state[oj::RigidBody::kMomentum + 2] = 30000.0f;
        car.setState(car.state);
        car.derivative(car.state, 0.0f);
        const float lift = car.force[1] + 15000.0f;
        check(std::fabs(lift + 900.0f) < 1e-2f && car.torque[0] < 0.0f && std::fabs(car.force[2] + 225.0f) < 1e-2f,
              "rigid body: airfoil force and torque by the speed squared, drag against the velocity");

        oj::RigidBody spun(still, {});
        spun.state[oj::RigidBody::kMomentum + 1] = 50000.0f;
        spun.state[oj::RigidBody::kAngularMomentum + 1] = 100000.0f;
        spun.constrain(spun.state, 0.0f);
        spun.setState(spun.state);
        check(spun.momentum[1] == 20000.0f && std::fabs(spun.angularVelocity[1] - 4.712389f) < 1e-4f,
              "rigid body: momentum and angular velocity limits");
    }
}
// A body's limits (0x004F1E10): the angular momentum through the inertia and back. The stock's used the inertia of the
// state set before and made the round trip at every hold; the fixed build's leaves a free state's L as it is and holds
// by the held state's own inertia. Turned a quarter about z the state's y is the body's x (30 against the body's own
// y's 170): a y spin past 4.71 rad/s is held at 4.71 (the stock's, by the unturned body's inertia, 170 x 4.71 / 30 =
// 26.7 rad/s once the state is set).
OJ_UNIT_TEST(RigidBody, LimitsOfTheHeldState) {
    oj::BodyDef def;
    def.mass = 100.0f;
    def.inertia = {0.3f, 1.7f, 0.9f, 0.0f};
    oj::RigidBody b(def, {});
    std::vector<float> s = b.state;
    const float half = 0.70710677f;
    s[oj::RigidBody::kOrientation + 0] = half;
    s[oj::RigidBody::kOrientation + 3] = half;
    s[oj::RigidBody::kAngularMomentum + 0] = 37.1f;
    s[oj::RigidBody::kAngularMomentum + 1] = -12.3f;
    s[oj::RigidBody::kAngularMomentum + 2] = 21.7f;
    std::vector<float> free = s;
    b.constrain(free, 0.0f);
    bool ok = free[oj::RigidBody::kAngularMomentum + 0] == 37.1f &&
              free[oj::RigidBody::kAngularMomentum + 1] == -12.3f &&
              free[oj::RigidBody::kAngularMomentum + 2] == 21.7f;
    std::vector<float> spun = s;
    spun[oj::RigidBody::kAngularMomentum + 1] = 100000.0f;
    b.constrain(spun, 0.0f);
    b.setState(spun);
    std::printf("     the limits: a free state's L %s, the held y spin %.2f rad/s\n", ok ? "kept" : "changed",
                b.angularVelocity[1]);
    ok = ok && std::fabs(b.angularVelocity[1] - 4.712389f) < 1e-3f;
    check(ok, "rigid body (fixed): the limits leave a free state alone and hold by the held state's inertia");
}
// The slow body's brake (0x004F2AFE): under 0.3 m/s the speed loses 2 dt. The stock's went past 0 and turned the body
// the other way (a body at rest shook); the fixed build's stops at 0.
OJ_UNIT_TEST(RigidBody, SlowBrakeStops) {
    oj::BodyDef def;
    def.mass = 100.0f;
    def.inertia = {1.0f, 1.0f, 1.0f, 0.0f};
    def.localGravity = 0.0f;
    auto after = [&](float speed) {
        oj::RigidBody b(def, {});
        b.setLocalVelocity({0.0f, 0.0f, speed, 0.0f});
        b.step(1.0f / 60.0f);
        return b.localVelocity[2];
    };
    const float slow = after(0.01f);
    const float faster = after(0.2f);
    check(slow == 0.0f && std::fabs(faster - (0.2f - 2.0f / 60.0f)) < 1e-4f,
          "rigid body (fixed): the slow body's brake stops at 0, not past it");
}

// The fixed build: a hit between two bodies (impulseWith) kept in the world's pushes while they are set, each body's
// share the momentum it took (the online cars' contacts told to the other machines); none kept without them.
OJ_UNIT_TEST(RigidBody, HitKeptForTheOnlineContacts) {
    const auto parsed = oj::parseDynamics("[BeginCar]\nMass 1000\nInertia 1 1 1\nCentreOfGravity 0 0 0\n[EndCar]\n");
    const oj::BodyDef def = parsed ? parsed->body : oj::BodyDef{};
    oj::PhysicsWorld world;
    oj::RigidBody a(def, {0.0f, 0.0f, 0.0f, 0.0f});
    oj::RigidBody b(def, {1.0f, 0.0f, 0.0f, 0.0f});
    a.world = &world;
    b.world = &world;
    std::vector<oj::BodyPush> pushes;
    a.impulseWith(b, -5.0f, {0.5f, 0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 0.0f}, {}, {}, 0.0f);
    const bool noneWithout = pushes.empty();
    world.pushes = &pushes;
    const oj::Vec4 before = b.momentum;
    a.impulseWith(b, -5.0f, {0.5f, 0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 0.0f}, {}, {}, 0.0f);
    world.pushes = nullptr;
    const bool kept = pushes.size() == 2 && pushes[0].to == &a && pushes[0].from == &b && pushes[1].to == &b &&
                      pushes[1].from == &a &&
                      std::fabs(pushes[1].linear[0] - (b.momentum[0] - before[0])) < 1e-3f &&
                      std::fabs(pushes[0].linear[0] + pushes[1].linear[0]) < 1e-3f && pushes[1].linear[0] > 0.0f;
    check(parsed && noneWithout && kept, "rigid body (fixed): a hit kept for both bodies, its momenta");
}
