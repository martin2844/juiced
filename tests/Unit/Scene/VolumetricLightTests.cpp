// Unit tests: WORLD LIGHT ULTRA's light in the night's haze (Game/Scene/VolumetricLight.h): a street lamp's cone, a
// headlight's beam, the point the lamps are looked for about, the pass's view of a camera and of the lights.

#include "Framework/Test.h"

#include "Game/Scene/AmbientOcclusion.h"
#include "Game/Scene/VolumetricLight.h"
#include "Render/Camera.h"

#include <cmath>
#include <vector>

namespace {

using oj::test::check;

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

}  // namespace

// A lamp's cone straight down from its place, as wide across as up, its colour by its strength, its steps; fewer steps
// for a far lamp; a headlight's beam along the frame's beam, its side and up over the narrowed tangents, its colour by
// the strength.
OJ_UNIT_TEST(VolumetricLight, LampsAndBeams) {
    const oj::render::VolumetricLight lamp = oj::scene::lampCone({100.0f, 900.0f, -50.0f, 0.0f}, 1.0f, 8);
    check(lamp.place[0] == 100.0f && lamp.place[1] == 900.0f && lamp.place[2] == -50.0f && lamp.axis[1] == -1.0f &&
              lamp.reach == oj::scene::kLampConeReach && lamp.falloff == oj::scene::kLampConeFalloff,
          "a lamp: at its place, down, its reach and falloff");
    check(near(lamp.side[0], 1.0f / oj::scene::kLampConeTan) && near(lamp.up[2], 1.0f / oj::scene::kLampConeTan) &&
              lamp.colour == oj::scene::kLampConeColour,
          "a round cone, a warm white");
    const oj::render::VolumetricLight faint = oj::scene::lampCone({0.0f, 900.0f, 0.0f, 0.0f}, 0.25f, 4);
    check(near(faint.colour[1], oj::scene::kLampConeColour[1] * 0.25f) && faint.steps == 4 && lamp.steps == 8,
          "its colour by its strength, its steps");
    oj::Mat34 eye{};
    eye[3] = 100.0f;
    check(oj::scene::hazeSteps(eye, {100.0f, 900.0f, oj::scene::kHazeNear - 1000.0f, 0.0f}) == 8 &&
              oj::scene::hazeSteps(eye, {100.0f, 0.0f, oj::scene::kHazeNear + 10.0f, 0.0f}) ==
                  oj::scene::kHazeFarSteps,
          "eight steps for a lamp near the camera, fewer past kHazeNear");
    oj::scene::HeadlightFrame f;
    f.place = {1.0f, 2.0f, 3.0f};
    f.beam = {0.0f, 0.0f, 1.0f};
    f.side = {1.0f, 0.0f, 0.0f};
    f.up = {0.0f, 1.0f, 0.0f};
    const oj::render::VolumetricLight beam = oj::scene::headlightBeam(f, 0.5f);
    check(beam.place == f.place && beam.axis == f.beam && near(beam.side[0], 1.0f / oj::scene::kBeamTanWidth) &&
              near(beam.up[1], 1.0f / oj::scene::kBeamTanHeight) && beam.reach == oj::scene::kBeamReach,
          "a beam: along the frame, narrowed, its reach");
    check(near(beam.colour[2], oj::scene::kBeamColour[2] * 0.5f), "its colour by the strength");
    check(beam.forward == oj::scene::kBeamForward && beam.forward > 0.0f && lamp.forward == 0.0f,
          "a beam scattered forward (seen from behind its car weaker), a lamp's cone evenly");
}

// The lamps looked for kHazeLampsAhead along the camera's look; the view: the camera's half angles and rows, the haze
// of the road's wetness, the lights (24 at most).
OJ_UNIT_TEST(VolumetricLight, View) {
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(0.75) * 2.0), static_cast<float>(std::atan(0.5) * 2.0), false);
    camera.resetMatrices();
    camera.setPosition(10.0f, 20.0f, 30.0f);
    camera.invalidate();
    camera.update();
    const oj::Vec4 centre = oj::scene::hazeCentre(camera.world());
    check(near(centre[0], 10.0f) && near(centre[1], 20.0f) && near(centre[2], 30.0f + oj::scene::kHazeLampsAhead),
          "the lamps looked for ahead of the camera");
    std::vector<oj::render::VolumetricLight> lights(30, oj::scene::lampCone({0.0f, 900.0f, 0.0f, 0.0f}, 1.0f, 8));
    const oj::render::VolumetricLightView dry = oj::scene::volumetricView(camera, lights, 0.0f);
    const oj::render::VolumetricLightView wet = oj::scene::volumetricView(camera, lights, 1.0f);
    check(near(dry.tanX, 0.75f) && near(dry.tanY, 0.5f) && dry.range == oj::scene::kOcclusionRange &&
              dry.camera[0][3] == 10.0f && dry.camera[2][3] == 30.0f,
          "the half angles' tangents, the depth's range, the camera's rows");
    check(near(dry.density, oj::scene::kHazeDry) && near(wet.density, oj::scene::kHazeWet) && dry.metre == 100.0f,
          "a thicker haze in the wet");
    check(dry.count == 24 && dry.lights[23].place[1] == 900.0f, "24 lights at most");
}
