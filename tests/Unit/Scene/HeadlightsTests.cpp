// Unit tests: the fixed build's headlights (Game/Scene/Headlights.h): a car's light between its flares, its beam and
// cone along the car, its intensity by the strength; the cars without their headlights left out; six lights at most.

#include "Framework/Test.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Game/Scene/Headlights.h"
#include "Render/Program.h"

#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

using oj::test::check;

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

// The bool constants set, each call's values.
class BoolsDevice : public oj::test::NullRenderDevice {
public:
    void setPixelShaderConstantB(uint32_t start, const int32_t* data, uint32_t count) override {
        if (start == oj::scene::kHeadlightBool) calls.emplace_back(data, data + count);
    }
    std::vector<std::vector<int32_t>> calls;
};

// A car at the place given, its root the identity's rotation (forward z, side x, up y), its flares 50 cm to each side.
oj::scene::CarHeadlights car(float x, float y, float z, bool shown) {
    oj::scene::CarHeadlights c;
    c.root = oj::identity34();
    c.root[3] = x;
    c.root[7] = y;
    c.root[11] = z;
    c.right = c.root;
    c.right[3] = x + 50.0f;
    c.left = c.root;
    c.left[3] = x - 50.0f;
    c.shown = shown;
    return c;
}

}  // namespace

OJ_UNIT_TEST(Headlights, OneCar) {
    const auto c = oj::scene::headlightConstants({car(100.0f, 60.0f, 200.0f, true)}, 1.0f);
    check(c.count == 1 && c.on[0] == 1 && c.on[1] == 0, "one light on");
    check(near(c.constants[0][0], 1.0f) && near(c.constants[0][1], 0.95f) && near(c.constants[0][2], 0.85f),
          "c196: the warm white");
    check(near(c.constants[1][0], 100.0f) && near(c.constants[1][1], 60.0f) && near(c.constants[1][2], 200.0f) &&
              near(c.constants[1][3], 2.5f),
          "c197: between the flares, intensity 2.5");
    const float t = HeadlightBeam::kTilt;
    const float n = std::sqrt(1.0f + t * t);
    check(near(c.constants[2][0], 0.0f) && near(c.constants[2][1], -t / n) && near(c.constants[2][2], 1.0f / n) &&
              near(c.constants[2][3] * 10000.0f, 1.0f - HeadlightBeam::kShadowBias),
          "c198: the beam forward, 6 degrees down, the depth's scale over 100 m less the bias");
    check(near(c.constants[3][0], 1.0f / HeadlightBeam::kTanHalfWidth) && c.constants[3][3] == 0.0f,
          "c199: the side over tan 45, no normal's offset without a map");
    const float h = HeadlightBeam::kTanHalfHeight;
    check(near(c.constants[4][1], 1.0f / (n * h)) && near(c.constants[4][2], t / (n * h)) && c.constants[4][3] == 0.0f,
          "c200: the beam's up over tan 10, no shadow map");
}

// A car's light at its level (coming on or going out in a tunnel's day): its intensity times it.
OJ_UNIT_TEST(Headlights, Level) {
    std::vector<oj::scene::CarHeadlights> cars = {car(0.0f, 0.0f, 0.0f, true)};
    const float full = oj::scene::headlightConstants(cars, 1.0f).constants[1][3];
    cars[0].level = 0.25f;
    const float quarter = oj::scene::headlightConstants(cars, 1.0f).constants[1][3];
    check(full > 0.0f && near(quarter, full * 0.25f), "a quarter of the light at a quarter");
}

// HEADLIGHTS ULTRA: a light's shadow map in its constants, its texel width (c200.w) and the normal's offset per unit of
// the distance, 1.5 texels' width at it (c199.w); a light without a map none.
OJ_UNIT_TEST(Headlights, ShadowMapInTheLight) {
    std::vector<oj::scene::CarHeadlights> cars = {car(0.0f, 0.0f, 0.0f, true), car(500.0f, 0.0f, 0.0f, true)};
    const std::array<float, oj::scene::kHeadlights> texels = {1.0f / 1536.0f, 0.0f, 0, 0, 0, 0};
    const auto c = oj::scene::headlightConstants(cars, 1.0f, &texels);
    check(near(c.constants[4][3] * 1536.0f, 1.0f) &&
              near(c.constants[3][3] * 1536.0f, 1.5f * 2.0f * HeadlightBeam::kTanHalfWidth),
          "the first light: its map's texel width, the normal's offset of 1.5 texels");
    check(c.constants[8][3] == 0.0f && c.constants[7][3] == 0.0f, "the second: no map, no offset");
}

OJ_UNIT_TEST(Headlights, SeveralCars) {
    std::vector<oj::scene::CarHeadlights> cars;
    cars.push_back(car(0.0f, 0.0f, 0.0f, false));
    for (int i = 0; i < 8; ++i) cars.push_back(car(static_cast<float>(i), 0.0f, 0.0f, true));
    const auto c = oj::scene::headlightConstants(cars, 2.0f);
    check(c.count == 6 && c.on[5] == 1, "six lights at most");
    check(near(c.constants[1][0], 0.0f) && near(c.constants[5][0], 1.0f) && near(c.constants[1][3], 5.0f),
          "the car without its headlights left out; at 200 % intensity 5");
    check(oj::scene::headlightConstants({car(0.0f, 0.0f, 0.0f, false)}, 1.0f).count == 0, "none shown: no light");
}

// The frame's constants: on, the lights; turned off, every light off once (the replaced night shaders read them until
// their programs are made again: a light left where it was, the user 2026-10-27), then nothing.
OJ_UNIT_TEST(Headlights, TurnedOffClearsTheLights) {
    BoolsDevice device;
    const std::vector<oj::scene::CarHeadlights> cars = {car(0.0f, 0.0f, 0.0f, true)};
    const bool on = oj::scene::setHeadlights(device, cars, 1.0f, false);
    const bool lit = device.calls.size() == 1 && device.calls[0].size() == 6 && device.calls[0][0] == 1;
    const bool off = oj::scene::setHeadlights(device, {}, 0.0f, on);
    const bool cleared = device.calls.size() == 2 && device.calls[1] == std::vector<int32_t>(6, 0);
    const bool still = oj::scene::setHeadlights(device, {}, 0.0f, off);
    check(on && lit, "on: the car's light on");
    check(!off && cleared, "turned off: every light off");
    check(!still && device.calls.size() == 2, "off since: nothing set");
}

// REFLECTIONS ULTRA: the cars' paint takes the headlights' constants in the effects' shared CarHeadlight (the colour,
// then each light, a light off all 0); not lit, or the stock's headlights (the strength 0), none. A shared value no
// effect has is not made.
OJ_UNIT_TEST(Headlights, OnTheCarsPaint) {
    oj::test::NullRenderDevice device;
    oj::render::ProgramContext programs(device, 0);
    const std::vector<oj::scene::CarHeadlights> cars = {car(100.0f, 60.0f, 200.0f, true)};
    const float one = 1.0f;
    const bool missing = !programs.setShared("CarHeadlight", &one, sizeof one) && programs.shared.empty();
    std::vector<uint8_t>& value = programs.shared["CarHeadlight"];
    const auto c = oj::scene::headlightConstants(cars, 1.0f);
    const auto zero = [&value] {
        for (uint8_t b : value) {
            if (b != 0) return false;
        }
        return true;
    };
    value.assign(sizeof c.constants, 0xFF);
    oj::scene::setCarHeadlights(programs, cars, 1.0f, true);
    const bool lit = std::memcmp(value.data(), c.constants.data(), sizeof c.constants) == 0;
    value.assign(sizeof c.constants, 0xFF);
    oj::scene::setCarHeadlights(programs, cars, 0.0f, true);
    const bool stock = zero();
    value.assign(sizeof c.constants, 0xFF);
    oj::scene::setCarHeadlights(programs, cars, 1.0f, false);
    const bool unlit = zero();
    check(missing, "no effect with it: nothing set");
    check(lit && c.count == 1, "lit: the headlights' constants");
    check(stock && unlit, "the stock's headlights, or not lit: none");
}
