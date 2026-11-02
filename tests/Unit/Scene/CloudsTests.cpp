// Unit tests: the procedural clouds' view (Game/Scene/Clouds.h): the world's cloud rows, the eye, toward the sun and a
// step toward it in the map's coordinates, the wind within its tile and moving on, the colours, the map's light by the
// set's shadows; the volume's view and the reflections'; the sky's own clouds by their files' and objects' names.

#include "Framework/Test.h"

#include "Game/Scene/Clouds.h"
#include "Game/Scene/LightSets.h"

#include <cmath>

namespace {

using oj::test::check;

bool close(float a, float b) { return std::fabs(a - b) < 1e-4f; }

}  // namespace

OJ_UNIT_TEST(Clouds, View) {
    const float s = 1.0f / oj::scene::kCloudTile;
    const std::array<std::array<float, 4>, 2> rows{{{s, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, s, 0.0f}}};
    oj::Mat34 eye{};
    eye[3] = 100.0f;
    eye[7] = 200.0f;
    eye[11] = 300.0f;
    const oj::Vec4 light{0.0f, -1.0f, -1.0f, 0.0f};  // toward the ground: the sun 45 degrees up toward +z
    const oj::scene::LightSet& day = oj::scene::lightSet(oj::render::LightCase::DayDry);
    const oj::render::CloudView v =
        oj::scene::cloudView(rows, eye, light, {255.0f, 204.0f, 102.0f, 0.0f}, {51.0f, 51.0f, 102.0f, 0.0f},
                             0xFF377EBE, false, 10000.0, day, false);
    check(v.project == rows && v.eye[0] == 100.0f && v.eye[1] == 200.0f && v.eye[2] == 300.0f &&
              v.height == oj::scene::kCloudHeight && close(v.cover, day.cloudCover),
          "the world's rows, the eye, the layer's height, the set's cover");
    const float half = std::sqrt(0.5f);
    check(close(v.toSun[0], 0.0f) && close(v.toSun[1], half) && close(v.toSun[2], half), "toward the sun, against the light");
    check(close(v.sunStep[0], 0.0f) && close(v.sunStep[1], oj::scene::kCloudSunStep * s),
          "a step toward the sun across the ground in the map's coordinates");
    check(close(v.sun[0], 1.0f) && close(v.sun[1], 0.8f) && close(v.sun[2], 0.4f) &&
              close(v.ambient[2], 0.4f * 1.2f + 0.04f),
          "the sun's light, the clouds' shade over the sky's ambient");
    bool within = true;
    for (float w : v.wind) within = within && w >= 0.0f && w < 1.0f;
    const oj::render::CloudView later =
        oj::scene::cloudView(rows, eye, light, {255.0f, 204.0f, 102.0f, 0.0f}, {}, 0xFF377EBE, false, 20000.0, day,
                             false);
    check(within && later.wind[0] != v.wind[0] && later.wind[2] != v.wind[2],
          "the wind's offsets within their tile, moving on with its way");
    const oj::render::CloudView night = oj::scene::cloudView(
        rows, eye, light, {}, {}, 0x0D0912, true, 0.0, oj::scene::lightSet(oj::render::LightCase::NightDry), false);
    check(close(v.clearLight, oj::scene::kCloudClear) && close(v.cloudLight, oj::scene::kCloudShadow) &&
              close(night.clearLight, oj::scene::kCloudMean) && close(night.cloudLight, oj::scene::kCloudMean),
          "the map's light: a dry day's shadows whole, by night even");
    // The clouds' shade: by day the atmosphere's ambient at the stock's own share (the same values either way), by
    // night a night atmosphere's ambient is brighter than a noon one's (90, 80, 90 against 73, 73, 62), so it takes a
    // small share and the sky's own light (the zenith colour) the rest — the user, 2026-10-29: HI's night sky was too
    // bright. The night here is the night's own colours with the noon's ambient, the worst of both.
    const oj::render::CloudView nightAmbient =
        oj::scene::cloudView(rows, eye, light, {}, {73.0f, 73.0f, 62.0f, 0.0f}, 0x0D0912, true, 0.0, day, false);
    const oj::render::CloudView dayAmbient =
        oj::scene::cloudView(rows, eye, light, {}, {73.0f, 73.0f, 62.0f, 0.0f}, 0xFF377EBE, false, 0.0, day, false);
    check(close(nightAmbient.ambient[0], 73.0f / 255.0f * 0.35f + 13.0f / 255.0f * 1.2f + 0.04f) &&
              close(dayAmbient.ambient[0], 73.0f / 255.0f * 1.2f + 0.04f) &&
              nightAmbient.ambient[0] < dayAmbient.ambient[0] * 0.75f,
          "the clouds' shade: the sky's own light by night, the ambient's own share by day");
}

OJ_UNIT_TEST(Clouds, Volume) {
    const float s = 1.0f / oj::scene::kCloudWindow;
    const std::array<std::array<float, 4>, 2> rows{{{s, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, s, 0.0f}}};
    const oj::Mat34 eye{};
    const oj::Vec4 light{0.0f, -1.0f, -1.0f, 0.0f};
    const oj::scene::LightSet& day = oj::scene::lightSet(oj::render::LightCase::DayDry);
    const oj::render::CloudView layer = oj::scene::cloudView(rows, eye, light, {}, {}, 0xFF377EBE, false, 10000.0, day, false);
    const oj::render::CloudView v = oj::scene::cloudView(rows, eye, light, {}, {}, 0xFF377EBE, false, 10000.0, day, true);
    check(!layer.volume && v.volume && v.height == oj::scene::kCloudHeight && v.top == oj::scene::kCloudTop &&
              v.steps == oj::scene::kCloudSteps && v.lightSteps == oj::scene::kCloudLightSteps &&
              close(v.shapeScale * oj::scene::kCloudShapes, 1.0f),
          "the volume from the layer's height to its top, the main view's steps and those toward the sun");
    bool within = true;
    for (float w : v.shapeDrift) within = within && w >= 0.0f && w < 1.0f;
    for (float w : v.erosionDrift) within = within && w >= 0.0f && w < 1.0f;
    const oj::render::CloudView later = oj::scene::cloudView(rows, eye, light, {}, {}, 0xFF377EBE, false, 20000.0, day, true);
    check(within && later.shapeDrift[0] != v.shapeDrift[0] && later.wind[0] != v.wind[0] &&
              later.erosionDrift[2] != v.erosionDrift[2],
          "the weather, the shapes and the details moved by the wind within their tiles");
    const oj::render::CloudView reflected = oj::scene::reflectionView(v);
    check(reflected.volume && reflected.steps == oj::scene::kCloudReflectionSteps && reflected.lightSteps == 0.0f &&
              reflected.shapeDrift == v.shapeDrift && !reflected.screen,
          "the reflections: the same clouds, fewer steps, none toward the sun, marched in their own draws");
}

OJ_UNIT_TEST(Clouds, SkyClouds) {
    check(oj::scene::skyCloud("sky.dlh", "donna_cloudEVE2") && oj::scene::skyCloud("sky_wet.dlh", "donna_clouddayw5") &&
              oj::scene::skyCloud("anc_sky.dlh", "cloud3") && oj::scene::skyCloud("EAI_sky01.dlh", "Cloud3"),
          "the sky files' clouds");
    check(!oj::scene::skyCloud("sky.dlh", "donna_cloudmoon") &&
              !oj::scene::skyCloud("sky_wet.dlh", "donna_cloudlightning1") &&
              !oj::scene::skyCloud("sky.dlh", "stars04") && !oj::scene::skyCloud("sky_wet.dlh", "rainbow"),
          "the moon, the lightning, the stars and the rainbow stay");
    check(!oj::scene::skyCloud("DAC_Dyn_obj_day.dlh", "cloud_sign"), "a cloud of another file stays");
}

// The wind by the race's weather and time of day: the rain 144 km/h, morning and evening 100, noon and night 70.
OJ_UNIT_TEST(Clouds, Wind) {
    using oj::scene::cloudWind;
    check(cloudWind(2, true) == oj::scene::kCloudWindRain && cloudWind(4, true) == oj::scene::kCloudWindRain,
          "the rain's, day or night");
    check(cloudWind(1, false) == oj::scene::kCloudWindDusk && cloudWind(3, false) == oj::scene::kCloudWindDusk,
          "morning and evening");
    check(cloudWind(2, false) == oj::scene::kCloudWindNoon && cloudWind(4, false) == oj::scene::kCloudWindNoon &&
              cloudWind(0, false) == oj::scene::kCloudWindNoon,
          "noon and night (and any other)");
    check(close(oj::scene::kCloudWindRain * 0.036f, 144.0f) && close(oj::scene::kCloudWindDusk * 0.036f, 100.008f) &&
              close(oj::scene::kCloudWindNoon * 0.036f, 69.984f),
          "144, 100 and 70 km/h (world units of a centimetre)");
}
