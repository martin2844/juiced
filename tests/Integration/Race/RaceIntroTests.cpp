// Integration tests: the fly-by before a race (Game/Race/RaceIntro.h) with a fly-by of the game's.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Game/Race/RaceCamera.h"
#include "Game/Race/RaceIntro.h"
#include "Render/Camera.h"

namespace {

using oj::test::check;

}  // namespace

// A shot after the first comes in through the first player's director's transition (0x0041C530), not a black screen:
// switched on as the shot starts, its amount 1 - the second's time left, stopped once the second is out; the fly-by's
// end stops it too.
OJ_INTEGRATION_TEST(RaceIntro, ShotsComeInThroughTheTransition) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::scene::RaceIntro intro;
    if (!intro.load("anderson_r1")) {
        check(false, "scripts\\anderson_r1_intro.cam read");
        return;
    }
    oj::scene::CameraDirector director;
    oj::render::Camera camera;
    const float dt = 1.0f / 60.0f;
    bool switched = false;
    bool amounts = true;
    bool stopped = false;
    int frames = 0;
    // To the second shot's start.
    while (intro.update(dt, camera, false, director) && frames < 100000) {
        ++frames;
        if (director.blendOn) {
            switched = true;
            break;
        }
    }
    // Through its second.
    for (int k = 0; k < 120 && switched; ++k) {
        intro.update(dt, camera, false, director);
        if (!director.blendOn) {
            stopped = intro.fade() == 0.0f && director.blendAmount == 0.0f;
            break;
        }
        amounts = amounts && director.blendAmount == 1.0f - intro.fade();
    }
    check(switched && amounts && stopped,
          "the second shot: the transition on, its amount 1 - the time left, then stopped");
    while (intro.update(dt, camera, false, director) && frames < 1000000) ++frames;
    check(!director.blendOn && director.blendAmount == 0.0f, "the fly-by's end: the transition stopped");
}
