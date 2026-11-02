#pragma once
// The fixed build's vibration of the controller (docs/fixes.md Added). The car control update computes the two motors
// of the first player's controller each frame of the race (0x0041F801 .. 0x0041FA89: the heavy one +0x10C, the light
// one +0x110, 0 .. 1) and clears them (0x0041FAA4): the PC has no call that sends them. The Xbox version (its final
// build's 0x00030190) sends them with its vibration option on: SetRumble(heavy, light), each x 65535 (0x0017DD40), the
// heavy one the left, low motor, the light one the right, high one; none in the race's first quarter of a second
// (the car's competitor +0xF0 below 0.25) or past the finish. The fixed build sends them to the controller
// (Game/Platform/Pads) as JuicedConfigExt's plugin does (its Features/XInput/Rumble), with the console's timing at any
// frame rate: the Xbox shows a frame at each refresh at the most (60 Hz), so a source of one frame (a hit, a landing,
// a bump) lasts there 1/60 s at least; here each frame of the game counts for 1/60 s and the strongest of them wins,
// and the motors keep the last frame 100 ms (a pause or the menus stop them).

#include <array>

namespace oj::input {

struct Motors {
    float heavy = 0.0f;  // the left, low motor
    float light = 0.0f;  // the right, high motor
};

class PadRumble {
public:
    static constexpr double kHeld = 100.0;                 // ms the motors keep the last frame of the game
    static constexpr double kConsoleFrame = 1000.0 / 60.0;  // ms: the shortest frame of the Xbox version

    // The steady clock in milliseconds.
    static double now();

    // A frame of the game's motors (each held to 0 .. 1) at the time (ms).
    void frame(double ms, Motors motors);
    // The motors at the time, each times the strength (1 for 100 %) up to 1: the newest frame's, with every frame
    // younger than a console frame (the strongest of each); none once the newest is kHeld old, or without one.
    Motors levels(double ms, float strength) const;
    // The newest frame's motors (none without one).
    Motors newest() const;
    void clear();

private:
    struct Frame {
        double time = 0.0;
        Motors motors;
    };
    static constexpr int kHistory = 64;  // frames of the game within a console frame, at the most
    std::array<Frame, kHistory> m_frames{};
    int m_newest = -1;
};

// The fixed build's light motor of the wheels' slide (the car's +0x418, the mean of the four |sideways slip / speed|
// above 4 m/s). The game's (the Xbox's, 0x00031214) is the slide + 0.2 above 0.05: 25 % at once, an ordinary fast
// turn 0.08 .. 0.11 28 .. 31 %, a slide 0.3 50 % (JuicedConfigExt's logs). Here the same from 0.08 on; below it a
// gentler start (the user, 2026-10-26): the line through 20 % at 0.06 and the Xbox's 28 % at 0.08 (16 % just above
// 0.05); none at 0.05 and below, the whole past 0.8.
float slideLight(float slide);

}  // namespace oj::input
