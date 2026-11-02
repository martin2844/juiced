// Integration tests: the race's camera (Game/Race/RaceCamera.h) on a route of the game's.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Tracks/Track.h"
#include "Game/Race/RaceCamera.h"
#include "Render/Camera.h"

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using oj::test::check;

// How far the sweep before the start goes in a second and a half with one car on the route's first grid point, the
// sweep's places given.
float sweepDistance(const std::vector<oj::Vec4>& places) {
    oj::scene::CameraTarget target;
    target.matrix[3] = places[0][0];
    target.matrix[7] = places[0][1];
    target.matrix[11] = places[0][2];
    oj::scene::CameraRace race;
    race.places = &places;
    race.count = 6.0f;
    oj::scene::RaceCamera view;
    oj::render::Camera camera;
    view.take(oj::scene::RaceCamera::kBeforeStart, true, target, race, camera);
    const oj::Vec4 from = view.position();
    for (int k = 0; k < 3; ++k) view.update(target, race, 0.5f, camera);
    const oj::Vec4 to = view.position();
    if (view.mode() != oj::scene::RaceCamera::kBeforeStart) return -1.0f;
    const float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

// The sweep before the start runs across the route's grid points (GRIDPOINT, the route's +0x4C and +0xAC read by
// 0x00405300), all of them whatever the cars on the grid: with one car (the garage's test track) the camera still
// travels across the front of the grid; taken from the cars' places alone it stood still.
OJ_INTEGRATION_TEST(RaceCamera, SweepCrossesTheGrid) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const auto spl = oj::test::gameFiles().read(std::filesystem::path("tracks") / "Angel SuperSpeedway" /
                                                "angel superspeedway_speedway_r1.spl");
    if (!spl) {
        check(false, "Speedway_R1's .spl read");
        return;
    }
    const oj::Spl lines = oj::parseSpl(std::string(reinterpret_cast<const char*>(spl->data()), spl->size()));
    const std::vector<oj::Vec4> places = oj::scene::gridPlaces(lines);
    if (lines.grid.size() < 2 || places.size() != lines.grid.size()) {
        check(false, "Speedway_R1: its grid points, all of them");
        return;
    }
    check(sweepDistance(places) > 100.0f, "the route's grid points: the camera travels across the grid");
    check(sweepDistance({places[0]}) == 0.0f, "the one car's place alone: the camera stands still");
}

// The view behind the car widens with the car's acceleration only while it moves forward (the fixed build's; the stock
// widened it braking in reverse too: the car slowed, accelerating forward at 15 m/s²); the speed's part as before.
OJ_INTEGRATION_TEST(RaceCamera, ReverseBrakingKeepsTheView) {
    const auto fovAfter = [](float forward) {
        oj::scene::CameraTarget target;
        target.forwardAcceleration = 15.0f;
        target.localVelocity[2] = forward;
        oj::scene::CameraRace race;
        oj::scene::RaceCamera view;
        oj::render::Camera camera;
        view.take(oj::scene::RaceCamera::kChase, true, target, race, camera);
        for (int frame = 0; frame < 300; ++frame) view.update(target, race, 1.0f / 60.0f, camera);
        return view.fov();
    };
    const float reversing = fovAfter(-3.0f);
    const float going = fovAfter(3.0f);
    check(reversing == 1.0f, "braking in reverse: the stock's view (" + std::to_string(reversing) + ")");
    check(going > 1.3f, "accelerating forward: the view widened (" + std::to_string(going) + ")");
}

// A restart's fly-by: 0x0041FE50 steps the race's camera in the fly-by's view 9 too, whose frame clears the scene
// camera's post effects and keeps the speed's blur off (views 8 .. 12). The blur of the race before the restart (the
// nitro at full) is gone from the fly-by's first frame, and eased away by its end, so the view behind the car after
// it has none (the fixed build's port drew the old blur through the fly-by: the race's camera was not stepped). The
// fixed build's fly-by has the depth of field of the settings (the TV cameras'), its point 1 m over the car.
OJ_INTEGRATION_TEST(RaceCamera, FlyByClearsTheBlur) {
    oj::scene::CameraTarget target;
    target.nitro = true;
    target.nitroAmount = 1.0f;
    oj::scene::CameraRace race;
    oj::scene::RaceCamera view;
    oj::render::Camera camera;
    view.take(oj::scene::RaceCamera::kChase, true, target, race, camera);
    for (int frame = 0; frame < 300; ++frame) view.update(target, race, 1.0f / 60.0f, camera);
    const float before = camera.blurEdge;
    target.nitro = false;  // the restart: the car still on the grid
    target.nitroAmount = 0.0f;
    target.matrix[3] = 1000.0f;
    target.matrix[7] = 20.0f;
    target.matrix[11] = -500.0f;
    view.flyByFrame(target, race, 1.0f / 60.0f, camera);
    const bool cleared = camera.blurEdge == 0.0f && camera.blurMiddle == 0.0f;
    const bool focused = camera.focus[0] == 1000.0f && camera.focus[1] == 120.0f && camera.focus[2] == -500.0f;
    for (int frame = 0; frame < 600; ++frame) view.flyByFrame(target, race, 1.0f / 60.0f, camera);
    view.take(oj::scene::RaceCamera::kChase, false, target, race, camera);
    check(before > 0.9f && cleared, "the fly-by's first frame: the race's blur off (" + std::to_string(before) + ")");
    check(focused, "the fly-by: the camera's point 1 m over the car");
    check(camera.blurEdge < 0.01f && camera.blurMiddle == 0.0f,
          "the view behind the car after the fly-by: no old blur (" + std::to_string(camera.blurEdge) + ")");
}
