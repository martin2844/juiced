#pragma once
// What a race is (the game object's +0x6C, Core/Race/RaceSession.h RaceSetup): its mode, how it was started, and its
// type, what is raced. The structs that read the race (the racers' rules, the HUD's, the replay's, the cruise's, the
// camera's) take them as they are.

#include <cstdint>

namespace oj {

// [G+0x6C]+0x00.
enum class RaceMode : int32_t {
    Arcade = 0,       // an arcade championship's race
    Custom = 1,       // a custom race
    Career = 2,       // the career's race (the test track's too)
    SplitScreen = 3,  // two players on one screen (link-up's "split", or more than one player asked for)
    Attract = 4,      // the title's attract replay
};

// [G+0x6C]+0x04. A career's round asks for Showoff (its type 2) as Cruise, PointToPoint (3), Circuit (4) and
// Sprint (5); its crew challenges for TimedLap, PerfectLap, Speed and Cruise (a showoff).
enum class RaceType : int32_t {
    TimedLap = 0,      // a challenge: a lap under a time
    PerfectLap = 1,    // a challenge: a lap without a fault
    Speed = 2,         // a challenge: a speed to reach
    Solo = 3,          // a custom race alone
    Cruise = 4,        // a showoff
    Circuit = 5,       // a race of laps
    Sprint = 6,        // heats
    PointToPoint = 7,  // a race from one point to another
    Unused8 = 8,       // a race to the HUD (hud_race); nothing sets it
    TestTrack = 9,     // the test track (the career's CMSCutScene window and its drive)
};

// A challenge's outcome ([G+0x6C]+0x7C): none yet, met (or not failed so far: a challenge race's setup), or why it
// failed (the game's lines kTextWellDone + 2, + 1, + 3, + 4: OUT OF TIME, CAR DAMAGED, YOU ARE TOO SLOW, TARGET SPEED
// NOT REACHED).
enum class ChallengeOutcome : int32_t {
    None = 0,
    Met = 1,
    OutOfTime = 2,        // a lap over the time
    CarDamaged = 3,       // a hit in a perfect lap
    TooSlow = 4,          // under the speed again after the trap
    SpeedNotReached = 5,  // 10 s without the speed
};

}  // namespace oj
