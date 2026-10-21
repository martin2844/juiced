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
