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

// Four voices or more: the stock's unrolled loop compared the quietest with a product that was not the mean (0 for
// a multiple of four); the fixed build compares it with the mean as below four.
OJ_UNIT_TEST(SoundSet, QuietestManyVoices) {
    uint32_t voice = 0;
    check(quietest({0.3f, 0.3f, 0.3f, 0.3f}, voice) == 4, "four as quiet as each other: none (their mean)");
    check(quietest({0.0f, 0.0f, 0.0f, 0.0f}, voice) == 4, "four silent: none");
    check(quietest({0.0f, 1.0f, 1.0f, 1.0f, 1.0f}, voice) == 1 && voice == 0,
          "five, the first silent and the rest full: the first taken");
    check(quietest({0.5f, 0.5f, 0.1f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}, voice) == 1 && voice == 2,
          "eight, the third quiet: the third taken");
}

// The x86's shifts take their count modulo 32: a 33rd voice's bit is the first's.
OJ_UNIT_TEST(SoundSet, VoiceBits) {
    check(oj::audio::SoundSet::bit(0) == 1u && oj::audio::SoundSet::bit(31) == 0x80000000u &&
              oj::audio::SoundSet::bit(32) == 1u,
          "voices 0, 31 and 32's bits");
}
