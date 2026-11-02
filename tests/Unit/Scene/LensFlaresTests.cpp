// Unit tests: the colour of a flare texture's glow (Game/Scene/LensFlares.h, glowColour): a brake light's red, a
// headlight's white, the dark and clear pixels left out, white for nothing to weigh.

#include "Framework/Test.h"

#include "Game/Scene/LensFlares.h"

#include <cmath>
#include <cstring>

namespace {

using oj::test::check;

bool close(float a, float b) { return std::fabs(a - b) < 0.02f; }

// A 4 x 4 A8R8G8B8 image: half of it `glow`, half dark and clear.
oj::Ddt image(uint32_t glow) {
    oj::Ddt d;
    d.format = oj::Ddt::kA8R8G8B8;
    d.width = 4;
    d.height = 4;
    d.pixels.resize(4 * 4 * 4);
    for (size_t i = 0; i < 16; ++i) {
        const uint32_t p = i % 2 == 0 ? glow : 0x00000000u;
        std::memcpy(d.pixels.data() + 4 * i, &p, 4);
    }
    return d;
}

}  // namespace

OJ_UNIT_TEST(LensFlares, GlowColour) {
    const std::array<float, 3> red = oj::scene::glowColour(image(0xFFFF2010u));
    check(close(red[0], 1.0f) && close(red[1], 32.0f / 255.0f) && close(red[2], 16.0f / 255.0f),
          "a brake light's red, its strongest channel 1");
    const std::array<float, 3> white = oj::scene::glowColour(image(0xFFC8C8C8u));
    check(close(white[0], 1.0f) && close(white[1], 1.0f) && close(white[2], 1.0f), "a headlight's white");
    const std::array<float, 3> none = oj::scene::glowColour(image(0x00000000u));
    check(none[0] == 1.0f && none[1] == 1.0f && none[2] == 1.0f, "nothing to weigh: white");
    check(oj::scene::glowColour(oj::Ddt{})[0] == 1.0f, "no image: white");
}
