// Unit tests: the fixed build's brightness (JuicedConfigExt's Shared/BrightnessCurve, drawn over each frame by
// Render/D3D9 with DeviceDesc::brightness): the curve of the stock's gamma ramp (0x00512920) as the adapter puts it out.

#include "Framework/Test.h"

#include <BrightnessCurve.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The stock's ramp for each brightness by 0.05 (pow(i x 256, g) rounded within 0 .. 65535, its high byte put out):
// the drawn curve within a step of it at every level.
OJ_UNIT_TEST(Brightness, TheStockRampsCurve) {
    int largest = 0;
    for (int b = 0; b <= 20; ++b) {
        const float brightness = static_cast<float>(b) / 20.0f;
        const double g = BrightnessCurve::Gamma(brightness);
        for (int i = 0; i < 256; ++i) {
            const double v = std::clamp(std::nearbyint(std::pow(static_cast<double>(i) * 256.0, g)), 0.0, 65535.0);
            const int stock = static_cast<int>(v) >> 8;
            const int drawn = static_cast<int>(
                std::lround(BrightnessCurve::Apply(static_cast<float>(i) / 255.0f, brightness) * 255.0f));
            largest = std::max(largest, std::abs(stock - drawn));
        }
    }
    check(largest <= 1, "within a step of the stock's ramp (largest " + std::to_string(largest) + ")");
}

// The middle draws nothing (the straight line); the ends: the brightest white from about the level 0.46, the darkest
// white at 0.44.
OJ_UNIT_TEST(Brightness, TheMiddleAndTheEnds) {
    check(BrightnessCurve::Neutral(0.5f) && !BrightnessCurve::Neutral(0.45f) && !BrightnessCurve::Neutral(0.55f),
          "only the middle is neutral");
    bool straight = true;
    for (int i = 0; i <= 10; ++i) {
        const float level = static_cast<float>(i) / 10.0f;
        straight = straight && std::fabs(BrightnessCurve::Apply(level, 0.5f) - level) < 1e-6f;
    }
    check(straight, "the middle: each level as it is");
    check(BrightnessCurve::Apply(0.47f, 1.0f) == 1.0f && BrightnessCurve::Apply(0.44f, 1.0f) < 1.0f,
          "the brightest: white from about 0.46");
    check(std::fabs(BrightnessCurve::Apply(1.0f, 0.0f) - 0.4355f) < 0.001f, "the darkest: white at 0.44");
}
