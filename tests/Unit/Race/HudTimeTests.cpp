// Unit tests: the fixed build's time text (Game/Race/Hud/HudRace.h, 0x0044FBD0).

#include "Framework/Test.h"

#include "Game/Race/Hud/HudRace.h"

namespace {

using oj::test::check;

}  // namespace

// The stock's hundredths are the fraction x 99 (00 .. 98); the fixed build's x 100 (00 .. 99).
OJ_UNIT_TEST(HudTime, HundredthsUpTo99) {
    check(oj::hudTime(61.995f) == "01:01:99", "time (fixed): 61.995 s as 01:01:99");
    check(oj::hudTime(0.5f) == "00:00:50", "time (fixed): half a second as 50 hundredths");
    check(oj::hudTime(-3.0f) == "00:00:00", "time: below 0 as 0");
}
