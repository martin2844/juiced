#include "Core/Race/RaceStart.h"

namespace oj {

RaceStart::Events RaceStart::update(float dt) {
    Events e;
    if (!(count > 0.0f)) return e;
    count = count - dt;
    // Stock problem: a count that lands on exactly 0 never starts the race.
    if (count < 0.0f) {
        count = 0.0f;
        e.go = true;
    }
    if (count < static_cast<float>(second)) {
        e.beep = true;
        second = second - 1;
    }
    return e;
}

}  // namespace oj
