// Unit tests: the cars' flames as lights (Game/Scene/FlameLights.h): a flame's light at its node, warm, by its share,
// flickering within its bounds; its colour, the warm orange with the flame's own (the car's neon) mixed in; the pass's
// view of the camera, the lights nearest it first, eight at most.

#include "Framework/Test.h"

#include "Game/Scene/AmbientOcclusion.h"
#include "Game/Scene/FlameLights.h"
#include "Render/Camera.h"

#include <cmath>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

OJ_UNIT_TEST(FlameLights, OneFlame) {
    oj::Mat34 node{};
    node[3] = 10.0f;
    node[7] = 20.0f;
    node[11] = 30.0f;
    const std::array<float, 3> colour = oj::scene::kFlameColour;
    bool within = true;
    for (int i = 0; i < 50; ++i) {
        const oj::render::PointLight l = oj::scene::flameLight(node, colour, 0.5f, static_cast<float>(i) * 0.013f, 3);
        const float full = oj::scene::kFlameColour[0] * oj::scene::kFlameStrength * 0.5f;
        within = within && l.colour[0] >= full * 0.749f && l.colour[0] <= full * 1.001f;
    }
    const oj::render::PointLight l = oj::scene::flameLight(node, colour, 1.0f, 0.0f, 0);
    check(l.place[0] == 10.0f && l.place[1] == 20.0f && l.place[2] == 30.0f && l.reach == oj::scene::kFlameReach &&
              l.falloff == oj::scene::kFlameFalloff,
          "at the node, its reach and falloff");
    check(l.colour[0] > l.colour[1] && l.colour[1] > l.colour[2], "a warm light");
    check(within, "by its share, flickering between three quarters and all of it");
}

// The light's colour: the warm orange alone at share 0 (the light as it was), the flame's own mixed in by the share,
// at the orange's brightness so the light's strength does not move; the flame's own is the car's neon, each of r, g
// and b at least 96 of 255 (a car that never had a neon fitted keeps the game's default, 0xFFFF0000: red).
OJ_UNIT_TEST(FlameLights, Colour) {
    const auto luminance = [](const std::array<float, 3>& c) {
        return 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
    };
    oj::Car plain;
    const std::array<float, 3> off = oj::scene::flameLightColour(plain, 0.0f);
    check(off == oj::scene::kFlameColour, "share 0: the orange alone, the light as it was");
    const std::array<float, 3> red = oj::scene::flameLightColour(plain, 1.0f);
    check(red[0] > red[1] && red[1] == red[2] && std::fabs(luminance(red) - luminance(oj::scene::kFlameColour)) < 1e-3f,
          "the game's default neon (0xFFFF0000): a red-tinted light at the orange's brightness");
    oj::Car none;
    none.neon = 0;
    const std::array<float, 3> grey = oj::scene::flameLightColour(none, 1.0f);
    check(grey[0] == grey[1] && grey[1] == grey[2] && std::fabs(luminance(grey) - luminance(oj::scene::kFlameColour)) < 1e-3f,
          "every channel 0: all three at 96, a grey light at the orange's brightness");
    oj::Car green;
    green.neon = 0x0000FF00u;  // r 0, g 255, b 0
    const std::array<float, 3> tinted = oj::scene::flameLightColour(green, 1.0f);
    check(tinted[1] > tinted[0] && tinted[1] > tinted[2], "a green neon: a green light");
    check(std::fabs(luminance(tinted) - luminance(oj::scene::kFlameColour)) < 1e-3f,
          "at the orange's brightness: the light's strength unmoved");
    const std::array<float, 3> half = oj::scene::flameLightColour(green, 0.5f);
    check(half[1] > oj::scene::kFlameColour[1] && half[1] < tinted[1] && half[0] < oj::scene::kFlameColour[0],
          "half way: the orange with the green mixed in");
}

OJ_UNIT_TEST(FlameLights, View) {
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(0.75) * 2.0), static_cast<float>(std::atan(0.5) * 2.0), false);
    camera.resetMatrices();
    camera.setPosition(0.0f, 0.0f, 0.0f);
    camera.invalidate();
    camera.update();
    std::vector<oj::render::PointLight> lights;
    for (int i = 0; i < 10; ++i) {
        oj::render::PointLight l;
        l.place = {0.0f, 0.0f, static_cast<float>(1000 - 100 * i)};
        lights.push_back(l);
    }
    const oj::render::PointLightView v = oj::scene::flameView(camera, lights);
    check(std::fabs(v.tanX - 0.75f) < 1e-4f && std::fabs(v.tanY - 0.5f) < 1e-4f &&
              v.range == oj::scene::kOcclusionRange,
          "the camera's half angles, the depth's range");
    check(v.count == 8 && v.lights[0].place[2] == 100.0f && v.lights[7].place[2] == 800.0f,
          "the nearest eight, nearest first");
}
