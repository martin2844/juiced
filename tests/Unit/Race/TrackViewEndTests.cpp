// Unit tests: the race's end (Game/Race/TrackView, TrackView::endRace).

#include "Framework/Test.h"

#include "Game/Race/TrackView/TrackView.h"

#include <memory>

namespace {

using oj::test::check;

}  // namespace

// The camera's rain, splash and screen smears are let go as the race ends, before its scene and 3D world: kept on the
// players till the next race's were made, a wet race's smears were freed after their particle system (the crash
// loading the race after a wet one, 0x1356C1 in the first 1.01.2). Each is watched through a stand-in owner.
OJ_UNIT_TEST(TrackViewEnd, CameraWeatherDropped) {
    auto view = std::make_unique<app::TrackView>();
    app::RacePlayer& player = view->first();
    std::weak_ptr<int> watched[3];
    std::shared_ptr<oj::scene::Emitter>* held[3] = {&player.cameraRain, &player.cameraSplash, &player.cameraBlobs};
    for (int k = 0; k < 3; ++k) {
        auto owner = std::make_shared<int>(k);
        watched[k] = owner;
        *held[k] = std::shared_ptr<oj::scene::Emitter>(owner, nullptr);
    }
    view->endRace();
    check(watched[0].expired() && watched[1].expired() && watched[2].expired(),
          "the camera's rain, splash and smears let go by the race's end");
}
