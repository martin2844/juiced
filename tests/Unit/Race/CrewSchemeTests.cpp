// Unit tests: a crew's scheme on its cars (Core/Race/RaceSession.h, 0x004A9080).

#include "Framework/Test.h"

#include "Core/Career/Career.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Race/RaceSession.h"

#include <cstring>
#include <memory>

namespace {

using oj::test::check;

}  // namespace

// The new career's crews keep four whites, the fourth over the metallic amount (0x80FFFFFF, about -0), and no pearl
// amount; the stock wrote them over the scheme's 0.5 and 0.1, the fixed build keeps those.
OJ_UNIT_TEST(CrewScheme, PaintAmounts) {
    auto career = std::make_unique<oj::Career>();
    career->opponentCars.resize(1);
    oj::Crew crew;
    crew.id = 2;
    crew.baseColour = 0x80FFFFFF;
    crew.metallicColour = 0x80FFFFFF;
    crew.pearlColour = 0x80FFFFFF;
    const uint32_t white = 0x80FFFFFF;
    std::memcpy(&crew.metallicAmount, &white, 4);
    const oj::CarDefs defs;
    const oj::PartCatalogue catalogue;
    oj::applyCrewScheme(*career, crew, defs, catalogue);
    const oj::Car& car = career->opponentCars[0];
    check(car.base == 0x80FFFFFF && car.pearl == 0x80FFFFFF && oj::Car::asFloat(car.metallicAmount) == 0.5f &&
              oj::Car::asFloat(car.pearlAmount) == 0.1f,
          "the crew's colours, the scheme's amounts 0.5 and 0.1");
}
