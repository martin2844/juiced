// Unit tests: StartingGrid.

#include "Framework/Test.h"

#include "Core/Race/StartingGrid.h"

namespace {

using oj::test::check;
using oj::RaceMode;
using oj::RaceType;

}  // namespace

// Off the network the race's cars go to the grid back to front (0x004975A0) but in a career race: arcade 1-1's six
// cars, the player's first, put the player on the sixth point (the second row); a career race keeps the order.
OJ_UNIT_TEST(StartingGrid, Order) {
    check(oj::startingGridIndex(RaceMode::Arcade, RaceType::Circuit, 0, 6, 0) == 5 &&
              oj::startingGridIndex(RaceMode::Arcade, RaceType::Circuit, 0, 6, 5) == 0,
          "an arcade race: the first car made on the last point, the last on the first");
    check(oj::startingGridIndex(RaceMode::Custom, RaceType::Circuit, 0, 4, 0) == 3 &&
              oj::startingGridIndex(RaceMode::SplitScreen, RaceType::Circuit, 0, 2, 1) == 0,
          "a custom and a split screen race likewise");
    check(oj::startingGridIndex(RaceMode::Career, RaceType::Circuit, 0, 6, 0) == 0 &&
              oj::startingGridIndex(RaceMode::Career, RaceType::Circuit, 0, 6, 4) == 4,
          "a career race: the order made");
}

// A sprint (type 6) moves each car on by its heat within the count (a heat of -1 as 0).
OJ_UNIT_TEST(StartingGrid, SprintHeats) {
    check(oj::startingGridIndex(RaceMode::Arcade, RaceType::Sprint, 1, 6, 0) == 0 &&
              oj::startingGridIndex(RaceMode::Arcade, RaceType::Sprint, 2, 6, 0) == 1 &&
              oj::startingGridIndex(RaceMode::Career, RaceType::Sprint, 2, 4, 3) == 1 &&
              oj::startingGridIndex(RaceMode::Arcade, RaceType::Sprint, -1, 6, 0) == 5,
          "a sprint's heat moves the place on, -1 as 0");
}
