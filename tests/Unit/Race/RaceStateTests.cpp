// Unit tests: RaceState.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Data/ExeTables.h"
#include "Core/Race/Racer.h"
#include "Core/Tracks/Route.h"
#include "Core/Tracks/Track.h"

namespace {

using oj::test::check;

}  // namespace

// The race state's reset (0x0049FDF0): the HUD's finishers told (+0x64) back to 0 (0x0049FF02), so a
// restarted race tells no finisher it has not got.
OJ_UNIT_TEST(RaceState, Reset) {
    const oj::Route route{oj::Spl{}};
    oj::RaceRules rules;
    rules.type = oj::RaceType::Circuit;
    oj::RaceState race(route, 3, rules);
    oj::CrtRandom random;
    race.finishersShown = 2;
    race.finishers.clear();
    race.reset(random, 0.0f);
    check(race.finishersShown == 0 && race.finishers.empty() && race.eventHold == 5.0f,
          "race state's reset: the finishers the HUD told back to 0");
}
