// Unit tests: a sound set's voices (Audio/Bank.h): the quietest voice taken from another instance (0x005C1B60) and
// the voices' bits.

#include "Framework/Test.h"

#include "Audio/Bank.h"

#include <initializer_list>

namespace {

using oj::test::check;

// The quietest voice's steal over a set of the volumes given (voice 0 first): 1 and the voice, or 4.
int quietest(std::initializer_list<float> volumes, uint32_t& voice) {
    oj::audio::SoundSet set;
    set.setInstances(static_cast<uint32_t>(volumes.size()));
    uint32_t i = 0;
    for (float v : volumes) set.setVolume(i++, v);
    voice = 0xFFFFFFFF;
    return set.stealQuietest(voice);
}

}  // namespace

// Below four voices the quietest is taken unless it is as quiet as their mean.
OJ_UNIT_TEST(SoundSet, QuietestFewVoices) {
    uint32_t voice = 0;
    check(quietest({0.6f, 0.2f, 0.4f}, voice) == 1 && voice == 1, "three voices: the quietest, voice 1");
    check(quietest({0.3f, 0.3f}, voice) == 4, "two as quiet as each other: none");
    check(quietest({0.3f}, voice) == 4, "one voice: none (its mean)");
}

// Four voices or more sum as the stock's unrolled loop does: the last 4 x (n / 4) into the 1 / n slot, the first
// n % 4 into the sum, the quietest compared with their product (Stock problem: not the mean).
OJ_UNIT_TEST(SoundSet, QuietestUnrolled) {
    uint32_t voice = 0;
    check(quietest({0.3f, 0.3f, 0.3f, 0.3f}, voice) == 1 && voice == 3,
          "four as quiet as each other: the sum stays 0, so the last of them is taken");
    check(quietest({0.0f, 0.0f, 0.0f, 0.0f}, voice) == 4, "four silent: none (0 against 0)");
    check(quietest({0.0f, 1.0f, 1.0f, 1.0f, 1.0f}, voice) == 4,
          "five, the first silent and the rest full: 0 against (1/5 + 4) x 0, none");
    check(quietest({0.1f, 0.5f, 0.5f, 0.5f, 0.5f}, voice) == 1 && voice == 0,
          "five, the first quiet: 0.1 against (1/5 + 2) x 0.1, the first taken");
}

// The x86's shifts take their count modulo 32: a 33rd voice's bit is the first's.
OJ_UNIT_TEST(SoundSet, VoiceBits) {
    check(oj::audio::SoundSet::bit(0) == 1u && oj::audio::SoundSet::bit(31) == 0x80000000u &&
              oj::audio::SoundSet::bit(32) == 1u,
          "voices 0, 31 and 32's bits");
}
