// Integration tests: the showoff's tricks (Core/Race/Cruise.h) with a car of the game's (ECLIPSE_STOCK's dynamics).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Data/Language.h"
#include "Core/Physics/CarBody.h"
#include "Core/Race/Cruise.h"
#include "Core/Race/Racer.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// High speed held in a zone of its crowd: after 2.5 s the zone's "BONUS MOVE!" line comes (0x004AC560) while the trick
// goes on. The stock's trick kept the list's last place, so its points went on into the bonus line; the fixed build's
// follows its own line up the list: the bonus keeps its worth, the high speed line grows.
OJ_INTEGRATION_TEST(Cruise, TrickKeepsItsLine) {
    const oj::CarDynamics* d = oj::test::eclipseDynamics();
    if (d == nullptr) return;
    oj::CarBody body(*d, {0.0f, 0.0f, 0.0f, 0.0f});
    oj::RaceCar car;
    car.body = &body;
    oj::Cruise cruise;
    cruise.setup(3, oj::RaceMode::Career, oj::RaceType::Cruise);
    oj::CruiseZone zone;
    zone.shape = 1;  // a box round the car
    zone.boxMin = {-1.0e6f, -1.0e6f, -1.0e6f, 0.0f};
    zone.boxMax = {1.0e6f, 1.0e6f, 1.0e6f, 0.0f};
    zone.active = 1;
    zone.kind = oj::kHighSpeed;
    cruise.zones.push_back(zone);
    oj::TextTable texts;
    oj::CruiseRace race;
    race.car = &car;
    race.mode = oj::RaceMode::Career;
    race.type = oj::RaceType::Cruise;
    race.frameTime = 0.05f;
    race.texts = &texts;
    std::vector<std::pair<std::wstring, float>> out;
    const auto line = [&](bool bonus) -> const oj::CruiseMessage* {
        for (const oj::CruiseMessage& m : cruise.messages) {
            if (m.trick == oj::kHighSpeed && (m.bonus != 0) == bonus) return &m;
        }
        return nullptr;
    };
    float bonusWorth = -1.0f;
    float speedAtBonus = -1.0f;
    for (int i = 0; i < 100; ++i) {
        body.setLocalVelocity({0.0f, 0.0f, 60.0f, 0.0f});  // over 55.6 m/s: high speed in any gear
        cruise.frame(race, out);
        const oj::CruiseMessage* bonus = line(true);
        const oj::CruiseMessage* speed = line(false);
        if (bonus != nullptr && bonusWorth < 0.0f && speed != nullptr) {
            bonusWorth = bonus->value;
            speedAtBonus = speed->value;
        }
    }
    const oj::CruiseMessage* bonus = line(true);
    const oj::CruiseMessage* speed = line(false);
    std::printf("     the bonus line %.1f (%.1f when it came), the high speed line %.1f (%.1f then)\n",
                bonus != nullptr ? bonus->value : -1.0f, bonusWorth, speed != nullptr ? speed->value : -1.0f,
                speedAtBonus);
    check(bonus != nullptr && speed != nullptr && bonusWorth > 0.0f, "high speed in its zone: its bonus line came");
    check(bonus != nullptr && bonus->value == bonusWorth, "cruise (fixed): the bonus line keeps its worth");
    check(speed != nullptr && speed->value > speedAtBonus && speed->active != 0,
          "cruise (fixed): the high speed line goes on growing");
}
