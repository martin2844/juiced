// Unit tests: DriverControl.

#include "Framework/Test.h"

#include "Core/Race/DriverControl.h"

#include <cmath>
#include <cstdio>

namespace {

using oj::test::check;

}  // namespace

// The steering of 0x0041E200: without the wheel lessened at speed and reached at the steering speed; with the
// wheel at once (3000 a second), MultiTurn (sensitivity + 1) x the input, not speed sensitive shaped only.
OJ_UNIT_TEST(DriverControl, Steering) {
    oj::DriverControl c;
    c.steeringSensitivity = 0.5f;
    const float keyboard = c.steerToward(1.0f, 0.0f, 0.1f);
    c.steer = 0.0f;
    c.wheel = true;
    const float wheel = c.steerToward(1.0f, 60.0f, 0.1f);
    c.steer = 0.0f;
    c.multiTurn = true;
    const float multi = c.steerToward(0.5f, 60.0f, 0.1f);
    c.steer = 0.0f;
    c.multiTurn = false;
    c.notSpeedSensitive = true;
    const float shaped = c.steerToward(0.5f, 60.0f, 0.1f);
    std::printf("     steering: keyboard %.4f, wheel %.4f, multi-turn %.4f, not speed sensitive %.4f\n", keyboard,
                wheel, multi, shaped);
    const float lessened = ((1.0f - 134.1f * 0.0071428572f) * 0.75f + 0.25f);
    check(std::fabs(keyboard - 0.061f) < 1e-5f && std::fabs(wheel - lessened) < 1e-5f &&
              std::fabs(multi - 0.75f) < 1e-6f &&
              std::fabs(shaped - static_cast<float>(std::pow(0.5, 1.65 - 0.825 + 0.8))) < 1e-4f,
          "steering: the wheel's ways (rate 3000, MultiTurn, not speed sensitive)");
}

// The fixed build's [Wheel] exponent: the steering wheel's power in place of the game's, with the wheel speed
// sensitive (lessened) or not; the keyboard's power stays.
OJ_UNIT_TEST(DriverControl, WheelExponent) {
    oj::DriverControl c;
    c.steeringSensitivity = 0.5f;
    c.wheel = true;
    c.notSpeedSensitive = true;
    c.wheelExponent = 1.0f;
    const float linear = c.steerToward(0.5f, 60.0f, 0.1f);
    c.steer = 0.0f;
    c.notSpeedSensitive = false;
    c.wheelExponent = 2.0f;
    const float squared = c.steerToward(0.5f, 0.0f, 0.1f);
    c.steer = 0.0f;
    c.wheel = false;
    const float keyboard = c.steerToward(1.0f, 0.0f, 0.1f);
    std::printf("     wheel exponent: 1 -> %.4f, 2 -> %.4f, keyboard %.4f\n", linear, squared, keyboard);
    check(std::fabs(linear - 0.5f) < 1e-6f && std::fabs(squared - 0.25f) < 1e-6f &&
              std::fabs(keyboard - 0.061f) < 1e-5f,
          "the exponent shapes the wheel's input (1 linear, 2 squared), not the keyboard's");
}

// The fixed build's controller as on the Xbox (consoleSteering, the input the controller's): the stick as it is at the
// sensitivity's middle, lessened by 1 + 0.0023 x (speed - 4)^2 above 4 m/s, bent by the power 1 + 1.1 x (0.5 -
// sensitivity); the keys and the steering wheel keep the game's, as consoleSteering off.
OJ_UNIT_TEST(DriverControl, ConsoleSteering) {
    oj::DriverControl c;
    c.steeringSensitivity = 0.5f;
    const float still = c.steerToward(0.5f, 0.0f, 100.0f, true);
    c.steer = 0.0f;
    const float fast = c.steerToward(1.0f, 40.0f, 100.0f, true);
    c.steer = 0.0f;
    const float keys = c.steerToward(1.0f, 40.0f, 100.0f, false);
    c.steer = 0.0f;
    c.steeringSensitivity = 0.0f;
    const float soft = c.steerToward(0.5f, 0.0f, 100.0f, true);
    c.steer = 0.0f;
    c.steeringSensitivity = 0.5f;
    c.consoleSteering = false;
    const float pc = c.steerToward(0.5f, 0.0f, 100.0f, true);
    c.steer = 0.0f;
    c.consoleSteering = true;
    c.wheel = true;
    const float wheel = c.steerToward(1.0f, 40.0f, 0.1f, true);
    std::printf("     console steering: %.4f still, %.4f at 40 m/s (keys %.4f), %.4f soft, PC %.4f, wheel %.4f\n", still,
                fast, keys, soft, pc, wheel);
    const float lessened = (1.0f - 89.4f * 0.0071428572f) * 0.75f + 0.25f;
    check(still == 0.5f && std::fabs(fast - 1.0f / (36.0f * 36.0f * 0.0023f + 1.0f)) < 1e-6f &&
              std::fabs(keys - lessened) < 1e-5f,
          "the controller: the stick as it is, a quarter at 40 m/s (89 mph); the keys the game's half");
    check(std::fabs(soft - static_cast<float>(std::pow(0.5, 1.55))) < 1e-5f &&
              std::fabs(pc - static_cast<float>(std::pow(0.5, 0.62))) < 1e-5f && std::fabs(wheel - lessened) < 1e-5f,
          "the sensitivity bends it (|s|^1.55 at 0); consoleSteering off and the steering wheel the game's");
}

// The keyboard's shape of 1.01 (0x0041FF7E): |s| to the power 1.1 - 1.1 x sensitivity + 0.07 (a float; 1.00's 0.05 a
// double), reached in full at a standstill.
OJ_UNIT_TEST(DriverControl, KeyboardShape) {
    oj::DriverControl c;
    c.steeringSensitivity = 0.5f;
    const float turned = c.steerToward(0.5f, 0.0f, 100.0f);
    const auto power = static_cast<float>(static_cast<double>(1.1f - 0.5f * 1.1f) + static_cast<double>(0.07f));
    const auto expected = static_cast<float>(std::pow(0.5, static_cast<double>(power)));
    std::printf("     keyboard shape: %.6f (power %.4f)\n", turned, power);
    check(std::fabs(turned - expected) < 1e-6f && std::fabs(power - 0.62f) < 1e-6f,
          "steering: the keyboard's power 1.1 - 1.1 x sensitivity + 0.07");
}
