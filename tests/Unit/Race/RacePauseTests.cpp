// Unit tests: the race's pause (Game/Race/Hud/RacePause.h).

#include "Framework/Test.h"

#include "Core/Career/Career.h"
#include "Core/Race/Racer.h"
#include "Core/Tracks/Route.h"
#include "Core/Tracks/Track.h"
#include "Game/Race/Hud/RacePause.h"

#include <memory>

namespace {

using oj::test::check;

}  // namespace

// A sprint restarted from the pause (0x0044EF7D) starts again from its first heat (the heat -1, restart_race counts
// it): the session's racers' points cleared and, the fixed build's, their heats' time (+0x134), which the stock kept
// and which decided the new sprint's ties; a car past the session's racers keeps both.
OJ_UNIT_TEST(RacePause, SprintRestart) {
    const oj::Route route{oj::Spl{}};
    oj::RaceRules rules;
    rules.type = oj::RaceType::Sprint;
    rules.heat = 2;
    oj::RaceState state(route, 1, rules);
    auto career = std::make_unique<oj::Career>();
    career->session.count = 2;
    oj::Competitor first;
    oj::Competitor second;
    oj::Competitor other;
    for (oj::Competitor* c : {&first, &second, &other}) {
        c->points = 6.0f;
        c->totalTime = 95.5f;
    }
    oj::RaceCar a;
    oj::RaceCar b;
    oj::RaceCar c;
    a.competitor = &first;
    a.record = 0;
    b.competitor = &second;
    b.record = 1;
    c.competitor = &other;
    c.record = 2;
    oj::HudRace race;
    race.type = oj::RaceType::Sprint;
    race.race = &state;
    race.career = career.get();
    race.cars = {&a, &b, &c};
    oj::restartSprint(race);
    check(first.points == 0.0f && second.points == 0.0f && state.rules().heat == -1,
          "sprint restart: the session's racers' points cleared, from the first heat");
    check(first.totalTime == 0.0f && second.totalTime == 0.0f,
          "sprint restart (fixed): the session's racers' heats' time cleared too");
    check(other.points == 6.0f && other.totalTime == 95.5f, "sprint restart: a car past the session's racers kept");
}
