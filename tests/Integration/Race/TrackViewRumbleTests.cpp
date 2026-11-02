// Integration tests: the fixed build's vibration of the controller from the race (Game/Race/TrackView,
// TrackView::playerRumble): the first player's motors of each frame to Core/Input/PadRumble as the Xbox version sends
// them, with the game's Eclipse.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Input/Input.h"
#include "Core/Input/PadRumble.h"
#include "Core/Physics/CarBody.h"
#include "Core/Race/Racer.h"
#include "Game/Race/TrackView/TrackView.h"

#include <cmath>
#include <memory>
#include <string>

namespace {

using oj::test::check;

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

}  // namespace

// A turn (the slide 0.1) gives the light motor of oj::input::slideLight, a hit the heavy one; none in the race's first
// quarter of a second or past the finish; the second player's frame does not reach the controller; the game's own
// motors cleared after each frame, as the stock's.
OJ_INTEGRATION_TEST(TrackViewRumble, FirstPlayersMotors) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    auto view = std::make_unique<app::TrackView>();
    oj::input::Input input;
    view->input = &input;
    app::RaceSlot slot;
    slot.body = std::make_unique<oj::CarBody>(*d, oj::Vec4{});
    slot.car.kind = oj::CarKind::Player;
    slot.car.body = slot.body.get();
    oj::Competitor competitor;
    competitor.raceTime = 5.0f;
    slot.car.competitor = &competitor;
    app::RacePlayer& player = view->first();
    player.slot = &slot;
    const auto frame = [&](app::RacePlayer& p) {
        view->playerRumble(p, 1.0f / 60.0f, false);
        return input.padRumble().newest();
    };

    slot.car.wheelSlide = 0.1f;
    const oj::input::Motors turn = frame(player);
    check(turn.heavy == 0.0f && near(turn.light, oj::input::slideLight(0.1f)) &&
              input.rumble().large == 0.0f && input.rumble().small == 0.0f,
          "a turn: the light motor " + std::to_string(turn.light) + " (the game's would be 0.30), the game's cleared");
    slot.car.wheelSlide = 0.0f;
    slot.car.impact = 1.0f;
    check(near(frame(player).heavy, 1.0f), "a hit: the heavy motor");
    competitor.raceTime = 0.1f;
    const oj::input::Motors start = frame(player);
    competitor.raceTime = 5.0f;
    player.finished = true;
    const oj::input::Motors finished = frame(player);
    player.finished = false;
    check(start.heavy == 0.0f && finished.heavy == 0.0f, "none in the first quarter of a second, nor past the finish");
    app::RacePlayer second;
    second.index = 1;
    second.slot = &slot;
    slot.car.impact = 0.0f;
    slot.car.wheelSlide = 0.2f;
    const oj::input::Motors before = frame(player);
    slot.car.impact = 1.0f;
    const oj::input::Motors secondFrame = frame(second);
    check(secondFrame.heavy == before.heavy && secondFrame.light == before.light,
          "the second player's frame not sent");
}

// Off the road the game's share of a jolt is the speed / 5 x the frame's time: at 120 frames a second the light motor
// is at least the share of 60 (30 m/s: 0.1, the game's 0.05); not in the steering wheel mode.
OJ_INTEGRATION_TEST(TrackViewRumble, BumpsAtConsoleRate) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    auto view = std::make_unique<app::TrackView>();
    oj::input::Input input;
    view->input = &input;
    app::RaceSlot slot;
    slot.body = std::make_unique<oj::CarBody>(*d, oj::Vec4{});
    slot.car.kind = oj::CarKind::Player;
    slot.car.body = slot.body.get();
    app::RacePlayer& player = view->first();
    player.slot = &slot;
    slot.body->localVelocity = {0.0f, 0.0f, 30.0f, 0.0f};
    slot.car.rumbleShare = 30.0f / 5.0f / 120.0f;
    view->playerRumble(player, 1.0f / 120.0f, false);
    const float pad = input.padRumble().newest().light;
    view->playerRumble(player, 1.0f / 120.0f, true);
    const float wheel = input.padRumble().newest().light;
    check(near(pad, 0.1f) && wheel == 0.0f,
          "the bumps' light motor " + std::to_string(pad) + " (60 frames' share), none in the wheel mode");
}
