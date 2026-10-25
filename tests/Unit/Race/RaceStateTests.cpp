// Unit tests: RaceState.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Data/ExeTables.h"
#include "Core/Race/RaceKinds.h"
#include "Core/Race/Racer.h"
#include "Core/Tracks/Route.h"
#include "Core/Tracks/Track.h"

#include <string>

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

// The count before the start (0x004A0220): from 6 s in steps of 0.7 s the seconds 3, 2, 1 sound, then the start; in
// steps of 0.25 s it lands on 0 (the stock's start never came: the fixed build starts there).
OJ_UNIT_TEST(RaceState, Count) {
    const oj::Route route{oj::Spl{}};
    oj::RaceRules rules;
    rules.type = oj::RaceType::Circuit;
    auto count = [&](float step) {
        oj::RaceState race(route, 3, rules);
        oj::CrtRandom random;
        race.count = 6.0f;
        race.second = 3;
        std::string heard;
        for (int i = 0; i < 40; ++i) {
            const oj::RaceState::Events e = race.update(step, step, random);
            if (e.beep) heard += std::to_string(race.second + 1);
            if (e.go) heard += "go";
        }
        return race.count == 0.0f ? heard : heard + "?";
    };
    check(count(0.7f) == "321go" && count(0.25f) == "321go",
          "race state (fixed): the count's seconds and the start, also landing on 0");
}

// 0x00401E80: a race's first view. A solo race and a cruise start in the kept view, on the network too (they played
// the fly-by, and skipping it brought the black over the start); a sprint's later heat with the sweep and the count at
// 8; the other races with the fly-by off the network and the sweep on it.
OJ_UNIT_TEST(RaceState, FirstView) {
    using oj::RaceType;
    using oj::StartView;
    using oj::startView;
    check(startView(RaceType::Solo, 0, false) == StartView::Kept &&
              startView(RaceType::Cruise, 0, false) == StartView::Kept &&
              startView(RaceType::Solo, 0, true) == StartView::Kept &&
              startView(RaceType::Cruise, 0, true) == StartView::Kept,
          "a solo race and a cruise: the kept view, no fly-by");
    check(startView(RaceType::Sprint, 1, false) == StartView::LaterHeat &&
              startView(RaceType::Sprint, 0, false) == StartView::FlyBy,
          "a sprint: the fly-by for its first heat, the sweep with the count 8 for a later one");
    check(startView(RaceType::Circuit, 0, false) == StartView::FlyBy &&
              startView(RaceType::TimedLap, 0, false) == StartView::FlyBy &&
              startView(RaceType::TestTrack, 0, false) == StartView::FlyBy &&
              startView(RaceType::Circuit, 0, true) == StartView::Sweep,
          "the other races: the fly-by off the network, the sweep on it");
}
