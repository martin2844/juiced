// Unit tests: the fixed build's vibration of the controller (Core/Input/PadRumble): the console's timing of the game's
// motors at any frame rate, the hold, the strength, and the light motor of the wheels' slide.

#include "Framework/Test.h"

#include "Core/Input/PadRumble.h"

#include <cmath>
#include <string>

namespace {

using oj::test::check;
using oj::input::Motors;
using oj::input::PadRumble;

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

}  // namespace

// At 60 frames a second the motors are the newest frame's; at 120 a hit of one frame (8 ms) lasts a console frame
// (16.7 ms) and is gone after it; the newest frame is kept 100 ms, none after; none before any frame.
OJ_UNIT_TEST(PadRumble, ConsoleFrames) {
    PadRumble r;
    check(r.levels(0.0, 1.0f).heavy == 0.0f && r.levels(0.0, 1.0f).light == 0.0f, "no frame: still");
    const double frame60 = 1000.0 / 60.0;
    r.frame(1000.0, {1.0f, 0.3f});
    r.frame(1000.0 + frame60, {0.0f, 0.3f});
    const Motors at60 = r.levels(1000.0 + frame60 + 1.0, 1.0f);
    check(at60.heavy == 0.0f && near(at60.light, 0.3f), "60 frames a second: the newest frame's (the hit gone)");
    r.clear();
    r.frame(2000.0, {1.0f, 0.0f});  // a hit
    r.frame(2008.3, {0.0f, 0.2f});
    const Motors within = r.levels(2010.0, 1.0f);
    r.frame(2016.6, {0.0f, 0.2f});
    const Motors after = r.levels(2017.0, 1.0f);
    check(near(within.heavy, 1.0f) && near(within.light, 0.2f),
          "120 frames a second: the hit of one frame kept within a console frame");
    check(after.heavy == 0.0f && near(after.light, 0.2f), "past the console frame: the hit gone");
    check(near(r.levels(2016.6 + 99.0, 1.0f).light, 0.2f) && r.levels(2016.6 + 100.0, 1.0f).light == 0.0f,
          "the newest frame kept 100 ms, then still");
}

// The strength scales both motors up to 1; each frame's motors held to 0 .. 1.
OJ_UNIT_TEST(PadRumble, Strength) {
    PadRumble r;
    r.frame(0.0, {0.4f, 0.8f});
    const Motors half = r.levels(1.0, 0.5f);
    const Motors twice = r.levels(1.0, 2.0f);
    check(near(half.heavy, 0.2f) && near(half.light, 0.4f), "50 %: half");
    check(near(twice.heavy, 0.8f) && near(twice.light, 1.0f), "200 %: twice, up to 1");
    r.frame(2.0, {-0.5f, 3.0f});
    check(r.newest().heavy == 0.0f && r.newest().light == 1.0f, "a frame's motors held to 0 .. 1");
}

// The light motor of the slide: none at 0.05 and below, 20 % at 0.06, from 0.08 the Xbox's slide + 0.2 (an ordinary
// fast turn 0.08 .. 0.11 28 .. 31 %, a slide of 0.3 50 %), the whole past 0.8.
OJ_UNIT_TEST(PadRumble, SlideLight) {
    using oj::input::slideLight;
    check(slideLight(0.0f) == 0.0f && slideLight(0.05f) == 0.0f && near(slideLight(0.0501f), 0.1604f),
          "none to 0.05, 16 % just above it");
    check(near(slideLight(0.06f), 0.2f) && near(slideLight(0.07f), 0.24f), "20 % at 0.06, 24 % at 0.07");
    const float turn = slideLight(0.08f), hardTurn = slideLight(0.11f), slide = slideLight(0.3f);
    check(near(turn, 0.28f) && near(hardTurn, 0.31f) && near(slide, 0.5f) && near(slideLight(0.4f), 0.6f),
          "as the Xbox from 0.08: an ordinary turn " + std::to_string(turn) + " .. " + std::to_string(hardTurn) +
              ", a slide " + std::to_string(slide));
    check(slideLight(0.9f) == 1.0f, "the whole past 0.8");
    bool rising = true;
    for (float s = 0.06f; s < 0.8f; s += 0.01f) rising = rising && slideLight(s + 0.01f) > slideLight(s);
    check(rising && near(slideLight(0.0799f), slideLight(0.08f)), "growing all the way, no step at 0.08");
}
