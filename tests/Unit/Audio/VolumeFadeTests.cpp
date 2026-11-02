// Unit tests: the fixed build's fade of the sound world's volume above 60 frames a second (Audio/Voices.h fadeByTime,
// JuicedConfigExt's HighFPSAudioFix).

#include "Framework/Test.h"

#include "Audio/Voices.h"

#include <cmath>

namespace {

using oj::test::check;

// The volume after the seconds given at the frame rate given, from the volume to the target (the world's rates 20
// down and 50 up).
float fade(float volume, float target, double seconds, int fps) {
    const int frames = static_cast<int>(std::lround(seconds * fps));
    for (int i = 0; i < frames; ++i) volume = oj::audio::fadeByTime(volume, target, 20.0f, 50.0f, 1.0f / fps);
    return volume;
}

}  // namespace

// Up at rate 50 every 1/30 s leaves 0.4 of the difference: 0.1 s 0.064 of it at 30, 144 and 240 frames a second.
OJ_UNIT_TEST(VolumeFade, ByTime) {
    const float at30 = fade(0.0f, 1.0f, 0.1, 30);
    check(std::fabs(at30 - (1.0f - 0.064f)) < 1e-4f, "30 frames a second: 0.936");
    check(std::fabs(fade(0.0f, 1.0f, 0.1, 240) - at30) < 1e-4f && std::fabs(fade(0.0f, 1.0f, 0.125, 144) -
                                                                             (1.0f - std::pow(0.4f, 3.75f))) < 1e-4f,
          "240 and 144 frames a second: the same by the time");
}

// No overshoot: down at rate 20 (the stock's step 1.5 of the difference at 30) stays above the target and reaches it;
// near silence is silence; no time no step.
OJ_UNIT_TEST(VolumeFade, NoOvershoot) {
    float v = 1.0f;
    bool above = true;
    for (int i = 0; i < 240; ++i) {
        v = oj::audio::fadeByTime(v, 0.5f, 20.0f, 50.0f, 1.0f / 240.0f);
        above = above && v >= 0.5f;
    }
    check(above && v == 0.5f, "down to 0.5: above it, then on it");
    check(oj::audio::fadeByTime(0.0011f, 0.0f, 20.0f, 50.0f, 1.0f / 60.0f) == 0.0f, "at or under 0.001: silence");
    check(oj::audio::fadeByTime(0.3f, 1.0f, 20.0f, 50.0f, 0.0f) == 0.3f, "dt 0: as it is");
}
