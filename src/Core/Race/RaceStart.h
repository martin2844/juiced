#pragma once
// The count before a race's start (the race state's +0x38 / +0x3C; 0x004A0220 at the end of the racers' frame;
// docs/spec/17-race-camera.md, "The start").
// Unused: the game's count is RaceState's (RaceState::update, Core/Race/Racer.h), with the competitors' frame and the
// start; this early port of the count alone is kept as it was, called by nothing (its tests are RaceState's now).

namespace oj {

class RaceStart {
public:
    // What a frame of the count did: a whole second passed (the airhorn at 44000 Hz), the start (55000 Hz).
    struct Events {
        bool beep = false;
        bool go = false;
    };

    // 0x004A0220: while above 0 the count goes down by dt; going below 0 the race starts (Stock problem: landing on
    // exactly 0 it never does); each whole second it goes below (from 3) sounds.
    Events update(float dt);

    // While not 0 the players' cars are held by the handbrake (0x0041E200); from 10 up the drivers' controls are
    // not read.
    float count = 0.0f;  // +0x38: 99 while the fly-by plays, 6 from the camera's mode 10
    int second = 3;      // +0x3C: the next whole second that sounds
};

}  // namespace oj
