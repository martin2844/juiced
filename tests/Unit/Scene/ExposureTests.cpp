// Unit tests: LIGHT FLARES' exposure (Game/Scene/PostEffects.h, stepExposure): the stock's rises to 2 in a dark
// picture, the fixed build's CUSTOM to 1.5; both dim a bright one alike.

#include "Framework/Test.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Game/Scene/PostEffects.h"

#include <cmath>

namespace {

using oj::test::check;

float settle(float high, float mean, float most) {
    oj::scene::ExposureStep e;
    for (int i = 0; i < 400; ++i) e = oj::scene::stepExposure(e, high, mean, most);
    return e.exposure;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-3f;
}

}  // namespace

// A night's measure (the high end 0.4, the mean 0.17: the user's East_Angel_R2): the stock's 2, CUSTOM's 1.5; a bright
// day's (0.95, 0.6): 0.8 / 0.845 both; a white one: 0.8, the least; a step at most 0.02.
OJ_UNIT_TEST(Exposure, StockAndCustom) {
    check(near(settle(0.4f, 0.17f, 2.0f), 2.0f) && near(settle(0.4f, 0.17f, oj::kCustomExposure), 1.5f),
          "a dark picture: the stock's 2, CUSTOM's 1.5");
    check(near(settle(0.95f, 0.6f, 2.0f), 0.8f / 0.845f) &&
              near(settle(0.95f, 0.6f, oj::kCustomExposure), 0.8f / 0.845f) && near(settle(1.0f, 1.0f, 2.0f), 0.8f),
          "a bright picture: dimmed alike; a white one to the least, 0.8");
    const oj::scene::ExposureStep one = oj::scene::stepExposure({}, 0.0f, 0.0f, 2.0f);
    check(near(one.exposure, 1.02f) && near(one.speed, 0.02f), "a step at most 0.02");
}
