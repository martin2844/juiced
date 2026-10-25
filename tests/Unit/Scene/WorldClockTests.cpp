// Unit tests: WorldClock.

#include "Framework/Test.h"

#include "Game/Scene/WorldClock.h"

namespace {

using oj::test::check;
using oj::scene::WorldClock;

}  // namespace

// Running, the world's time is the game's clock; stopped (the race paused) it holds the time last read and the clock's
// stop is kept; started again it goes on from there.
OJ_UNIT_TEST(WorldClock, Pause) {
    WorldClock clock;
    check(clock.read(10.0f) == 10.0f, "running from the start, the game's clock");
    clock.stop(true, 12.0f);
    check(clock.stopped && clock.stoppedAt == 12.0f, "the stop keeps the game's clock");
    check(clock.read(15.0f) == 10.0f, "stopped, the time last read held");
    clock.stop(true, 16.0f);
    check(clock.stoppedAt == 12.0f, "a second stop while stopped changes nothing");
    clock.stop(false, 20.0f);
    check(clock.lead == 8.0f && clock.read(20.0f) == 12.0f && clock.read(21.0f) == 13.0f,
          "started again, the lead the pause's length");
}

// Stock problem: the lead is the last pause's length, not the sum, so after a second pause the world's time jumps on by
// the first. The fixed build adds the pauses up: the time goes on from where it was held.
OJ_UNIT_TEST(WorldClock, SecondPause) {
    WorldClock clock;
    clock.stop(true, 10.0f);
    clock.stop(false, 15.0f);
    check(clock.read(20.0f) == 15.0f, "after the first pause, 5 behind");
    clock.stop(true, 20.0f);
    check(clock.read(25.0f) == 15.0f, "the second pause holds the time");
    clock.stop(false, 23.0f);
    check(clock.lead == 8.0f && clock.read(23.0f) == 15.0f, "the pauses' lengths added: on from the held time");
}
