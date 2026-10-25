// Unit tests: NetCar.

#include "Framework/Test.h"

#include "Core/Network/NetCar.h"

#include <cmath>

namespace {

using oj::test::check;

}  // namespace

// The fixed build's smoothing of the others' cars by the frame's time: a 1/60 s frame the stock's step (0.07), two
// frames of half that time the same as one, a 144 Hz frame less (the stock's 0.07 at any frame rate).
OJ_UNIT_TEST(NetCar, SmoothStepByTime) {
    using oj::net::smoothStep;
    const float frame = smoothStep(0.07f, 1.0f / 60.0f);
    const float half = smoothStep(0.07f, 1.0f / 120.0f);
    const float twoHalves = 1.0f - (1.0f - half) * (1.0f - half);
    check(std::fabs(frame - 0.07f) < 1e-5f && std::fabs(twoHalves - frame) < 1e-5f &&
              smoothStep(0.07f, 1.0f / 144.0f) < 0.07f && smoothStep(0.07f, 0.0f) == 0.0f &&
              std::fabs(smoothStep(0.21f, 1.0f / 60.0f) - 0.21f) < 1e-5f,
          "network cars: the smoothing's step by the frame's time, the stock's at 60 frames a second");
}
