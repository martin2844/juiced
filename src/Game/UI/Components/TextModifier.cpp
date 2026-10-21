// The text modifiers (0x005B14C0, 0x005B14A0).

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

namespace oj::ui {

using namespace detail;

// 0x005B14C0: the frame time held within 0 .. 0.1; +0x24 (the time) runs down by it and +0x28 (the pulse) by 3 x it,
// back to 1 below -1; the time below 0 goes back to 1.5 and the spread (+0x1C) to 0; below 1 the spread is (1 - time
// squared) x the text's length x 7 and +0x20 the time x 0.5; the place (+0x08) moves by the speed (+0x10) backward
// while +0x18 is 0, forward while it is 1, a BOUNCE (type 2) turning past the reach (+0x14) before the text or after
// its end.
void TextModifier::step(float dt, float length) {
    if (0.0f <= dt) {
        if (0.1f < dt) dt = 0.1f;
    } else {
        dt = 0.0f;
    }
    const float t = time - dt;
    time = t;
    pulse = pulse - dt * 3.0f;
    if (pulse < -1.0f) pulse = 1.0f;
    if (t < 0.0f) {
        time = 1.5f;
        spread = 0.0f;
    }
    if (time < 1.0f) {
        spread = (1.0f - time * time) * length * 7.0f;
        alpha = time * 0.5f;
    }
    if (forward == 0) {
        place = place - dt * speed;
        if (place < -reach && type == ModifierType::Bounce) forward = 1;
    } else {
        place = dt * speed + place;
        if (length + reach < place && type == ModifierType::Bounce) forward = 0;
    }
}

// 0x005B14A0: the place back to the start, forward, no spread, +0x20 0 and the time 1.5.
void TextModifier::restart() {
    place = start;
    forward = 1;
    spread = 0.0f;
    alpha = 0.0f;
    time = 1.5f;
}

}  // namespace oj::ui
