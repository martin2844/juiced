// Unit tests: HEADLIGHTS ULTRA (Game/Scene/HeadlightShadows.h, Headlights.h, Render/Renderer3d.h): the maps' sizes
// and tiers by the car, the far maps' turns, the night shaders' shadow constants, a light's frame as the constants give
// it, the material classes that cast a shadow.

#include "Framework/Test.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/ExeTables.h"
#include "Game/Scene/HeadlightShadows.h"
#include "Game/Scene/Headlights.h"
#include "Render/Renderer3d.h"

#include <array>
#include <cmath>
#include <vector>

namespace {

using oj::test::check;
using oj::scene::ShadowTier;

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

}  // namespace

// The first player's car's map 1536 x 768, a car within 40 m of it 768 x 384, farther 384 x 192; the far maps drawn
// one frame in three each (by the car), a near one each frame, a far one the first time at once.
OJ_UNIT_TEST(HeadlightShadows, TiersAndTurns) {
    using oj::scene::shadowDue;
    using oj::scene::shadowSize;
    using oj::scene::shadowTier;
    check(shadowTier(true, 99999.0f) == ShadowTier::Own && shadowTier(false, 3999.0f) == ShadowTier::Near &&
              shadowTier(false, 4001.0f) == ShadowTier::Far,
          "own, within 40 m, farther");
    check(shadowSize(ShadowTier::Own).width == 1536 && shadowSize(ShadowTier::Own).height == 768 &&
              shadowSize(ShadowTier::Near).width == 768 && shadowSize(ShadowTier::Far).height == 192,
          "1536 x 768, 768 x 384, 384 x 192");
    int farDue = 0;
    for (uint64_t frame = 0; frame < 9; ++frame) farDue += shadowDue(ShadowTier::Far, 2, frame, true) ? 1 : 0;
    bool nearEach = true;
    for (uint64_t frame = 0; frame < 9; ++frame) nearEach = nearEach && shadowDue(ShadowTier::Near, 2, frame, true);
    check(farDue == 3 && nearEach && shadowDue(ShadowTier::Far, 2, 1, false),
          "far 3 frames in 9, near each, a new far map at once");
    check(shadowDue(ShadowTier::Far, 0, 3, true) != shadowDue(ShadowTier::Far, 1, 3, true),
          "two far cars take different frames");
}

// A light's frame: between the flares, its beam, side and up unit and at right angles, the beam a little down from the
// car's forward; the constants give the same.
OJ_UNIT_TEST(HeadlightShadows, LightFrame) {
    oj::scene::CarHeadlights car;
    car.root = oj::identity34();
    car.root[3] = 100.0f;
    car.right = car.root;
    car.right[3] = 150.0f;
    car.left = car.root;
    car.left[3] = 50.0f;
    car.shown = true;
    const oj::scene::HeadlightFrame f = oj::scene::headlightFrame(car);
    const auto dot = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    };
    check(near(f.place[0], 100.0f) && near(dot(f.beam, f.beam), 1.0f) && near(dot(f.up, f.up), 1.0f) &&
              near(dot(f.beam, f.up), 0.0f) && near(dot(f.beam, f.side), 0.0f) && f.beam[1] < 0.0f &&
              f.beam[2] > 0.99f,
          "between the flares; beam, side, up unit, at right angles; the beam forward, a little down");
    const auto c = oj::scene::headlightConstants({car}, oj::scene::kHeadlightStrength);
    check(near(c.constants[2][0], f.beam[0]) && near(c.constants[2][1], f.beam[1]) &&
              near(c.constants[4][2], f.up[2] / oj::scene::kHeadlightTanHeight) &&
              near(c.constants[1][3], 2.5f * 0.75f),
          "the constants of the same frame (the up over tan 10), at 75 % strength");
}

// The casters drawn whole: the cars' bodies, the buildings, the walls and fences (opaque); not the road, the sky, the
// cars' glass, the alpha tested ones (trees, wire fences, AmbientLit's leaves: cut out by the depth passes' cutout
// shaders) nor a class past the table.
OJ_UNIT_TEST(HeadlightShadows, Casters) {
    using oj::render::Renderer3d;
    check(Renderer3d::castsShadow(oj::kCarBody) && Renderer3d::castsShadow(oj::kBuilding) &&
              Renderer3d::castsShadow(oj::kStandardOpaque) && Renderer3d::castsShadow(oj::kSpecMapOpaque),
          "car bodies, buildings, opaque walls cast");
    check(!Renderer3d::castsShadow(oj::kTarmac) && !Renderer3d::castsShadow(oj::kSky) &&
              !Renderer3d::castsShadow(oj::kCarGlass) && !Renderer3d::castsShadow(oj::kStandardAlphaTest) &&
              !Renderer3d::castsShadow(oj::kAmbientLit) && Renderer3d::cutsOut(oj::kAmbientLit) &&
              !Renderer3d::castsShadow(999),
          "the road, the sky, glass, alpha tested, an unknown class do not");
}
