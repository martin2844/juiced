// Unit tests: the race's light cases and their sets (Game/Scene/LightSets.h, Render/LightCase.h): the case of the time
// of day, the road's wetness and the cars' lights in a tunnel, what each case draws.

#include "Framework/Test.h"

#include "Game/Scene/LightSets.h"

namespace {

using oj::render::LightCase;
using oj::test::check;

}  // namespace

// The case of a night and of the wet, a dry day's with lights shown the tunnel's; each case's place in the table.
OJ_UNIT_TEST(LightSets, Cases) {
    using oj::scene::lightCase;
    check(lightCase(false, false) == LightCase::DayDry && lightCase(false, true) == LightCase::DayWet &&
              lightCase(true, false) == LightCase::NightDry && lightCase(true, true) == LightCase::NightWet,
          "a dry day, a wet day, a dry night, a wet night");
    check(lightCase(false, false, true) == LightCase::DayTunnel && lightCase(false, true, true) == LightCase::DayWet &&
              lightCase(true, false, true) == LightCase::NightDry,
          "lights shown: a dry day's the tunnel's, the others' their own");
    for (size_t i = 0; i < oj::render::kLightCases.size(); ++i) {
        check(oj::render::lightCaseIndex(oj::render::kLightCases[i]) == i, "a case's place");
    }
}

// The sun's cascades by a dry day alone (no sun's shadow in the wet light tiles); the street lamps' shadows and the
// light in the haze by night, the lights in the road in the wet night alone; the haze thicker in the wet.
OJ_UNIT_TEST(LightSets, Passes) {
    using oj::scene::lightSet;
    const oj::scene::LightSet& dayDry = lightSet(LightCase::DayDry);
    const oj::scene::LightSet& dayWet = lightSet(LightCase::DayWet);
    const oj::scene::LightSet& nightDry = lightSet(LightCase::NightDry);
    const oj::scene::LightSet& nightWet = lightSet(LightCase::NightWet);
    const oj::scene::LightSet& dayTunnel = lightSet(LightCase::DayTunnel);
    check(dayDry.sunCascades && !dayWet.sunCascades && !nightDry.sunCascades && !nightWet.sunCascades &&
              dayTunnel.sunCascades,
          "the sun's cascades by a dry day, in a tunnel too");
    check(!dayTunnel.lampShadows && !dayTunnel.roadLights && !dayTunnel.hazeLight,
          "a tunnel's day: none of the night's");
    check(!dayDry.lampShadows && !dayWet.lampShadows && nightDry.lampShadows && nightWet.lampShadows &&
              !dayDry.hazeLight && !dayWet.hazeLight && nightDry.hazeLight && nightWet.hazeLight,
          "the street lamps' shadows and the light in the haze by night");
    check(!dayDry.roadLights && !dayWet.roadLights && !nightDry.roadLights && nightWet.roadLights,
          "the lights in the road in the wet night");
    check(nightDry.haze == 0.0f && nightWet.haze == 1.0f, "the haze thicker in the wet");
}

// The clouds: scattered by a dry day (in a tunnel's too) with the sun's shadows, an overcast sky in the rain with soft
// ones, by night no shadow and thinner.
OJ_UNIT_TEST(LightSets, Clouds) {
    using oj::scene::lightSet;
    const oj::scene::LightSet& dayDry = lightSet(LightCase::DayDry);
    const oj::scene::LightSet& dayWet = lightSet(LightCase::DayWet);
    const oj::scene::LightSet& nightDry = lightSet(LightCase::NightDry);
    const oj::scene::LightSet& nightWet = lightSet(LightCase::NightWet);
    const oj::scene::LightSet& dayTunnel = lightSet(LightCase::DayTunnel);
    check(dayDry.cloudCover < 0.6f && dayWet.cloudCover > 0.85f && nightWet.cloudCover > 0.85f &&
              dayTunnel.cloudCover == dayDry.cloudCover,
          "scattered by a dry day, overcast in the rain");
    check(dayDry.cloudShadows == 1.0f && dayWet.cloudShadows > 0.0f && dayWet.cloudShadows < 1.0f &&
              nightDry.cloudShadows == 0.0f && nightWet.cloudShadows == 0.0f && dayTunnel.cloudShadows == 1.0f,
          "the sun's shadows by a dry day, soft in the rain, none by night");
    check(nightDry.cloudOpacity < dayDry.cloudOpacity && nightWet.cloudOpacity < dayWet.cloudOpacity,
          "thinner by night");
}

// The headlights: none by a dry day (the cars' lights off), half in the daylight of the rain, all by night and in a
// tunnel.
OJ_UNIT_TEST(LightSets, Headlights) {
    using oj::scene::lightSet;
    check(lightSet(LightCase::DayDry).headlights == 0.0f && lightSet(LightCase::DayWet).headlights == 0.5f &&
              lightSet(LightCase::NightDry).headlights == 1.0f && lightSet(LightCase::NightWet).headlights == 1.0f &&
              lightSet(LightCase::DayTunnel).headlights == 1.0f,
          "none, half, all, all, all");
}

// The cars' flames' light: weak in the daylight, all of it by night.
OJ_UNIT_TEST(LightSets, Flames) {
    using oj::scene::lightSet;
    check(lightSet(LightCase::DayDry).flames < lightSet(LightCase::DayWet).flames &&
              lightSet(LightCase::DayWet).flames < lightSet(LightCase::NightDry).flames &&
              lightSet(LightCase::NightDry).flames == 1.0f && lightSet(LightCase::NightWet).flames == 1.0f,
          "a dry day's the weakest, the nights' all");
}
