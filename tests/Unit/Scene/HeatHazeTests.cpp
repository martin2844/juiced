// Unit tests: the fixed build's heat haze (Game/Scene/HeatHaze.h), JuicedConfigExt's HeatHaze checks: the plume a
// moving car leaves, the particles' size at an exhaust, the quads along the plume, the distortion map's mip bias, the
// strength by the car's motion, the emission by the time, the emitter at one pipe or between two, the quads drawn.

#include "Framework/Test.h"

#include "Game/Scene/HeatHaze.h"

#include <cmath>

namespace {

using oj::test::check;
namespace haze = oj::scene::haze;

bool near(float a, float b, float within = 1e-4f) {
    return std::fabs(a - b) < within;
}

}  // namespace

// Particles a moving car leaves behind keep their rise and spread along the ground by their age (a trail); a car
// dropping or climbing past its particles keeps them near the exhaust.
OJ_UNIT_TEST(HeatHaze, Plume) {
    const haze::Vec3 up = {0.0f, 1.0f, 0.0f};
    const haze::Vec3 behind = haze::plume({0.0f, 40.0f, -1000.0f}, up, 3000.0f);  // 108 km/h
    const haze::Vec3 younger = haze::plume({0.0f, 10.0f, -300.0f}, up, 3000.0f);
    const haze::Vec3 beside = haze::plume({3.0f, 20.0f, -4.0f}, up, 100.0f);
    check(behind[1] > 36.0f && behind[1] < 40.0f && behind[2] < -65.0f && behind[2] > -85.0f && younger[2] > -35.0f &&
              younger[2] < -20.0f,
          "10 m behind at 108 km/h: within the trail, its rise kept; 3 m behind nearer");
    check(near(beside[1], 20.0f, 0.5f) && near(beside[0], 3.0f, 0.2f) && near(beside[2], -4.0f, 0.3f),
          "a slow car's particle near the pipe where it is");
    const haze::Vec3 dropped = haze::plume({0.0f, 300.0f, -20.0f}, up, 0.0f);
    const haze::Vec3 climbed = haze::plume({0.0f, -300.0f, -20.0f}, up, 0.0f);
    check(dropped[1] > 70.0f && dropped[1] <= 80.0f && climbed[1] < -10.0f && climbed[1] >= -15.0f,
          "3 m above or below: within 80 above, 15 below");
}

OJ_UNIT_TEST(HeatHaze, ExhaustScale) {
    check(haze::exhaustScale(0.0f, 1.0f) == 0.2f && haze::exhaustScale(40.0f, 0.0f) < 0.28f &&
              haze::exhaustScale(40.0f, 1.0f) > 0.38f && haze::exhaustScale(1000.0f, 0.0f) > 0.49f &&
              haze::exhaustScale(1000.0f, 0.0f) <= 0.5f,
          "a thin stream off the throttle, a cone on it");
    check(near(haze::boost(0.0f, 0.0f), 1.0f) && near(haze::boost(1.0f, 4167.0f), 6.0f), "the shift x1 .. x3 x2");
}

// The quads along the plume in the camera plane; a plume seen end on as the game faces it.
OJ_UNIT_TEST(HeatHaze, Orient) {
    const haze::Vec3 a = {1.0f, 0.0f, 0.0f}, b = {0.0f, 1.0f, 0.0f};
    haze::Vec3 along{}, across{};
    const float endOn = haze::orient({0.0f, 0.0f, 5.0f}, a, b, along, across);
    check(endOn == 0.0f && near(along[1], 1.0f) && near(across[0], 1.0f), "end on: the game's facing");
    const float sideways = haze::orient({3.0f, 0.0f, 0.0f}, a, b, along, across);
    check(sideways == 1.0f && near(std::fabs(along[0]), 1.0f) && near(along[1], 0.0f) && near(std::fabs(across[1]), 1.0f),
          "sideways: along x");
    const float down = haze::orient({0.0f, -2.0f, 0.0f}, a, b, along, across);
    check(down == 1.0f && near(along[1], 1.0f), "down: along y");
}

OJ_UNIT_TEST(HeatHaze, MipBias) {
    check(haze::mipBias(256.0f) == 2.0f && near(haze::mipBias(16.0f), 1.0f) && near(haze::mipBias(4.0f), -1.0f),
          "256 px +2, 16 px +1, 4 px -1 (8 texels across kept)");
    check(haze::visible(0.1f) == 1.0f && haze::visible(0.5f) == 0.0f && haze::visible(0.425f) > 0.0f &&
              haze::visible(0.425f) < 1.0f,
          "a quad of half the screen gone, faded over the last 30 %");
}

// The haze of a standing car at a quarter of its strength, of a moving one (18 km/h and more) at full.
OJ_UNIT_TEST(HeatHaze, Motion) {
    haze::Motion m;
    haze::follow(m, {100.0f, 0.0f, 200.0f}, 0.0f, 1000.0);
    const float first = m.strength;
    haze::follow(m, {100.0f, 0.0f, 200.0f}, 0.0f, 1000.5);
    const float standing = m.strength;
    haze::follow(m, {1100.0f, 0.0f, 200.0f}, 1.0f, 1001.5);  // 10 m in a second
    const float moving = m.strength;
    const float speed = m.speed;
    haze::follow(m, {1100.0f, 0.0f, 200.0f}, 1.0f, 1002.5);
    const float stopped = m.strength;
    haze::follow(m, {100.0f, 0.0f, 200.0f}, 1.0f, 1002.51);  // another view of the frame
    check(near(first, 0.25f) && near(standing, 0.25f) && near(moving, 1.0f) && near(stopped, 0.25f) &&
              near(m.strength, 0.25f) && near(speed, 1000.0f),
          "standing 0.25, moving 1, stopped 0.25, another view of the same frame left alone");
}

// At 144 frames a second EMIT_INTERVAL of the game's timer 0.576 (0.2 s); a standing car's own interval 0.1 s.
OJ_UNIT_TEST(HeatHaze, EmitByTime) {
    check(near(haze::emitInterval(1.0f / 144.0f), 0.576f, 1e-3f) && haze::emitInterval(0.0f) == 0.0f,
          "0.576 at 144 frames a second, none for no step");
    check(near(haze::ownInterval(0.0f), 0.1f) && near(haze::ownInterval(500.0f), 0.2f) &&
              near(haze::ownInterval(250.0f), 0.15f),
          "0.1 s standing .. 0.2 s from 18 km/h");
}

// The emitter at the end of the pipes (35 along the -z of their nodes): between the two of a double exhaust, at the
// one of a single; none without a pipe.
OJ_UNIT_TEST(HeatHaze, PlaceAtExhaust) {
    const oj::Mat34 root = oj::identity34();
    oj::Mat34 left = oj::identity34(), right = oj::identity34();
    left[3] = -50.0f;
    left[7] = 20.0f;
    left[11] = -200.0f;
    right[3] = 50.0f;
    right[7] = 20.0f;
    right[11] = -200.0f;
    haze::Vec3 local{};
    const haze::Exhaust twin = haze::placeAtExhaust(root, &left, &right, 0.5f, local);
    check(twin.valid && twin.twin && near(local[0], 0.0f) && near(local[1], 20.0f) && near(local[2], -235.0f) &&
              near(twin.half[0], 50.0f) && near(twin.axis[2], -1.0f) && near(twin.throttle, 0.5f),
          "a double exhaust: between the pipes' ends, half 50 to each");
    const haze::Exhaust one = haze::placeAtExhaust(root, &left, nullptr, 2.0f, local);
    check(one.valid && !one.twin && near(local[0], -50.0f) && near(local[2], -235.0f) && one.throttle == 1.0f,
          "one pipe: at its end, the throttle at most 1");
    check(!haze::placeAtExhaust(root, nullptr, nullptr, 0.0f, local).valid, "no pipe: the game's place");
}

// A quad of the game away from an exhaust put back where its particle is (+ view x size), its alpha kept on the screen;
// more quads than the haze places: none.
OJ_UNIT_TEST(HeatHaze, Quads) {
    const haze::Axes axes{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
    std::vector<haze::Vertex> game = {{-10.0f, -10.0f, 500.0f, 0x80FFFFFFu, 0.0f, 0.0f},
                                      {10.0f, -10.0f, 500.0f, 0x80FFFFFFu, 1.0f, 0.0f},
                                      {-10.0f, 10.0f, 500.0f, 0x80FFFFFFu, 0.0f, 1.0f},
                                      {10.0f, 10.0f, 500.0f, 0x80FFFFFFu, 1.0f, 1.0f}};
    // A clip matrix of x / z, y / z: the quad 20 / 500 of the screen.
    const float toClip[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0};
    const auto drawn = haze::hazeQuads(game, axes, haze::Exhaust{}, 1.0f, 0.0f, 0.0f, toClip, 640.0f);
    check(drawn.size() == 4 && near(drawn[0].z, 490.0f) && near(drawn[0].x, -10.0f) && (drawn[0].colour >> 24) == 0x80 &&
              drawn[0].u == 0.0f && drawn[3].u == 1.0f,
          "put back 10 toward the camera, its alpha and u v kept");
    std::vector<haze::Vertex> many(4 * (haze::kMaxParticles + 1), game[0]);
    check(haze::hazeQuads(many, axes, haze::Exhaust{}, 1.0f, 0.0f, 0.0f, toClip, 640.0f).empty(),
          "more than 256 quads: none (the game's drawn)");
}
