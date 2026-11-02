// Unit tests: WORLD LIGHT ULTRA's sun cascades (Game/Scene/SunShadows.h): the cascades' fit to a view, the atlas's
// cells, the pixel shaders' constants.

#include "Framework/Test.h"

#include "Game/Scene/SunShadows.h"

#include <cmath>

namespace {

using oj::test::check;

bool near(float a, float b, float tolerance = 1e-3f) {
    return std::fabs(a - b) < tolerance;
}

// A row of the constants applied to a world place.
float apply(const float* row, float x, float y, float z) {
    return row[0] * x + row[1] * y + row[2] * z + row[3];
}

}  // namespace

// A view at the origin looking along +z, the sun straight down: the cascades grow, each ahead of the last, their frame
// the sun's; the atlas's cells the quarters, half a texel in.
OJ_UNIT_TEST(SunShadows, Cascades) {
    const oj::Mat34 view = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    const auto c = oj::scene::sunCascades(view, 1.0f, 0.5625f, 15.0f, {0.0f, -1.0f, 0.0f, 0.0f});
    check(c[0].half > 0.0f && c[0].half < c[1].half && c[1].half < c[2].half && c[2].half < c[3].half,
          "each cascade wider than the last");
    check(c[0].frame[11] < c[1].frame[11] && c[1].frame[11] < c[2].frame[11] && c[2].frame[11] < c[3].frame[11],
          "each further along the view");
    check(2.0f * c[0].half / static_cast<float>(oj::scene::kSunCell) < 1.0f,
          "the first's texel under 1 cm at 90 degrees across (a cascade of 20 m: 2.2 cm)");
    check(near(c[0].frame[2], 0.0f) && near(c[0].frame[6], -1.0f) && near(c[0].frame[10], 0.0f),
          "the frame's third axis the sun's direction");
    check(std::fmod(c[0].half, 100.0f) == 0.0f, "the size to whole metres");
    // A view moved a little: the first cascade's place stays or moves by whole steps of its tests (two texels), the
    // second's by whole texels.
    const oj::Mat34 moved = {1, 0, 0, 0.37f, 0, 1, 0, 0, 0, 0, 1, 0.61f};
    const auto m = oj::scene::sunCascades(moved, 1.0f, 0.5625f, 15.0f, {0.0f, -1.0f, 0.0f, 0.0f});
    const auto steps = [&](size_t i, float per) {
        const float texel = 2.0f * c[i].half / static_cast<float>(oj::scene::kSunCell) * per;
        const float dx = (m[i].frame[3] - c[i].frame[3]) / texel, dz = (m[i].frame[11] - c[i].frame[11]) / texel;
        const float x = m[i].frame[3] / texel, z = m[i].frame[11] / texel;
        return near(dx, std::round(dx), 1e-3f) && near(dz, std::round(dz), 1e-3f) && near(x, std::round(x), 1e-3f) &&
               near(z, std::round(z), 1e-3f);
    };
    check(steps(0, oj::scene::kSunNearStep) && steps(1, 1.0f),
          "the first cascade kept to whole steps of its tests, the next to whole texels");
    const auto a = oj::scene::sunCell(0), b = oj::scene::sunCell(1), d = oj::scene::sunCell(2);
    const auto e = oj::scene::sunCell(3);
    const float half = 0.5f / static_cast<float>(oj::scene::kSunAtlas);
    check(near(a[0], 0.25f + half, 1e-6f) && near(b[0], 0.75f + half, 1e-6f) && near(d[1], 0.75f + half, 1e-6f) &&
              near(d[0], 0.25f + half, 1e-6f) && near(e[0], 0.75f + half, 1e-6f) && near(e[1], 0.75f + half, 1e-6f),
          "cascade 0 the top left quarter, 1 the top right, 2 the bottom left, 3 the bottom right");
}

// The constants map a cascade's middle to its centre (0, 0) and its depth to the sun's camera's distance behind it; a
// place one half size along the side to x 1.
OJ_UNIT_TEST(SunShadows, Constants) {
    const oj::Mat34 view = {1, 0, 0, 100, 0, 1, 0, 200, 0, 0, 1, 300};
    const auto c = oj::scene::sunCascades(view, 1.0f, 0.5625f, 15.0f, {0.3f, -0.9f, 0.2f, 0.0f});
    const auto k = oj::scene::sunConstants(c, {100.0f, 200.0f, 300.0f, 0.0f});
    for (size_t i = 0; i < c.size(); ++i) {
        const oj::Mat34& f = c[i].frame;
        const float* rows = k.data() + 12 * i;
        const float x = f[3], y = f[7], z = f[11];
        check(near(apply(rows, x, y, z), 0.0f) && near(apply(rows + 4, x, y, z), 0.0f),
              "the cascade's middle at its centre");
        check(near(apply(rows + 8, x, y, z), oj::scene::kSunBackOff / oj::scene::kSunDepthScale, 1e-5f),
              "its depth the sun's camera's distance behind it");
        const float h = c[i].half;
        check(near(apply(rows, x + f[0] * h, y + f[4] * h, z + f[8] * h), 1.0f), "half its size along the side: 1");
    }
    const float atlas = static_cast<float>(oj::scene::kSunAtlas);
    check(near(k[48], 1.0f / atlas, 1e-9f) && near(k[50], atlas) && near(k[51], 0.5f / atlas, 1e-9f) &&
              near(k[58], 0.75f + 0.5f / atlas, 1e-6f) && near(k[59], 0.75f + 0.5f / atlas, 1e-6f) &&
              near(k[60], 100.0f) && near(k[63], oj::scene::kSunNormalOffset, 1e-9f) && near(k[64], atlas) &&
              near(k[65], -0.5f) && near(k[66], oj::scene::kSunNearStep) &&
              near(k[67], 1.0f / oj::scene::kSunNearStep),
          "c188 the atlas's texel, its size and half its texel, c190.zw cascade 3's cell, c191 the view's place and "
          "the normal's offset, c192 the atlas's size, -0.5, the first cascade's step and 1 / it (the soft edge)");
}
