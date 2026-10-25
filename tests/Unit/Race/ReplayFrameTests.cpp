// Unit tests: a replay's frame packed and unpacked (Core/Race/Replay.h).

#include "Framework/Test.h"

#include "Core/Race/Replay.h"

namespace {

using oj::test::check;

}  // namespace

// The throttle's two bits beside the parts' six (+0x1D): the fixed build reads the throttle of its bits alone (the
// stock read the whole byte: with any part knocked off the throttle was 1).
OJ_UNIT_TEST(ReplayFrame, ThrottleBesideParts) {
    oj::ReplayFrame f;
    f.throttle = 0.34f;
    f.parts = 5;
    const oj::PackedFrame packed = oj::packFrame(f);
    const oj::ReplayFrame back = oj::unpackFrame(packed.data());
    check(back.parts == 5 && back.throttle > 0.33f && back.throttle < 0.34f, "a third of the throttle, parts 5");
    f.throttle = 0.0f;
    f.parts = 0x3F;
    const oj::PackedFrame off = oj::packFrame(f);
    check(oj::unpackFrame(off.data()).throttle == 0.0f && oj::unpackFrame(off.data()).parts == 0x3F,
          "no throttle with every part off");
}
