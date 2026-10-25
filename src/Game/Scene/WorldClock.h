#pragma once
// The world's clock (the scene base 0x00530D70: +0x08 the game's clock, +0x10 its time, +0x18 stopped, +0x1C the game's
// clock as it stopped, +0x20 the lead).

namespace oj::scene {

// The world's time is the game's clock less the lead, held while stopped (taken again only when read running). The
// race's pause stops it (0x004263E0's end) and its end starts it again, the lead then the game's clock less the stop's.
// Stock problem: the last stop's length only, not the sum, so after a second pause the world's time jumps on by the
// pauses before it. The fixed build adds each stop's length to the lead. The main pass (0x00547690), the cubes
// (0x00547D50) and the particles (0x00503D10) read it.
struct WorldClock {
    float time = 0.0f;       // +0x10
    bool stopped = false;    // +0x18
    float stoppedAt = 0.0f;  // +0x1C
    float lead = 0.0f;       // +0x20

    void stop(bool stop, float clock) {
        if (stop == stopped) return;
        stopped = stop;
        if (stop) {
            stoppedAt = clock;
        } else {
            lead += clock - stoppedAt;
        }
    }

    float read(float clock) {
        if (!stopped) time = clock - lead;
        return time;
    }
};

}  // namespace oj::scene
