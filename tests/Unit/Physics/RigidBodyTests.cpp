// Unit tests: RigidBody.

#include "Framework/Test.h"

#include "Core/Base/Matrix.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Physics/RigidBody.h"

#include <cmath>

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
