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
