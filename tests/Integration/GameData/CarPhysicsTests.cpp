// Integration tests: CarPhysics.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FlatGround.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Matrix.h"
#include "Core/Base/Random.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Data/ExeTables.h"
#include "Core/Formats/Cld.h"
#include "Core/Network/NetCar.h"
#include "Core/Physics/CarBody.h"
#include "Core/Physics/Contact.h"
#include "Core/Physics/Dynamics.h"
#include "Core/Physics/PhysicsWorld.h"
#include "Core/Physics/RigidBody.h"
#include "Core/Physics/TrackCollision.h"
#include "Core/Race/Racer.h"
#include "Core/Tracks/Route.h"
#include "Core/Tracks/Track.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;
using oj::test::FlatGround;

}  // namespace

OJ_INTEGRATION_TEST(CarPhysics, EclipseDynamics) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::CarDef* eclipse = shared.defs.byModel("ECLIPSE_STOCK");
    if (eclipse == nullptr) return;
    oj::DynamicsStore& dynamics = *shared.dynamics;
    const oj::CarDynamics* d = dynamics.of(*eclipse);
    check(d != nullptr && d->torqueSpeeds.size() == 9 && d->torqueFactors.size() == 9 &&
              d->torqueSpeeds[0] == oj::productToFloat(1000.0, 0.10471976f) &&
              d->topSpeed == d->torqueSpeeds.back() && d->maxTorque == 320.0f &&
              d->reverseRatio == oj::productToFloat(-3.166, 0.6f) && d->forwardRatios.size() == 5 &&
              d->wheels.size() == 4 && d->wheels[0].brakeMu == 4000.0f && d->turbo == 1.0f &&
              d->body.mass == 1483.0f && d->body.inertia[1] == 1.0f &&
              d->body.centreOfGravity[1] == 0.32f && d->body.localGravity == 1.5f &&
              d->body.drag == 1.55f && d->body.airfoils.empty(),
          "dynamic_eclipse.txt: body, torque curve in rad/s, gears, four wheels");
}
OJ_INTEGRATION_TEST(CarPhysics, WheelLines) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    const auto& c = d->body.contacts;
    const auto& lines = d->body.collision.lines;
    const float tyreMass = 0.5f * 0.5f * 1483.0f;
    check(lines.size() == 4 && lines[3].id == 3 && lines[0].point[2] == 125.0f &&
              std::fabs(lines[0].length - 52.2f) < 1e-4f && lines[0].direction[1] < -0.99999f &&
              d->body.collision.meshCollider == "mesh\\eclipse_chass_shad.ocd" &&
              d->body.collision.meshColliderId == 4 && d->body.collision.meshCollideeMu == 0.6f &&
              c.size() == 6 && c[1].type == oj::ContactDef::Tyre && c[1].id == 1 &&
              c[1].stiffness == 72000.0f && c[1].mass == tyreMass && c[1].muRatio == 1.0f &&
              c[3].stiffness == 60000.0f && c[4].type == oj::ContactDef::Rigid && c[4].clipped &&
              c[4].clipNormal[1] == 1.0f && c[5].type == oj::ContactDef::Spring &&
              c[5].mass == 1483.0f * 0.195f && c[5].damping == 0.0f && c[5].restitution == -1.0f,
          "dynamic_eclipse.txt: wheel lines, chassis meshes, tyre, rigid and spring contacts");
}
// In neutral at full throttle the engine runs up towards the end of its curve (where its torque is
// 0); off the throttle it falls back to its first speed and the engine counts as stopped.
OJ_INTEGRATION_TEST(CarPhysics, EngineInNeutral) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    oj::CarBody car(*d, {0.0f, 10.0f, 0.0f, 0.0f});
    car.setGear(1);
    car.state[car.engineIndex] = d->torqueSpeeds.front();
    car.setState(car.state);
    car.throttle = 1.0f;
    bool running = car.state.size() == 18 && car.engineIndex == 13 && car.wheels[3].index == 17;
    for (int i = 0; i < 120; ++i) car.step(1.0f / 60.0f);
    running = running && car.engineOn && car.engineSpeed <= d->topSpeed &&
              car.engineSpeed > d->topSpeed - 0.1f;
    car.throttle = 0.0f;
    for (int i = 0; i < 300; ++i) car.step(1.0f / 60.0f);

    check(running && car.engineSpeed == d->torqueSpeeds.front() && !car.engineOn &&
              car.position[1] < 10.0f,
          "car body: the engine up to its redline in neutral and back to idle, the car falls");
}
// The tilt held (0x00489740, the fixed build's): a car pitched 54 degrees nose up or down in the air turns back
// towards level faster with the hold than without it (the stock's pitch torque never came: its pitch over pi
// squared never passed 0.3).
OJ_INTEGRATION_TEST(CarPhysics, TiltHeld) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    bool ok = true;
    for (float start : {0.95f, -0.95f}) {
        float pitch[2] = {};
        for (int held = 0; held < 2; ++held) {
            oj::CarBody body(*d, {0.0f, 100.0f, 0.0f, 0.0f});
            body.setAngles({start, 0.0f, 0.0f, 0.0f});
            oj::RaceCar car;
            car.body = &body;
            for (int i = 0; i < 30; ++i) {
                if (held != 0) oj::tiltGuard(car);
                body.step(1.0f / 60.0f);
            }
            pitch[held] = body.angles[0];
        }
        std::printf("     pitched %.2f rad in the air: after 0.5 s %.3f without the hold, %.3f with it\n", start,
                    pitch[0], pitch[1]);
        ok = ok && std::fabs(pitch[1]) < std::fabs(pitch[0]) - 0.01f;
    }
    check(ok, "car tilt (fixed): pitched past 49 degrees in the air the hold turns the car towards level");
}
// A tyre rolling backwards (0x004F7C40, the fixed build's): sliding back at 15 m/s with the wheel straight, the Magic
// Formula's longitudinal force is kept as for a wheel steered a hair (the stock's was taken off and added back: none).
OJ_INTEGRATION_TEST(CarPhysics, TyreBackwards) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    const oj::ContactDef* tyre = nullptr;
    for (const oj::ContactDef& c : d->body.contacts) {
        if (c.type == oj::ContactDef::Tyre && tyre == nullptr) tyre = &c;
    }
    auto along = [&](float steer) {
        oj::Contact c;
        c.def = tyre;
        c.normal = {0.0f, 1.0f, 0.0f, 0.0f};
        c.localNormal = {0.0f, 1.0f, 0.0f, 0.0f};
        c.velocity = {0.0f, 0.0f, -15.0f, 0.0f};
        c.load = 4000.0f;
        c.mu = 1.0f;
        c.gravity = 1.0f;
        c.gravityInverse = 1.0f;
        c.radius = 0.33f;
        c.wheelSpin = -30.0f;
        c.steer = steer;
        c.tyreForce();
        return c.friction[2];
    };
    const float straight = tyre != nullptr ? along(0.0f) : 0.0f;
    const float steered = tyre != nullptr ? along(1e-4f) : 0.0f;
    std::printf("     a tyre sliding back at 15 m/s: the longitudinal force %.1f straight, %.1f steered a hair\n",
                straight, steered);
    check(tyre != nullptr && std::fabs(straight) > 100.0f && std::fabs(straight - steered) < std::fabs(steered) * 0.01f,
          "tyre (fixed): rolling backwards with the wheel straight keeps its longitudinal force");
}
// A tyre contact's drive and the traction control (0x004F6050, 0x004F6F30; the fixed build's): by the contact's own
// wheel. The stock took the wheel at the contact's place in the step's list, so with a wheel in the air the drive slid
// along the list: a rear-driven 350Z with its front wheels up gave its rear tyres (the list's first two, the front
// wheels' places) neither the throttle nor the traction control; a front-driven Integra with its front left wheel up
// drove its rear left tyre (the front right's place) and not its front right.
OJ_INTEGRATION_TEST(CarPhysics, DriveOfTheContactsOwnWheel) {
    const oj::test::SharedGameData& g = oj::test::sharedGameData();
    if (!g.carDefsRead) return;
    // The throttle each tyre contact takes (by its wheel's ID, -1 for a wheel up) with the contacts of the wheels down
    // alone in the step's list, and whether the traction control cuts in with those contacts slipping by 2.
    struct Drive {
        float throttle[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
        bool asr = false;
    };
    const auto drive = [&](const char* model, std::initializer_list<int32_t> down) -> std::optional<Drive> {
        const oj::CarDef* def = g.defs.byModel(model);
        const oj::CarDynamics* d = def != nullptr ? g.dynamics->of(*def) : nullptr;
        if (d == nullptr) return std::nullopt;
        oj::CarDynamics withAsr = *d;
        withAsr.asr = true;
        withAsr.asrFactor = 1.0f;
        oj::ContactPools pools;
        FlatGround flat;
        oj::CarBody body(withAsr, {0.0f, 1.0f, 0.0f, 0.0f});
        body.pools = &pools;
        body.collision = &flat;
        for (const int32_t id : down) {
            const oj::ContactDef* tyre = nullptr;
            for (const oj::ContactDef& c : body.dynamics().body.contacts) {
                if (c.type == oj::ContactDef::Tyre && c.id == id) tyre = &c;
            }
            if (tyre == nullptr) return std::nullopt;
            oj::Contact& c = pools.tyre[pools.tyreUsed++];
            c.def = tyre;
            c.slip = 2.0f;
        }
        body.tyreFirst = 0;
        body.tyreCount = pools.tyreUsed;
        body.throttle = 1.0f;
        body.forces(1.0f / 60.0f);
        Drive out;
        for (size_t i = 0; i < body.tyreCount; ++i) {
            const int32_t id = body.tyre(i).def->id;
            if (id >= 0 && id < 4) out.throttle[id] = body.tyre(i).throttle;
        }
        body.setLocalVelocity({0.0f, 0.0f, 10.0f, 0.0f});
        body.asrMode = false;
        body.step(1.0f / 60.0f);
        out.asr = body.asrActive;
        return out;
    };
    const std::optional<Drive> rearDown = drive("350Z_STOCK", {2, 3});
    const std::optional<Drive> allDown = drive("350Z_STOCK", {0, 1, 2, 3});
    const std::optional<Drive> frontLeftUp = drive("INTEGRA_STOCK", {1, 2, 3});
    if (!rearDown || !allDown || !frontLeftUp) {
        check(false, "the 350Z's and the Integra's dynamics and tyre contacts");
        return;
    }
    std::printf("     350Z, front wheels up: the rear tyres' throttle %.1f %.1f, the traction control %s\n",
                rearDown->throttle[2], rearDown->throttle[3], rearDown->asr ? "on" : "off");
    std::printf("     Integra, front left up: the throttle front right %.1f, rear left %.1f, rear right %.1f\n",
                frontLeftUp->throttle[1], frontLeftUp->throttle[2], frontLeftUp->throttle[3]);
    check(rearDown->throttle[2] == 1.0f && rearDown->throttle[3] == 1.0f && rearDown->asr,
          "drive (fixed): a rear-driven car's rear tyres take the throttle and the traction control, front wheels up");
    check(allDown->throttle[0] == 0.0f && allDown->throttle[1] == 0.0f && allDown->throttle[2] == 1.0f &&
              allDown->throttle[3] == 1.0f,
          "drive: with every wheel down the rear-driven car's rear tyres alone take the throttle");
    check(frontLeftUp->throttle[1] == 1.0f && frontLeftUp->throttle[2] == 0.0f && frontLeftUp->throttle[3] == 0.0f,
          "drive (fixed): a front-driven car's front right tyre alone takes the throttle, its front left up");
}
// A turbo given back (0x004896A0, a restart after the damage took it): the stock's at 1.3 whatever the car's own; the
// fixed build's the Turbo the car was made with.
OJ_INTEGRATION_TEST(CarPhysics, TurboGivenBack) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    oj::CarDynamics fitted = *d;
    fitted.turbo = 1.4f;
    oj::CarBody body(fitted, {0.0f, 1.0f, 0.0f, 0.0f});
    body.loseTurbo();
    const float lost = body.dynamics().turbo;
    body.restoreTurbo();
    check(lost == 1.0f && body.dynamics().turbo == 1.4f,
          "turbo (fixed): given back as the car's own after it was lost");
}
// On flat ground the car settles on its springs, then drives off in first gear.
OJ_INTEGRATION_TEST(CarPhysics, FlatGround) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    {
        oj::ContactPools pools;
        FlatGround flat;
        oj::CarBody onGround(*d, {0.0f, 0.0f, 0.0f, 0.0f});
        onGround.pools = &pools;
        onGround.collision = &flat;
        onGround.setGear(1);
        onGround.state[onGround.engineIndex] = d->torqueSpeeds.front();
        onGround.setState(onGround.state);
        for (int i = 0; i < 180; ++i) {
            pools.empty();
            onGround.step(1.0f / 60.0f);
        }
        const float restY = onGround.position[1];
        // At rest the slow body's brake turns its speed of under 2 dt the other way every step.
        const bool settled = onGround.tyreCount == 4 && std::fabs(onGround.velocity[1]) < 0.04f &&
                             restY > 0.0f && restY < 0.5f;
        std::printf("     car at rest on flat ground: %.4f m/s up\n", onGround.velocity[1]);
        // The driver's throttle is set before every step (the traction control lowers it).
        onGround.setGear(2);
        for (int i = 0; i < 180; ++i) {
            onGround.throttle = 1.0f;
            pools.empty();
            onGround.step(1.0f / 60.0f);
        }
        std::printf("     car on flat ground: rest %.3f m, after 3 s in first gear %.2f m/s forward, "
                    "%.0f rpm, gear %u\n",
                    restY, onGround.localVelocity[2], onGround.engineSpeed * 9.549296f,
                    onGround.gear);
        check(settled && onGround.localVelocity[2] > 5.0f && std::fabs(onGround.position[0]) < 0.5f,
              "car body on flat ground: tyre contacts carry it, the drive takes it forward");
        // The network's state of the moving car, applied to another: as it was within the steps
        // of its numbers; carried on, it goes by its velocity.
        {
            oj::RaceCar sender;
            sender.body = &onGround;
            onGround.handbrake = 0.4f;
            onGround.steering = -0.25f;
            uint8_t state[0x40] = {};
            oj::net::carState(sender, false, true, state);
            oj::CarBody copyBody(*d, {5.0f, 3.0f, 1.0f, 0.0f});
            oj::RaceCar copy;
            copy.body = &copyBody;
            bool honked = false;
            static bool* honk = nullptr;
            honk = &honked;
            oj::net::applyState(copy, state, nullptr, [](oj::RaceCar&, bool on) { *honk = on; });
            const bool same = copyBody.position == onGround.position &&
                              copyBody.orientation == onGround.orientation &&
                              std::fabs(copyBody.localVelocity[2] - onGround.localVelocity[2]) < 0.005f &&
                              std::fabs(copyBody.engineSpeed - onGround.engineSpeed) < 0.01f &&
                              copyBody.gear == onGround.gear && copyBody.engineOn &&
                              std::fabs(copyBody.handbrake - 0.4f) < 0.004f &&
                              std::fabs(copyBody.steering + 0.25f) < 0.008f &&
                              std::fabs(copyBody.throttle - onGround.throttle * onGround.throttle) < 0.02f &&
                              honked && !copy.netPaused;
            float before[3];
            std::memcpy(before, state, 12);
            oj::net::extrapolate(state, 0.5f);
            float after[3];
            std::memcpy(after, state, 12);
            const float moved = std::sqrt((after[0] - before[0]) * (after[0] - before[0]) +
                                          (after[2] - before[2]) * (after[2] - before[2]));
            check(same && std::fabs(moved - onGround.localVelocity[2] * 0.5f) < 0.05f,
                  "network car state: made, applied to another car, carried on by its velocity");
        }
    }
}
// On Engstrom R1: dropped above the first grid place it lands on its wheel lines against the
// route's collision and drives off.
OJ_INTEGRATION_TEST(CarPhysics, EngstromR1) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    {
        const std::filesystem::path base = std::filesystem::path("tracks") / "Engstrom District";
        const auto cldBytes = game.read(base / "engstrom district_engstrom_r1.cld");
        const auto splBytes = game.read(base / "engstrom district_engstrom_r1.spl");
        std::optional<oj::Cld> cld;
        if (cldBytes) {
            const std::vector<uint8_t> cldData(reinterpret_cast<const uint8_t*>(cldBytes->data()),
                                             reinterpret_cast<const uint8_t*>(cldBytes->data()) +
                                                 cldBytes->size());
            cld = oj::parseCld(cldData);
        }
        const oj::Spl route = splBytes ? oj::parseSpl(std::string(
                                             reinterpret_cast<const char*>(splBytes->data()),
                                             splBytes->size()))
                                       : oj::Spl{};
        bool drove = false;
        if (cld && !route.grid.empty()) {
            oj::CarDynamics withShapes = *d;
            oj::loadShapes(withShapes.body, game, std::filesystem::path("cars") / "eclipse");
            const oj::Spl::GridPoint& g = route.grid[0];
            oj::ContactPools pools;
            oj::TrackCollision track(*cld);
            oj::CarBody racer(withShapes, {g.x * 0.01f, g.y * 0.01f + 0.3f, g.z * 0.01f, 0.0f});
            racer.pools = &pools;
            racer.collision = &track;
            const float h = g.heading;
            racer.state[oj::RigidBody::kOrientation + 0] = std::cos(h * 0.5f);
            racer.state[oj::RigidBody::kOrientation + 2] = std::sin(h * 0.5f);
            racer.state[racer.engineIndex] = d->torqueSpeeds.front();
            racer.setGear(1);
            racer.setState(racer.state);
            for (int i = 0; i < 120; ++i) {
                pools.empty();
                racer.step(1.0f / 60.0f);
            }
            const bool landed = racer.tyreCount == 4 && std::fabs(racer.velocity[1]) < 0.05f;
            const float startY = racer.position[1];
            racer.setGear(2);
            for (int i = 0; i < 120; ++i) {
                racer.throttle = 1.0f;
                pools.empty();
                racer.step(1.0f / 60.0f);
            }
            std::printf("     on Engstrom R1: shapes %s/%s, sphere %.1f cm, landed at %.3f m (%zu tyres), "
                        "after 2 s %.2f m/s forward\n",
                        withShapes.body.collision.colliderShape ? "yes" : "no",
                        withShapes.body.collision.collideeShape ? "yes" : "no", racer.boundsRadius,
                        startY, racer.tyreCount, racer.localVelocity[2]);
            drove = landed && racer.localVelocity[2] > 5.0f && racer.tyreCount == 4;
        }
        check(drove, "car body on Engstrom R1: lands on the route's collision and drives off");

        // The race start: settled on the grid place, then the world steps it at 60 frames a second.
        bool started = false;
        bool walled = false;
        bool rayStops = false;
        bool groundFound = false;
        if (cld && !route.grid.empty()) {
            oj::CarDynamics withShapes = *d;
            oj::loadShapes(withShapes.body, game, std::filesystem::path("cars") / "eclipse");
            oj::PhysicsWorld world(*cld);
            oj::CarBody racer(withShapes, {});
            world.add(racer);
            const oj::Spl::GridPoint& g = route.grid[0];
            int firstSteps = 75;
            oj::settleOnGrid(racer, world.pools(), {g.x, g.y, g.z, 0.0f}, g.heading, firstSteps);
            const bool still = racer.tyreCount == 4 && std::fabs(racer.velocity[1]) < 1e-6f &&
                               std::fabs(racer.angles[1] - g.heading) < 0.01f;
            racer.setGear(2);
            for (int i = 0; i < 180; ++i) {
                racer.throttle = 1.0f;
                world.update(1.0f / 60.0f);
            }
            const float along = racer.velocity[0] * std::sin(g.heading) +
                                racer.velocity[2] * std::cos(g.heading);
            std::printf("     grid start on Engstrom R1: settled at %.3f m, after 3 s %.2f m/s along the "
                        "grid's heading\n", racer.position[1], along);
            started = still && along > 10.0f && firstSteps == 50;

            // Straight on at full throttle with the automatic box: the walls turn the car back onto
            // the road where it would have left it, the chassis mesh keeps it out of them.
            racer.automatic = true;
            float lowest = racer.position[1];
            float across = 0.0f;
            for (int i = 0; i < 1200; ++i) {
                racer.throttle = 1.0f;
                world.update(1.0f / 60.0f);
                lowest = std::min(lowest, racer.position[1]);
                const float side = racer.velocity[0] * std::cos(g.heading) -
                                   racer.velocity[2] * std::sin(g.heading);
                across = std::max(across, std::fabs(side));
            }
            std::printf("     walls of Engstrom R1: 20 s straight on, lowest %.2f m, %.1f m/s across the "
                        "heading at most, at (%.1f, %.1f, %.1f)\n", lowest, across, racer.position[0],
                        racer.position[1], racer.position[2]);
            walled = lowest > g.y * 0.01f - 2.0f && across > 5.0f;

            // The camera's ray (0x005046E0): from above the first grid place, of the rays 30 m
            // round it the nearest stops at a wall of the route; straight up it meets none.
            const oj::Vec4 at{g.x, g.y + 50.0f, g.z, 0.0f};
            bool stops = false;
            float gone = 1e9f;
            for (int k = 0; k < 8; ++k) {
                const float a = g.heading + static_cast<float>(k) * 0.7853982f;
                const oj::Vec4 to{at[0] + std::sin(a) * 3000.0f, at[1], at[2] + std::cos(a) * 3000.0f,
                                  0.0f};
                oj::Vec4 wall{};
                if (!world.collision().firstWall(at, to, wall)) continue;
                stops = true;
                gone = std::min(gone, std::sqrt((wall[0] - at[0]) * (wall[0] - at[0]) +
                                                (wall[2] - at[2]) * (wall[2] - at[2])));
            }
            oj::Vec4 above{};
            const bool clear =
                !world.collision().firstWall(at, {at[0], at[1] + 2000.0f, at[2], 0.0f}, above);
            std::printf("     camera ray on Engstrom R1: the nearest wall %.0f cm from the first place\n",
                        gone);
            rayStops = stops && clear && gone < 3000.0f;

            // The ground under the grid place (0x00504630): the road a little below the place.
            float ground = 0.0f;
            groundFound = world.collision().groundBelow(g.x, g.z, ground) && ground < g.y &&
                          ground > g.y - 200.0f;
            std::printf("     ground under Engstrom R1's first place: %.1f cm (the place %.1f)\n",
                        ground, g.y);
        }
        // A world without a route (0x0053DEA0) with the ground plane of the garage's test track
        // (0x004C7DF0: (0, 0, 0), up): an Eclipse a metre above it lands on its wheels and drives off.
        bool planed = false;
        {
            oj::CarDynamics withShapes = *d;
            oj::loadShapes(withShapes.body, game, std::filesystem::path("cars") / "eclipse");
            oj::PhysicsWorld world;
            world.collision().planes.push_back({{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}});
            oj::CarBody racer(withShapes, {0.0f, 1.0f, 0.0f, 0.0f});
            world.add(racer);
            world.makeGrid();
            for (int i = 0; i < 120; ++i) world.update(1.0f / 60.0f);
            const bool landed = racer.tyreCount == 4 && racer.position[1] > 0.0f &&
                                racer.position[1] < 1.0f;
            const float restY = racer.position[1];
            racer.setGear(2);
            for (int i = 0; i < 180; ++i) {
                racer.throttle = 1.0f;
                world.update(1.0f / 60.0f);
            }
            std::printf("     a plane of a world without a route: rests at %.3f m, after 3 s %.2f m/s "
                        "forward\n", restY, racer.localVelocity[2]);
            planed = landed && racer.localVelocity[2] > 5.0f && racer.tyreCount == 4;
        }
        check(planed, "car body on the plane of a world without a route: lands and drives off");

        // The AI on Engstrom R1: an Eclipse of the AI alone (no driver's record: skill 0) takes the
        // race's count and goes round the route; one lap is done in a minute and a half.
        bool lapped = false;
        if (cld && !route.grid.empty()) {
            oj::CarDynamics withShapes = *d;
            oj::loadShapes(withShapes.body, game, std::filesystem::path("cars") / "eclipse");
            oj::PhysicsWorld world(*cld);
            oj::CarBody body(withShapes, {});
            world.add(body);
            const oj::Route aiRoute(route);
            oj::RaceRules rules;
            rules.mode = oj::RaceMode::Custom;
            rules.type = oj::RaceType::Circuit;
            oj::RaceState race(aiRoute, 1, rules);
            oj::RaceCar aiCar;
            aiCar.body = &body;
            oj::CrtRandom aiRandom;
            aiCar.revs.make(aiRandom, 0.0f);
            race.add(aiCar, aiRandom, 0.0f);
            // An effects object without effects (none read): the impacts and the motion only.
            oj::makeEffects(aiCar, oj::EffectLibrary{}, oj::SurfaceEffects{}, rules.mode, rules.type,
                            false, aiRandom, false);
            oj::startJolt(aiCar);
            race.linkNeighbours();
            race.reset(aiRandom, 0.0f);
            const oj::Spl::GridPoint& g = route.grid[0];
            int firstSteps = 75;
            oj::settleOnGrid(body, world.pools(), {g.x, g.y, g.z, 0.0f}, g.heading, firstSteps);
            aiCar.competitor->rollingStart = 0.0f;
            race.count = 6.0f;
            int respawns = 0;
            float time = 0.0f;
            for (int i = 0; i < 60 * 150 && !aiCar.competitor->finished; ++i) {
                world.update(1.0f / 60.0f);
                aiCar.node = body.modelMatrix();
                const float step = world.stepTime();
                oj::carFrame(aiCar, step, 1.0f / 60.0f, rules.type);
                oj::damageEffects(aiCar, aiRandom);
                oj::wheelEffects(aiCar, step, 1.0f / 60.0f, aiRandom, world.pools(), &race, i % 2 == 0);
                if (aiCar.ghostTime == 3.0f) ++respawns;  // put back on its line upside down
                if (aiCar.competitor->respawnAsked) {
                    oj::respawn(aiCar, rules, world.pools());
                    ++respawns;
                }
                oj::applyControls(aiCar, race, aiRandom, 1.0f / 60.0f);
                race.update(step, 1.0f / 60.0f, aiRandom);
                time = aiCar.competitor->raceTime;
            }
            const oj::Competitor& ai = *aiCar.competitor;
            std::printf("     AI on Engstrom R1: lap of %.0f m (%zu points, %zu corners), %s after %.2f s, "
                        "%d respawns, %zu rand() values\n",
                        aiRoute.lengths[0] * 0.01f, aiRoute.lines[0].size(), aiRoute.corners.size(),
                        ai.finished ? "finished" : "not finished", time, respawns,
                        static_cast<size_t>(aiRandom.draws()));
            lapped = ai.finished && respawns == 0 && time > 40.0f && time < 90.0f;
        }
        check(lapped, "AI on Engstrom R1: from the count round the route, one lap without a respawn");

        // Two cars of the world: the one of the fourth grid place drives straight into the car of
        // the first, 13 m ahead; it pushes it on instead of passing through it.
        bool pushed = false;
        if (cld && route.grid.size() > 3) {
            oj::CarDynamics withShapes = *d;
            oj::loadShapes(withShapes.body, game, std::filesystem::path("cars") / "eclipse");
            oj::PhysicsWorld world(*cld);
            oj::CarBody ahead(withShapes, {});
            oj::CarBody behind(withShapes, {});
            world.add(ahead);
            world.add(behind);
            int firstSteps = 75;
            const oj::Spl::GridPoint& g0 = route.grid[0];
            const oj::Spl::GridPoint& g3 = route.grid[3];
            oj::settleOnGrid(ahead, world.pools(), {g0.x, g0.y, g0.z, 0.0f}, g0.heading, firstSteps);
            oj::settleOnGrid(behind, world.pools(), {g3.x, g3.y, g3.z, 0.0f}, g3.heading, firstSteps);
            const float fx = std::sin(g0.heading), fz = std::cos(g0.heading);
            auto along = [&](const oj::CarBody& b) { return b.position[0] * fx + b.position[2] * fz; };
            const float start = along(ahead);
            behind.setGear(2);
            float closest = 1e9f;
            auto across = [&](const oj::CarBody& b) { return b.position[0] * fz - b.position[2] * fx; };
            for (int i = 0; i < 60 * 8; ++i) {
                behind.throttle = 1.0f;
                world.update(1.0f / 60.0f);
                const float gap = along(ahead) - along(behind);
                const float side = std::fabs(across(ahead) - across(behind));
                if (side < 1.8f) closest = std::min(closest, gap);
            }
            std::printf("     two cars on Engstrom R1: the one ahead pushed %.2f m on, %zu near bodies, "
                        "the gap at least %.2f m\n", along(ahead) - start, behind.nearBodies.size(),
                        closest);
            pushed = along(ahead) - start > 2.0f && closest > 2.0f && behind.nearBodies.size() == 1;
        }
        check(pushed, "two cars on Engstrom R1: the one behind drives into the other and pushes it");

        // A race car's impacts (0x004916F0, the fixed build's): of its two rigid contacts the first touches the other
        // car and the last has come apart, so the other body is the first's (the stock read the last's: none); a
        // change of 1000 cm/s against a body is an energy of 8000, the AI's hit 1.5 x (2 + (8000 - 400) x 0.003) =
        // 37.2, its impact that (the stock's 50, the last piece's).
        bool impacted = false;
        if (!route.grid.empty()) {
            oj::CarBody mine(*d, {});
            oj::CarBody theirs(*d, {});
            const oj::Route raceRoute(route);
            oj::RaceRules rules;
            rules.mode = oj::RaceMode::Custom;
            rules.type = oj::RaceType::Circuit;
            oj::RaceState race(raceRoute, 1, rules);
            oj::RaceCar mineCar;
            oj::RaceCar theirCar;
            mineCar.body = &mine;
            theirCar.body = &theirs;
            oj::CrtRandom random;
            mineCar.revs.make(random, 0.0f);
            theirCar.revs.make(random, 0.0f);
            race.add(mineCar, random, 0.0f);
            race.add(theirCar, random, 0.0f);
            oj::makeEffects(mineCar, oj::EffectLibrary{}, oj::SurfaceEffects{}, rules.mode, rules.type, false,
                            random, false);
            race.linkNeighbours();
            race.reset(random, 0.0f);
            mineCar.competitor->raceTime = 1.0f;
            oj::startJolt(mineCar);
            mineCar.jolt.velocity = {0.0f, 0.0f, 1000.0f, 0.0f};
            oj::ContactPools pools;
            pools.rigid[0].compression = 0.01f;
            pools.rigid[0].other = &theirs;
            pools.rigid[0].arm = {0.0f, 0.0f, 2.0f, 0.0f};
            pools.rigid[0].localNormal = {0.0f, 0.0f, 1.0f, 0.0f};
            pools.rigid[1].compression = -0.01f;
            pools.rigid[1].arm = {0.0f, 0.0f, 2.0f, 0.0f};
            pools.rigid[1].localNormal = {0.0f, 0.0f, 1.0f, 0.0f};
            pools.rigidUsed = 2;
            mine.rigidFirst = 0;
            mine.rigidCount = 2;
            oj::wheelEffects(mineCar, 1.0f / 60.0f, 1.0f / 60.0f, random, pools, &race, true);
            std::printf("     a race car's impacts: a body's contact %s, the impact %.2f\n",
                        mineCar.bodyContact ? "seen" : "not seen", mineCar.impact);
            impacted = mineCar.bodyContact && std::fabs(mineCar.impact - 37.2f) < 0.01f;
        }
        check(impacted, "a race car's impacts (fixed): the touching contact's body, the impact the hit's amount");
        check(started, "race start on Engstrom R1: the car settles on its grid place and drives off");
        check(walled, "walls of Engstrom R1: driving straight on, the car stays on the route");
        check(rayStops, "camera ray on Engstrom R1: the nearest wall between two points");
        check(groundFound, "ground ray on Engstrom R1: the road under a grid place");
    }
}
// Each saved rating was computed at single (main thread) or double precision; cars of another
// installation's data differ.
OJ_INTEGRATION_TEST(CarPhysics, SavedRatings) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::CarDefs& defs = shared.defs;
    const oj::PartCatalogue& catalogue = shared.catalogue;
    oj::DynamicsStore& dynamics = *shared.dynamics;
    const std::vector<std::pair<std::string, oj::Car>>& savedCars = oj::test::savedCars();
    size_t matched = 0;
    for (const auto& [who, saved] : savedCars) {
        oj::Car single = saved;
        single.def = oj::findCarDef(defs, saved.modelName());
        oj::Car twice = single;
        const oj::CarDynamics* d = single.def != nullptr ? dynamics.of(*single.def) : nullptr;
        oj::rateCar(single, d, catalogue, oj::X87::Single);
        oj::rateCar(twice, d, catalogue, oj::X87::Double);
        if (single.rating == saved.rating || twice.rating == saved.rating) {
            ++matched;
        }
    }
    std::printf("     ratings of the saved cars: %zu of %zu as computed\n", matched, savedCars.size());
    check(matched * 4 >= savedCars.size() * 3, "performance ratings of the saved cars (+0x88)");
}

// The brakes wear while braking (0x0048F080: 0.01 a second of the frame time), but not on the dyno's stand (the car's
// +0x19C) nor on the test track (race type 9): a dyno run, braking all along, leaves the garage's car unworn.
OJ_INTEGRATION_TEST(CarPhysics, BrakeWear) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    oj::CarBody body(*d, {0.0f, 0.0f, 0.0f, 0.0f});
    oj::Car record;
    record.brake = std::bit_cast<uint32_t>(1.0f);
    oj::RaceCar car;
    car.body = &body;
    car.car = &record;
    body.brake = 1.0f;
    const auto braked = [&](bool onStand, oj::RaceType type) {
        record.brakeWear = 0;
        car.onStand = onStand;
        for (int i = 0; i < 60; ++i) oj::carFrame(car, 1.0f / 60.0f, 1.0f / 60.0f, type);
        return std::bit_cast<float>(record.brakeWear);
    };
    const float raced = braked(false, oj::RaceType::Circuit);
    check(raced > 0.0099f && raced < 0.0101f, "a second braking in a race wears the brakes by 0.01");
    check(braked(true, oj::RaceType::Circuit) == 0.0f, "a second braking on the dyno's stand wears nothing");
    check(braked(false, oj::RaceType::TestTrack) == 0.0f, "a second braking on the test track wears nothing");
}
