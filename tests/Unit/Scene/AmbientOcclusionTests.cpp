// Unit tests: WORLD LIGHT HI's ambient occlusion (Game/Scene/AmbientOcclusion.h, Render/Renderer3d.h): the depth's
// size by the view, the pass's view of a camera, the materials the view's depth draws; WORLD LIGHT ULTRA's lights
// reflected in the road by night: the view of a camera, the ground marked in the depth.

#include "Framework/Test.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/ExeTables.h"
#include "Game/Scene/AmbientOcclusion.h"
#include "Render/Camera.h"
#include "Render/Program.h"
#include "Render/Renderer3d.h"

#include <array>
#include <cmath>

namespace {

using oj::test::check;

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

}  // namespace

// Half the view either way, 16 at least; the camera's half angles' tangents and the constants.
OJ_UNIT_TEST(AmbientOcclusion, DepthAndView) {
    check(oj::scene::occlusionSize(1920) == 960 && oj::scene::occlusionSize(1079) == 539 &&
              oj::scene::occlusionSize(20) == 16,
          "half the view, 16 at least");
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(0.75) * 2.0), static_cast<float>(std::atan(0.5) * 2.0), false);
    const oj::render::AmbientOcclusionView v = oj::scene::occlusionView(camera);
    check(near(v.tanX, 0.75f) && near(v.tanY, 0.5f) && v.range == oj::scene::kOcclusionRange &&
              v.radius == oj::scene::kOcclusionRadius && v.strength == oj::scene::kOcclusionStrength,
          "the half angles' tangents of the camera, the range, the radius and the strength");
    check(v.foliageRadius == oj::scene::kFoliageRadius && v.foliageStrength == oj::scene::kFoliageStrength &&
              v.foliageFadeFrom == oj::scene::kFoliageFadeFrom && v.foliageFadeLength == oj::scene::kFoliageFadeLength,
          "the trees' leaves: their radius, strength and fade");
}

// The reflections' view of a camera: its half angles, the range and the reach, the world's up in its frame (a camera
// pitched down sees the up turned toward it), the road's wetness; no street lamp's streak yet.
OJ_UNIT_TEST(LightReflections, View) {
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(0.75) * 2.0), static_cast<float>(std::atan(0.5) * 2.0), false);
    camera.update();
    const oj::render::LightReflectionView level = oj::scene::reflectionView(camera, 0.0f);
    check(near(level.tanX, 0.75f) && near(level.tanY, 0.5f) && level.range == oj::scene::kOcclusionRange &&
              level.reach == oj::scene::kReflectionReach,
          "the half angles' tangents, the depth's range, the rays' reach");
    check(near(level.up[0], 0.0f) && near(level.up[1], 1.0f) && near(level.up[2], 0.0f),
          "a level camera: the world's up its own");
    oj::rotateX(camera.local, 0.3f);
    camera.invalidate();
    camera.update();
    const oj::render::LightReflectionView pitched = oj::scene::reflectionView(camera, 0.0f);
    const float length = std::sqrt(pitched.up[0] * pitched.up[0] + pitched.up[1] * pitched.up[1] +
                                   pitched.up[2] * pitched.up[2]);
    check(near(length, 1.0f) && std::fabs(pitched.up[2]) > 0.2f && pitched.up[1] < 1.0f,
          "a pitched camera: the up turned along its view, still unit");
    const oj::render::LightReflectionView wet = oj::scene::reflectionView(camera, 1.0f);
    check(level.wetness == 0.0f && wet.wetness == 1.0f && wet.streakCount == 0, "dry and wet; no streak yet");
}

// A street lamp's streak in the wet road: its mirror point below its foot on the view, a width and a length toward the
// view; nearer, wider; none for a lamp behind the camera, at no strength or past the reach.
OJ_UNIT_TEST(LightReflections, LampStreak) {
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(0.75) * 2.0), static_cast<float>(std::atan(0.5) * 2.0), false);
    camera.resetMatrices();
    camera.setPosition(0.0f, 150.0f, 0.0f);
    camera.invalidate();
    camera.update();
    std::array<float, 8> far{};
    std::array<float, 8> close{};
    const std::array<float, 3>& warm = oj::scene::kStreakColour;
    check(oj::scene::lampStreak(camera, {300.0f, 900.0f, 6000.0f, 0.0f}, 0.0f, 1.0f, warm, far) &&
              oj::scene::lampStreak(camera, {300.0f, 900.0f, 2000.0f, 0.0f}, 0.0f, 1.0f, warm, close),
          "lamps ahead have streaks");
    check(far[0] > 0.5f && far[1] > far[7] && far[1] > 0.5f && far[2] > 0.0f && far[3] > 0.0f,
          "right of the middle, the mirror point below the foot and the horizon, a width and a length");
    check(close[2] > far[2] && close[1] > far[1], "nearer: wider and lower on the view");
    check(near(far[4] / far[6], oj::scene::kStreakColour[0] / oj::scene::kStreakColour[2]) &&
              far[4] < close[4] && close[4] <= oj::scene::kStreakStrength * oj::scene::kStreakColour[0],
          "a warm white, fading with the distance");
    std::array<float, 8> red{};
    check(oj::scene::lampStreak(camera, {300.0f, 900.0f, 2000.0f, 0.0f}, 0.0f, 1.0f, {1.0f, 0.1f, 0.05f}, red) &&
              red[4] > 0.0f && near(red[5] / red[4], 0.1f) && near(red[6] / red[4], 0.05f),
          "a car's light in its own colour (a brake light red)");
    std::array<float, 8> none{};
    check(!oj::scene::lampStreak(camera, {300.0f, 900.0f, -2000.0f, 0.0f}, 0.0f, 1.0f, warm, none) &&
              !oj::scene::lampStreak(camera, {300.0f, 900.0f, 2000.0f, 0.0f}, 0.0f, 0.0f, warm, none) &&
              !oj::scene::lampStreak(camera, {300.0f, 900.0f, oj::scene::kStreakReach + 100.0f, 0.0f}, 0.0f, 1.0f,
                                     warm, none),
          "none behind the camera, at no strength, past the reach");
}

// The ground the view's depth marks for the reflections: the road, its lines, the water, the sea; not a wall or a car.
OJ_UNIT_TEST(LightReflections, ReflectingGround) {
    using oj::render::Renderer3d;
    check(Renderer3d::isReflectingGround(oj::kTarmac) && Renderer3d::isReflectingGround(oj::kLineTexturedOpaque) &&
              Renderer3d::isReflectingGround(oj::kWater) && Renderer3d::isReflectingGround(oj::kSea),
          "the road, its lines, the water and the sea");
    check(!Renderer3d::isReflectingGround(oj::kBuilding) && !Renderer3d::isReflectingGround(oj::kCarBody) &&
              !Renderer3d::isReflectingGround(oj::kStandardOpaque),
          "not the walls nor the cars");
    check(Renderer3d::kGroundMark > 3.5f && Renderer3d::kGroundMark - Renderer3d::kFoliageMark > 1.0f,
          "the ground's mark apart from the depth (0 .. 1) and the leaves' (negative)");
    check(Renderer3d::isWater(oj::kWater) && Renderer3d::isWater(oj::kSea) && !Renderer3d::isWater(oj::kTarmac) &&
              !Renderer3d::isWater(oj::kLineTexturedOpaque),
          "the water among them: the puddles and the sea, not the road nor its lines");
    check(Renderer3d::kWaterMark - Renderer3d::kGroundMark > 1.5f &&
              Renderer3d::kCarMark - Renderer3d::kWaterMark > 1.5f,
          "the water's mark apart from the road's and the cars' (each with its depth of 0 .. 1)");
    const oj::render::LightReflectionView v;
    check(v.asphalt == 0.3f && v.water == 1.9f,
          "the asphalt's reflections weaker, the water's stronger (the user's, 2026-10-29)");
}

// The shadow pass draws the casters (the cars' bodies, the buildings); with `ground` the road, the water and the sea
// too; never the sky or glass; the alpha tested only with the cutout shaders (AmbientLit's leaves among them, never
// drawn whole).
OJ_UNIT_TEST(AmbientOcclusion, DepthMaterials) {
    oj::test::NullRenderDevice device;
    oj::render::ProgramContext programs(device, 0);
    oj::render::Renderer3d r(device, programs);
    check(r.shadowPassDraws(oj::kCarBody) && r.shadowPassDraws(oj::kBuilding) && !r.shadowPassDraws(oj::kTarmac),
          "the headlights' shadows: the casters, not the road");
    r.shadowShaders.ground = true;
    check(r.shadowPassDraws(oj::kTarmac) && r.shadowPassDraws(oj::kWater) && r.shadowPassDraws(oj::kSea) &&
              r.shadowPassDraws(oj::kCarBody) && !r.shadowPassDraws(oj::kSky) &&
              !r.shadowPassDraws(oj::kCarGlass) && !r.shadowPassDraws(oj::kStandardAlphaTest),
          "the view's depth: the ground too; not the sky, glass, the alpha tested");
    check(!r.shadowPassDraws(oj::kAmbientLit) && !r.shadowPassDraws(oj::kTreeStandard),
          "without the cutout shaders no leaves, not even AmbientLit's cards");
    r.shadowShaders.cutoutPixel = 1;
    check(r.shadowPassDraws(oj::kStandardAlphaTest) && r.shadowPassDraws(oj::kAmbientLit) &&
              r.shadowPassDraws(oj::kTreeStandard) && r.shadowPassDraws(oj::kTreeSprite) &&
              r.shadowPassDraws(oj::kVertColAlphaTest) && r.shadowPassDraws(oj::kUnlitATest) &&
              !r.shadowPassDraws(oj::kStandardAlphaBlend) && !r.shadowPassDraws(oj::kCrowdSprite),
          "with them the alpha tested classes; not the blended ones nor the sprites");
    check(oj::render::Renderer3d::isFoliage(oj::kTreeStandard) && oj::render::Renderer3d::isFoliage(oj::kTreeSprite) &&
              oj::render::Renderer3d::isFoliage(oj::kAmbientLit) &&
              !oj::render::Renderer3d::isFoliage(oj::kStandardAlphaTest) &&
              !oj::render::Renderer3d::isFoliage(oj::kVertColAlphaTest) && !oj::render::Renderer3d::isFoliage(oj::kTarmac),
          "the trees' classes marked as leaves; the other alpha tested ones (fences, signs) not");
}
